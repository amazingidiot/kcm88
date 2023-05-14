#include "pedals.h"

#include "utils.h"

// Pedals
static const uint8_t PEDAL_DIGITAL_PIN_TIP = 1;
static const uint8_t PEDAL_DIGITAL_PIN_RING = 0;

static const uint8_t PEDAL_ANALOG_PIN = 23;

static bool pedal_digital_tip = false;
static bool pedal_digital_tip_prev = false;
static uint32_t pedal_digital_tip_debounce = 0;
static bool pedal_digital_tip_pressed = false;

static bool pedal_digital_ring = false;
static bool pedal_digital_ring_prev = false;
static uint32_t pedal_digital_ring_debounce = 0;
static bool pedal_digital_ring_pressed = false;

void setupPedals()
{
    // Pedals
    pinMode(PEDAL_DIGITAL_PIN_TIP, INPUT_PULLUP);
    pinMode(PEDAL_DIGITAL_PIN_RING, INPUT_PULLUP);

    pinMode(PEDAL_ANALOG_PIN, INPUT);
}

void loopPedals(uint32_t current_time, uint32_t last_time)
{
    pedal_digital_ring = digitalReadFast(PEDAL_DIGITAL_PIN_RING);
    pedal_digital_tip = !digitalReadFast(PEDAL_DIGITAL_PIN_TIP);

    if (pedal_digital_tip && !pedal_digital_tip_prev && !pedal_digital_tip_pressed) {
        // Pedal was pressed
        if (pedal_digital_tip_debounce == 0) {
            pedal_digital_tip_debounce = current_time;
            pedal_digital_tip_pressed = true;

            sendPedalDamper(true);
        }
        if (debounce(pedal_digital_tip_debounce, current_time)) {
            // Debounce time over, reset counter
            pedal_digital_tip_debounce = 0;
        }
    }

    if (!pedal_digital_tip && pedal_digital_tip_pressed) {
        // Pedal was released
        pedal_digital_tip_debounce = 0;
        pedal_digital_tip_pressed = false;

        sendPedalDamper(false);
    }

    pedal_digital_ring_prev = pedal_digital_ring;
    pedal_digital_tip_prev = pedal_digital_tip;
}