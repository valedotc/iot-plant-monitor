#pragma once
/*!
 * \file FreeRTOS.h
 * \brief Mock FreeRTOS base types for native unit testing.
 */

#include <cstdint>

using UBaseType_t = unsigned int;
using BaseType_t = int;
using TickType_t = uint32_t;

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#endif

#define pdTRUE 1
#define pdFALSE 0
#define pdPASS pdTRUE

inline void vTaskDelay(TickType_t) {}
