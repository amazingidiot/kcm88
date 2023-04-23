#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

#include "config.h"

// Return true when debounce time is over
bool debounce(uint32_t debounce_timestamp, uint32_t current_time);
#endif