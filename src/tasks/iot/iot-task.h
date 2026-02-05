#pragma once

#include <Arduino.h>
#include "../sensor/sensor-task.h"

namespace PlantMonitor {
namespace Tasks {

/*!
 * \brief Starts the IoT task (BLE + WiFi + MQTT FSM)
 * \param stackSize Task stack size
 * \param priority Task priority
 * \param core ESP32 core to pin the task to
 */
void startIoTTask(
    uint32_t stackSize = 8192,
    UBaseType_t priority = 2,
    BaseType_t core = 0
);

} // namespace Tasks
} // namespace PlantMonitor
