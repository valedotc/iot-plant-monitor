#pragma once

#include <Arduino.h>

namespace PlantMonitor {
namespace Tasks {

struct SensorData {
    float temperature;
    float humidity;
    float moisture;
    bool  lightDetected;
};

// start task
void startSensorTask(
    uint32_t stackSize = 4096,
    UBaseType_t priority = 2,
    BaseType_t core = 0
);

// Get thread safe latest sensor data
bool getLatestSensorData(SensorData& out);

} // namespace Tasks
} // namespace PlantMonitor
