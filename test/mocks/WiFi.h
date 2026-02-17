#pragma once
/*!
 * \file WiFi.h
 * \brief Mock ESP32 WiFi for native unit testing.
 */

#include <cstdint>

#define WL_CONNECTED 3
#define WL_DISCONNECTED 6
#define WL_IDLE_STATUS 0

inline int mockWiFiStatus = WL_DISCONNECTED;

class MockWiFi {
  public:
    int status() { return mockWiFiStatus; }
};

inline MockWiFi WiFi;
