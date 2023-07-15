#pragma once

#include "config.h"
#include <Arduino.h>

// Return true when debounce time is over
bool debounce(uint32_t debounce_timestamp, uint32_t current_time);
