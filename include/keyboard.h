#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <Arduino.h>

void setupKeyboard();
void loopKeyboard(uint32_t current_time, uint32_t last_time);

#endif