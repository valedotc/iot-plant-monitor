
#include <Arduino.h>
#include "app-config.h"
#include "tasks/sensor/sensor-task.h"
#include "tasks/iot/iot-task.h"
#include "tasks/display/display-task.h"
#include "utils/configuration/config.h"

/*!
 * \file main.cpp
 * \brief Main entry point for the Plant Monitor System
 */

using namespace PlantMonitor;

/*!
 * \brief System initialization
 */
void setup() {

    // Initialize serial
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n");
    Serial.println("=====================================");
    Serial.println("  Plant Monitor System v1.0");
    Serial.println("  ESP32 IoT Monitoring System");
    Serial.println("=====================================");

    Tasks::startDisplayTask(
        Config::Tasks::DISPLAY_STACK_SIZE,
        Config::Tasks::DISPLAY_PRIORITY,
        Config::Tasks::DISPLAY_CORE);

    Tasks::startIoTTask(
        Config::Tasks::IOT_STACK_SIZE,
        Config::Tasks::IOT_PRIORITY,
        Config::Tasks::IOT_CORE);

    Tasks::startSensorTask(
        Config::Tasks::SENSOR_STACK_SIZE,
        Config::Tasks::SENSOR_PRIORITY,
        Config::Tasks::SENSOR_CORE);

    Serial.println("[INIT] System ready\n");
}

void loop() {
    // Empty loop since all operations are handled in FreeRTOS tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
}