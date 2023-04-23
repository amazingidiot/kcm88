#ifndef PEDAL_H
#define PEDAL_H

#include <Arduino.h>

#include "net_data.h"

void setupPedals();
void loopPedals(uint32_t current_time, uint32_t last_time);

#endif