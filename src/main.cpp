
#include <Arduino.h>
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

    // NOTE: Uncomment to reset configuration during development
    // ConfigHandler::clear();
    // Serial.println("[DEV] Configuration cleared");

    Tasks::startDisplayTask(
        4096,
        3, //Highest priority
        0  // Core 0
    );

    Tasks::startIoTTask(
        8192,
        1, //Low priority
        1  // Core 1 - Keeps WiFi/BLE operations separate from Core 0 to prevent watchdog triggers
    );

    Tasks::startSensorTask(
        4096,
        2, //Medium priority
        1  // Core 0
    );

    Serial.println("[INIT] System ready\n");
}

void loop() {
    // Empty loop since all operations are handled in FreeRTOS tasks
    vTaskDelay(pdMS_TO_TICKS(1000));
}