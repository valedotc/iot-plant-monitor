/*!
* \file Arduino.h
* \brief Mock Arduino.h for unit testing
* This file provides mock implementations of Arduino functions
* used in the MoistureSensorHAL for unit testing purposes.
*/

#pragma once

#include <cstdint>
#include <algorithm>

#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0

int mockAnalogValue = 0;

void pinMode(uint8_t, uint8_t) {
}
void delay(unsigned long) {
}
int analogRead(uint8_t) {
    return mockAnalogValue;
}
int map(long x, long in_min, long in_max, long out_min, long out_max) {
    if (in_max == in_min)
        return out_min;
    return static_cast<int>((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}
int constrain(int x, int a, int b) {
    if (x < a)
        return a;
    if (x > b)
        return b;
    return x;
}