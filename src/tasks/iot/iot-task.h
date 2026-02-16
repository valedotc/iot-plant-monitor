#pragma once

#include <Arduino.h>
#include "app-config.h"
#include "../sensor/sensor-task.h"

/*!
 * \file iot-task.h
 * \brief Task for IoT communication (BLE + WiFi + MQTT)
 * 
 * This task manages all IoT-related functionalities including Bluetooth Low Energy (BLE) communication,
 * WiFi connectivity, and MQTT protocol operations. It runs in its own FreeRTOS thread to ensure
 * responsive handling of network events and data transmission.
 */

namespace PlantMonitor {
namespace Tasks {

/*!
 * \brief Starts the IoT task (BLE + WiFi + MQTT FSM)
 * \param stackSize Task stack size
 * \param priority Task priority
 * \param core ESP32 core to pin the task to
 */
void startIoTTask(
    uint32_t stackSize = Config::Tasks::IOT_STACK_SIZE,
    UBaseType_t priority = Config::Tasks::IOT_PRIORITY,
    BaseType_t core = Config::Tasks::IOT_CORE);

} // namespace Tasks
} // namespace PlantMonitor
