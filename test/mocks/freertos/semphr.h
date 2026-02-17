#pragma once
/*!
 * \file semphr.h
 * \brief Mock FreeRTOS semaphore types for native unit testing.
 */

#include "FreeRTOS.h"

using SemaphoreHandle_t = void *;

inline SemaphoreHandle_t xSemaphoreCreateBinary() { return reinterpret_cast<void *>(1); }
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return reinterpret_cast<void *>(1); }
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t) { return pdTRUE; }
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t) { return pdTRUE; }
inline void vSemaphoreDelete(SemaphoreHandle_t) {}
