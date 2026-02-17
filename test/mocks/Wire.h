#pragma once
/*!
 * \file Wire.h
 * \brief Mock I2C Wire library for native unit testing.
 */

#include <cstdint>

class TwoWire {
  public:
    void begin() {}
    void begin(int, int) {}
    void setClock(uint32_t) {}
};

inline TwoWire Wire;
