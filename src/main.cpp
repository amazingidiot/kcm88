#include <Arduino.h>

#include "keyboard.h"
#include "net_data.h"
#include "pedals.h"

static const uint32_t time_between_loops = 600;
static uint32_t last_loop_time = 0;

// store current time every loop for velocity calculations
static uint32_t current_time = 0;
// time of last loop, needed for timer rollover
static uint32_t last_time = 0;

void setup()
{
    setupNetData();
    setupKeyboard();
    setupPedals();
}

void loop()
{
    loopNetData();

    current_time = micros();

    if (current_time >= last_loop_time + time_between_loops) {
        last_loop_time = current_time;

        loopKeyboard(current_time, last_time);
        loopPedals(current_time, last_time);

        last_time = current_time;
    }
}
