#pragma once
#include <Arduino.h>
#include "app-config.h"

/*!
 * \file sensor-task.h
 * \brief Task for reading and processing sensor data
 * 
 * This task is responsible for periodically reading data from the connected sensors (temperature, humidity, soil moisture, light),
 * processing the data (e.g., applying filters), and making it available to other tasks such as the display and MQTT communication tasks.
 * 
 * The task runs in its own FreeRTOS thread and uses a thread-safe mechanism to share the latest sensor data with other tasks.
 */

namespace PlantMonitor {
namespace Tasks {

/*!
 * \struct SensorData
 * \brief Structure to hold sensor readings
 */
struct SensorData {
    float temperature;
    float humidity;
    float moisture;
    bool lightDetected;
};

/*!
 * \brief Start the sensor task
 * \param stackSize Stack size for the task (default: 4096 bytes)
 * \param priority Task priority (default: 2)
 * \param core Core to pin the task to (default: 0)
 */
void startSensorTask(
    uint32_t stackSize = Config::Tasks::SENSOR_STACK_SIZE,
    UBaseType_t priority = Config::Tasks::SENSOR_PRIORITY,
    BaseType_t core = Config::Tasks::SENSOR_CORE);

/*!
 * \brief Get the latest sensor data in a thread-safe manner
 * \param out Reference to SensorData structure to populate
 * \return true if data was successfully retrieved, false otherwise
 */
bool getLatestSensorData(SensorData &out);

} // namespace Tasks
} // namespace PlantMonitor
