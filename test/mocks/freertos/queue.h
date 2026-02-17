#pragma once
/*!
 * \file queue.h
 * \brief Mock FreeRTOS queue types for native unit testing.
 */

#include "FreeRTOS.h"

using QueueHandle_t = void *;

inline QueueHandle_t xQueueCreate(UBaseType_t, UBaseType_t) { return nullptr; }
inline BaseType_t xQueueSend(QueueHandle_t, const void *, TickType_t) { return pdTRUE; }
inline BaseType_t xQueueReceive(QueueHandle_t, void *, TickType_t) { return pdFALSE; }
inline void vQueueDelete(QueueHandle_t) {}
