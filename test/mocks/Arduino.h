#pragma once
/*!
 * \file Arduino.h
 * \brief Mock Arduino framework for native unit testing.
 */

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <string>
#include <functional>

// FreeRTOS base types (needed by app-config.h and task headers)
#include "freertos/FreeRTOS.h"

// ============ Arduino type aliases ============
using byte = uint8_t;

// ============ Pin modes & levels ============
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define INPUT_PULLUP 2
#define INPUT_PULLDOWN 3

// ============ Controllable mock state ============
inline int mockAnalogValue = 0;
inline uint32_t mockMillisValue = 0;
inline int mockDigitalValue = LOW;

// ============ GPIO stubs ============
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t) { return mockDigitalValue; }
inline int analogRead(uint8_t) { return mockAnalogValue; }

// ============ Timing ============
inline void delay(unsigned long) {}
inline uint32_t millis() { return mockMillisValue; }
inline void yield() {}

// ============ Math helpers (Arduino built-ins) ============
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    if (in_max == in_min) return out_min;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Arduino constrain accepts mixed types (macro in real Arduino)
template <typename T, typename U, typename V>
inline auto constrain(T x, U a, V b) -> decltype(x + a + b) {
    using Common = decltype(x + a + b);
    Common cx = x, ca = a, cb = b;
    if (cx < ca) return ca;
    if (cx > cb) return cb;
    return cx;
}

// ============ Minimal String class ============
class String {
  public:
    String() {}
    String(const char *s) : m_data(s ? s : "") {}
    String(const std::string &s) : m_data(s) {}
    String(int val) : m_data(std::to_string(val)) {}
    String(float val, int decimals = 2) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.*f", decimals, val);
        m_data = buf;
    }
    const char *c_str() const { return m_data.c_str(); }
    unsigned int length() const { return static_cast<unsigned int>(m_data.size()); }
    bool operator==(const String &o) const { return m_data == o.m_data; }
    bool operator!=(const String &o) const { return m_data != o.m_data; }
    String operator+(const String &o) const { return String((m_data + o.m_data).c_str()); }
    String &operator+=(const String &o) { m_data += o.m_data; return *this; }
    String &operator+=(const char *s) { if (s) m_data += s; return *this; }

  private:
    std::string m_data;
};

// ============ Minimal Serial mock ============
class MockSerial {
  public:
    void begin(unsigned long) {}
    void print(const char *) {}
    void print(int) {}
    void print(float, int = 2) {}
    void println(const char * = "") {}
    void println(int) {}
    void println(float, int = 2) {}
    void printf(const char *, ...) {}
};

inline MockSerial Serial;
