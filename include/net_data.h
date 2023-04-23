#ifndef NET_DATA_H
#define NET_DATA_H

#include <Arduino.h>

void setupNetData();
void loopNetData();

void sendNoteOn(uint8_t note, float velocity);
void sendNoteOff(uint8_t note, float velocity);

void sendPedalDamper(bool pressed);

void sendPedalExpression(float value);

#endif