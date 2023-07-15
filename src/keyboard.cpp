#include "keyboard.h"
#include "net.h"
#include "utils.h"

// KEYS SETTINGS

// Key offset, lowest key is midi note 21
static const uint8_t KEY_OFFSET = 21;

// microseconds between trigger_0 and trigger_1 for maximum velocity
// increase value to reach maximum velocity easier
static const uint32_t NOTEON_TIME_MIN = 3000;

// microseconds between trigger_0 and trigger_1 for minimum velocity
// decrease value to reach minimum velocity faster
static const uint32_t NOTEON_TIME_MAX = 120000;

// precalculate timing range
static const float NOTEON_TIME_RANGE = (float)(NOTEON_TIME_MAX - NOTEON_TIME_MIN);

// Curve for the noteon velocity. Change only the value in the middle. pow(1 - 0.65f, 3)
static const float NOTEON_VELOCITY_BIAS = 0.1f;

// microseconds between trigger_0 and trigger_1 for maximum velocity
// increase value to reach maximum velocity easier
static const uint32_t NOTEOFF_TIME_MIN = 4000;

// microseconds between trigger_0 and trigger_1 for minimum velocity
// decrease value to reach minimum velocity faster
static const uint32_t NOTEOFF_TIME_MAX = 80000;

// precalculate timing range
static const float NOTEOFF_TIME_RANGE = (float)(NOTEOFF_TIME_MAX - NOTEOFF_TIME_MIN);

// delay after digitalWrite() during scan_matrix()
static const uint32_t TIME_DELAY_SCANMATRIX = 8;

static const uint32_t MIDI_CHANNEL = 1;
// Number of keys
static const uint8_t KEY_COUNT = 88;

// All possible states of a key
enum keystate { UP,
    GOING_DOWN,
    DOWN,
    GOING_UP };

typedef struct {
    // time between switch activations
    uint32_t time = 0;

    // debounce-times
    uint32_t debounce_0 = 0;
    uint32_t debounce_1 = 0;

    keystate state = UP;

    // BR-Line, upper switches
    bool trigger_0 = false;
    // MK-Line, lower switches
    bool trigger_1 = false;

    // previous value of trigger_0
    bool prev_trigger_0 = false;
    // previous value of trigger_1
    bool prev_trigger_1 = false;
} keyboard_key_t;

static const uint8_t COUNT_T = 8;
static const uint8_t COUNT_MKBR_LOW = 5;
static const uint8_t T_LOW[COUNT_T] = { 34, 36, 38, 40, 14, 16, 18, 20 };
static const uint8_t MK_LOW[COUNT_MKBR_LOW] = { 33, 37, 41, 17, 21 };
static const uint8_t BR_LOW[COUNT_MKBR_LOW] = { 35, 39, 15, 19, 22 };

static const uint8_t COUNT_MKBR_HIGH = 6;
static const uint8_t T_HIGH[COUNT_T] = { 3, 5, 7, 9, 11, 24, 26, 28 };
static const uint8_t MK_HIGH[COUNT_MKBR_HIGH] = { 2, 6, 10, 25, 29, 32 };
static const uint8_t BR_HIGH[COUNT_MKBR_HIGH] = { 4, 8, 12, 27, 31, 30 };

static keyboard_key_t keys[KEY_COUNT];

void setupKeyboard()
{
    for (int i = 0; i < COUNT_T; i++) {
        pinMode(T_LOW[i], OUTPUT);
        digitalWrite(T_LOW[i], LOW);
        pinMode(T_HIGH[i], OUTPUT);
        digitalWrite(T_HIGH[i], LOW);
    }

    for (int i = 0; i < COUNT_MKBR_LOW; i++) {
        pinMode(MK_LOW[i], INPUT_PULLDOWN);
        pinMode(BR_LOW[i], INPUT_PULLDOWN);
    }
    for (int i = 0; i < COUNT_MKBR_HIGH; i++) {
        pinMode(MK_HIGH[i], INPUT_PULLDOWN);
        pinMode(BR_HIGH[i], INPUT_PULLDOWN);
    }
}

void saveTimeDifference(int key, uint32_t min, uint32_t max, uint32_t current_time, uint32_t last_time)
{
    // Handle rollover of timer
    if (current_time < last_time) {
        keys[key].time = constrain(UINT32_MAX - keys[key].time + current_time, min, max) - min;
    } else {
        keys[key].time = constrain(current_time - keys[key].time, min, max) - min;
    }
}

float calculateMidiNoteOnVelocity(uint32_t time)
{
    float x = (NOTEON_TIME_RANGE - (float)time) / NOTEON_TIME_RANGE;

    float midi_velocity = x * NOTEON_VELOCITY_BIAS / (x * NOTEON_VELOCITY_BIAS - x + 1.0f);

    return midi_velocity;
}

float calculateMidiNoteOffVelocity(uint32_t time)
{
    float midi_velocity = (NOTEOFF_TIME_RANGE - (float)time) / NOTEOFF_TIME_RANGE;

    return midi_velocity;
}

void processMatrix()
{
    for (uint8_t T_LINE = 0; T_LINE < COUNT_T; T_LINE++) {
        // Set T-Lines HIGH
        digitalWrite(T_LOW[T_LINE], HIGH);
        digitalWrite(T_HIGH[T_LINE], HIGH);
        delayMicroseconds(TIME_DELAY_SCANMATRIX);

        for (uint8_t block = 0; block < COUNT_MKBR_LOW; block++) {
            // Calculate current key
            uint8_t currentKey = block * 8 + T_LINE;

            // Read lower matrix
            keys[currentKey].trigger_0 = digitalRead(BR_LOW[block]);
            // Read BR-Line
            keys[currentKey].trigger_1 = digitalRead(MK_LOW[block]);
        }
        for (uint8_t block = 0; block < COUNT_MKBR_HIGH; block++) {
            // Calculate current key
            uint8_t currentKey = block * 8 + T_LINE + 40;

            // Read upper matrix
            keys[currentKey].trigger_0 = digitalRead(BR_HIGH[block]);
            // Read BR-Line
            keys[currentKey].trigger_1 = digitalRead(MK_HIGH[block]);
        }

        digitalWrite(T_LOW[T_LINE], LOW);
        digitalWrite(T_HIGH[T_LINE], LOW);
    }
}

void loopKeyboard(uint32_t current_time, uint32_t last_time)
{
    processMatrix();

    for (int key = 0; key < KEY_COUNT; key++) {
        // Debounce and process keys
        if (keys[key].trigger_1 && !keys[key].prev_trigger_1 && keys[key].state == GOING_DOWN) {
            if (keys[key].debounce_1 == 0) {
                // trigger_1 is pressed, key is now down
                keys[key].prev_trigger_1 = true;
                keys[key].debounce_1 = current_time;
                keys[key].state = DOWN;

                saveTimeDifference(key, NOTEON_TIME_MIN, NOTEON_TIME_MAX, current_time, last_time);

                float velocity = calculateMidiNoteOnVelocity(keys[key].time);

                sendNoteOn(key + KEY_OFFSET, velocity);
            }
        }
        if (!keys[key].trigger_1 && keys[key].prev_trigger_1 && keys[key].state == DOWN) {
            if (keys[key].debounce_1 == 0) {
                // trigger_1 is released, key is going up
                keys[key].prev_trigger_1 = false;
                keys[key].debounce_1 = current_time;
                keys[key].state = GOING_UP;

                keys[key].time = current_time;
            }
        }

        if (keys[key].trigger_0 && !keys[key].prev_trigger_0 && keys[key].state == UP) {
            if (keys[key].debounce_0 == 0) {
                // trigger_1 is pressed, key is going down
                keys[key].prev_trigger_0 = true;
                keys[key].debounce_0 = current_time;
                keys[key].state = GOING_DOWN;

                keys[key].time = current_time;
            }
        }
        if (!keys[key].trigger_0 && keys[key].prev_trigger_0 && keys[key].state == GOING_UP) {
            if (keys[key].debounce_0 == 0) {
                // trigger_0 is released, key is now up
                keys[key].prev_trigger_0 = false;
                keys[key].debounce_0 = current_time;

                keys[key].state = UP;

                saveTimeDifference(key, NOTEOFF_TIME_MIN, NOTEOFF_TIME_MAX, current_time, last_time);

                float velocity = calculateMidiNoteOffVelocity(keys[key].time);

                sendNoteOff(key + KEY_OFFSET, velocity);
            }
        }

        if (keys[key].debounce_0 > 0 && debounce(keys[key].debounce_0, current_time)) {
            keys[key].debounce_0 = 0;
        }
        if (keys[key].debounce_1 > 0 && debounce(keys[key].debounce_1, current_time)) {
            keys[key].debounce_1 = 0;
        }

        // Handling keys that go back up after the first trigger
        if (!keys[key].trigger_0 && keys[key].state == GOING_DOWN) {
            if (keys[key].debounce_0 == 0) {
                // trigger_0 is released, key is back up
                keys[key].prev_trigger_0 = false;
                keys[key].state = UP;
            }
        }

        // Handling keys that go back down after releasing second trigger
        if (keys[key].trigger_1 && keys[key].state == GOING_UP) {
            if (keys[key].debounce_1 == 0) {
                // trigger_1 is pressed, key is back down
                keys[key].prev_trigger_1 = true;
                keys[key].state = DOWN;
            }
        }
    }
}
