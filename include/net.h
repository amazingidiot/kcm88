#pragma once

#include <Arduino.h>

int setupNet();
void loopNet();

bool isActive();

void sendNoteOn(uint8_t note, float velocity);
void sendNoteOff(uint8_t note, float velocity);

void sendPedalDamper(bool pressed);

void sendPedalExpression(float value);
