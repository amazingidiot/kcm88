#include <Arduino.h>
#include <LiteOSCParser.h>
#include <QNEthernet.h>

// #define DEBUG

// ETHERNET

namespace net = qindesign::network;
namespace osc = qindesign::osc;

// Buffer for received UDP messages
static uint8_t buf[net::Ethernet.mtu() - 20 - 8];
static const uint8_t queueSize = 8;

static osc::LiteOSCParser parser;
static osc::LiteOSCParser log_parser;
static osc::OSCBundle bundle;

net::EthernetUDP server(queueSize);

// Local IP settings
static IPAddress local_ip(192, 168, 100, 10);
static IPAddress local_subnet(255, 255, 255, 0);
static uint16_t local_port = 8000;

// Remote IP settings
static IPAddress remote_ip(192, 168, 100, 1);
static uint16_t remote_port = 8000;

// TIME

#ifndef DEBUG
static const uint32_t time_between_loops = 500;
#else
static const uint32_t time_between_loops = 1000000; // in microseconds
#endif
static uint32_t last_matrix_update_time = 0;

// Max possible value for time measurement
// https://www.pjrc.com/teensy/td_timing_millis.html
static const uint32_t TIME_MAX = 4294967295;

// Value for debouncing in microseconds
static const uint32_t TIME_DEBOUNCE = 3000;

// store current time every loop for velocity calculations
static uint32_t current_time = 0;
// time of last loop, needed for timer rollover
static uint32_t last_time = 0;

// KEYS SETTINGS

// Key offset, lowest key is midi note 21
static const uint8_t KEY_OFFSET = 21;

// microseconds between trigger_0 and trigger_1 for maximum velocity
// increase value to reach maximum velocity easier
static const uint32_t NOTEON_TIME_MIN = 4000;

// microseconds between trigger_0 and trigger_1 for minimum velocity
// decrease value to reach minimum velocity faster
static const uint32_t NOTEON_TIME_MAX = 120000;

// precalculate timing range
static const float NOTEON_TIME_RANGE = (float)(NOTEON_TIME_MAX - NOTEON_TIME_MIN);

// Curve for the noteon velocity. Change only the value in the middle. pow(1 - 0.65f, 3)
static const float NOTEON_VELOCITY_BIAS = 0.042875f;

// microseconds between trigger_0 and trigger_1 for maximum velocity
// increase value to reach maximum velocity easier
static const uint32_t NOTEOFF_TIME_MIN = 4000;

// microseconds between trigger_0 and trigger_1 for minimum velocity
// decrease value to reach minimum velocity faster
static const uint32_t NOTEOFF_TIME_MAX = 80000;

// precalculate timing range
static const float NOTEOFF_TIME_RANGE = (float)(NOTEOFF_TIME_MAX - NOTEOFF_TIME_MIN);

// delay after digitalWrite() during scan_matrix()
static const uint32_t TIME_DELAY_SCANMATRIX = 5;

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

// Functions

void save_key_time(int key) { keys[key].time = current_time; }

void save_time_difference(int key, uint32_t min, uint32_t max)
{
    // Handle rollover of timer
    if (current_time < last_time) {
        keys[key].time = constrain(TIME_MAX - keys[key].time + current_time, min, max) - min;
    } else {
        keys[key].time = constrain(current_time - keys[key].time, min, max) - min;
    }
}

float calculate_midi_noteon_velocity(uint32_t time)
{
    float x = (NOTEON_TIME_RANGE - (float)time) / NOTEON_TIME_RANGE;

    float midi_velocity = x * NOTEON_VELOCITY_BIAS / (x * NOTEON_VELOCITY_BIAS - x + 1);

    return midi_velocity;
}

float calculate_midi_noteoff_velocity(uint32_t time)
{
    float midi_velocity = (NOTEOFF_TIME_RANGE - (float)time) / NOTEOFF_TIME_RANGE;

    return midi_velocity;
}

void processKeyboard()
{
    // Keyboard
    uint32_t current_debounce = current_time - TIME_DEBOUNCE;

#ifdef DEBUG
    parser.init("/debug");
    parser.addInt(0);
#endif

    for (uint8_t T_LINE = 0; T_LINE < COUNT_T; T_LINE++) {
        // Set T-Lines HIGH
        digitalWrite(T_LOW[T_LINE], HIGH);
        digitalWrite(T_HIGH[T_LINE], HIGH);
        delayMicroseconds(TIME_DELAY_SCANMATRIX);

        for (uint8_t block = 0; block < COUNT_MKBR_LOW; block++) {
            // Calculate current key
            uint8_t currentKey = block * 8 + T_LINE;

            // Read lower matrix
            keys[currentKey].trigger_0 = digitalReadFast(BR_LOW[block]);
            // Read BR-Line
            keys[currentKey].trigger_1 = digitalReadFast(MK_LOW[block]);
#ifdef DEBUG
            if (keys[currentKey].trigger_0 || keys[currentKey].trigger_1) {
                parser.addInt(block);
                parser.addInt(currentKey);
                parser.addBoolean(keys[currentKey].trigger_0);
                parser.addBoolean(keys[currentKey].trigger_1);
            }
#endif
        }
        for (uint8_t block = 0; block < COUNT_MKBR_HIGH; block++) {
            // Calculate current key
            uint8_t currentKey = block * 8 + T_LINE + 40;

            // Read upper matrix
            keys[currentKey].trigger_0 = digitalReadFast(BR_HIGH[block]);
            // Read BR-Line
            keys[currentKey].trigger_1 = digitalReadFast(MK_HIGH[block]);
#ifdef DEBUG
            if (keys[currentKey].trigger_0 || keys[currentKey].trigger_1) {
                parser.addInt(block);
                parser.addInt(currentKey);
                parser.addBoolean(keys[currentKey].trigger_0);
                parser.addBoolean(keys[currentKey].trigger_1);
            }
#endif
        }

        digitalWrite(T_LOW[T_LINE], LOW);
        digitalWrite(T_HIGH[T_LINE], LOW);
    }
#ifdef DEBUG
    server.send(remote_ip, remote_port, parser.getMessageBuf(), parser.getMessageSize());
    return;
#endif

    for (int key = 0; key < KEY_COUNT; key++) {
        // Debounce and process keys

        if (keys[key].debounce_0 < current_debounce) {
            if (keys[key].trigger_0 && !keys[key].prev_trigger_0 && keys[key].state == UP) {
                // trigger_0 is pressed, key is going down
                keys[key].prev_trigger_0 = true;
                keys[key].debounce_0 = current_time;
                keys[key].state = GOING_DOWN;

                save_key_time(key);
            } else if (!keys[key].trigger_0 && keys[key].prev_trigger_0 && keys[key].state == GOING_UP) {
                // trigger_0 is released, key is now up
                keys[key].prev_trigger_0 = false;
                keys[key].debounce_0 = current_time;

                keys[key].state = UP;

                save_time_difference(key, NOTEOFF_TIME_MIN, NOTEOFF_TIME_MAX);

                float velocity = calculate_midi_noteoff_velocity(keys[key].time);

                parser.init("/noteoff");
                parser.addInt(key + KEY_OFFSET);
                parser.addInt( (int32_t)(126.0f * velocity) + 1);

                server.send(remote_ip, remote_port, parser.getMessageBuf(), parser.getMessageSize());
            }
        }

        if (keys[key].debounce_1 < current_debounce) {
            if (keys[key].trigger_1 && !keys[key].prev_trigger_1 && keys[key].state == GOING_DOWN) {
                // trigger_1 is pressed, key is now down
                keys[key].prev_trigger_1 = true;
                keys[key].debounce_1 = current_time;
                keys[key].state = DOWN;

                save_time_difference(key, NOTEON_TIME_MIN, NOTEON_TIME_MAX);

                float velocity = calculate_midi_noteon_velocity(keys[key].time);

                parser.init("/noteon");
                parser.addInt(key + KEY_OFFSET);
                parser.addInt((int32_t)(126.0f * velocity) + 1);

                server.send(remote_ip, remote_port, parser.getMessageBuf(), parser.getMessageSize());
            } else if (!keys[key].trigger_1 && keys[key].prev_trigger_1 && keys[key].state == DOWN) {
                // trigger_1 is released, key is going up
                keys[key].prev_trigger_1 = false;
                keys[key].debounce_1 = current_time;
                keys[key].state = GOING_UP;

                save_key_time(key);
            }
        }
    }
}

void setup()
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

    if (!net::Ethernet.begin(local_ip, local_subnet, INADDR_NONE)) {
        return;
    }

    server.begin(local_port);
}

void loop()
{
    int size = server.parsePacket();

    if (0 < size && static_cast<unsigned int>(size) <= sizeof(buf)) {
        server.read(buf, size);
    }

    current_time = micros();

    if (current_time >= last_matrix_update_time + time_between_loops) {
        last_matrix_update_time = current_time;
        processKeyboard();
    }
}
