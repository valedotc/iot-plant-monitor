#include "sensor_task.h"

#include "drivers/sensors/temperature-sensor/bme280_hal.h"
#include "drivers/sensors/moisture-sensor/moisture_sensor_hal.h"
#include "drivers/sensors/light-sensor/light_sensor.h"

using namespace PlantMonitor::Drivers;

namespace PlantMonitor {
namespace Tasks {

static bme280HAL* environmentalSensor = nullptr;
static MoistureSensorHAL* moistureSensor = nullptr;
static LightSensor* lightSensor = nullptr;

static SensorData latestData;
static SemaphoreHandle_t dataMutex = nullptr;

static bool initSensors() {
    environmentalSensor = new bme280HAL();
    if (!environmentalSensor->begin()) {
        Serial.println("[ERROR] BME280 initialization failed");
        return false;
    }

    moistureSensor = new MoistureSensorHAL();
    if (!moistureSensor->begin()) {
        Serial.println("[ERROR] Moisture sensor initialization failed");
        return false;
    }

    lightSensor = new LightSensor();
    lightSensor->begin();

    Serial.println("[INIT] Sensors initialized");
    return true;
}

static bool readAllSensors(SensorData& data) {
    if (!environmentalSensor || !moistureSensor) {
        return false;
    }

    data.temperature   = environmentalSensor->readTemperature();
    data.humidity      = environmentalSensor->readHumidity();
    data.moisture      = moistureSensor->readMoistureLevel();
    
    Serial.printf("[SENSORS] [Light] Raw: %d\n" , lightSensor->readRaw());
    Serial.printf("[SENSORS] [Light] Voltage: %f\n" , lightSensor->readVoltage());
    Serial.printf("[SENSORS] [Light] Percentage: %f\n" , lightSensor->readPercentage());

    if (lightSensor->readPercentage() > 50){
        data.lightDetected = true; // TODO: Light sensor
    } else{
        data.lightDetected = false;
    }

    if (isnan(data.temperature) || isnan(data.humidity)) {
        Serial.println("[SENSORS] Invalid readings");
        return false;
    }

    return true;
}

static void SensorTask(void* pvParameters) {
    if (!initSensors()) {
        Serial.println("[SENSOR TASK] Init failed, task stopped");
        vTaskDelete(nullptr);
    }

    SensorData tempData;

    while (true) {
        if (readAllSensors(tempData)) {
            if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
                latestData = tempData;
                xSemaphoreGive(dataMutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void startSensorTask(uint32_t stackSize, UBaseType_t priority, BaseType_t core) {
    dataMutex = xSemaphoreCreateMutex();

    xTaskCreatePinnedToCore(
        SensorTask,
        "SensorTask",
        stackSize,
        nullptr,
        priority,
        nullptr,
        core
    );
}

bool getLatestSensorData(SensorData& out) {
    if (!dataMutex) return false;

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50))) {
        out = latestData;
        xSemaphoreGive(dataMutex);
        return true;
    }

    return false;
}

} // namespace Tasks
} // namespace PlantMonitor
