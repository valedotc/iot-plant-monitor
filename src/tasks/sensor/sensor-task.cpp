#include "sensor-task.h"

#include "drivers/sensors/temperature-sensor/bme280-hal.h"
#include "drivers/sensors/moisture-sensor/moisture-sensor-hal.h"
#include "drivers/sensors/light-sensor/light-sensor.h"
#include "tasks/plant/plant-config.h"

using namespace PlantMonitor::Drivers;

namespace PlantMonitor {
namespace Tasks {

static Bme280Hal *sensor_task_environmental_sensor = nullptr;
static MoistureSensorHAL *sensor_task_moisture_sensor = nullptr;
static LightSensor *sensor_task_light_sensor = nullptr;

static SensorData sensor_task_latest_data;
static SemaphoreHandle_t sensor_task_data_mutex = nullptr;

static bool prv_init_sensors() {
    sensor_task_environmental_sensor = new Bme280Hal();
    if (!sensor_task_environmental_sensor->begin()) {
        Serial.println("[ERROR] BME280 initialization failed");
        return false;
    }

    sensor_task_moisture_sensor = new MoistureSensorHAL();
    if (!sensor_task_moisture_sensor->begin()) {
        Serial.println("[ERROR] Moisture sensor initialization failed");
        return false;
    }

    sensor_task_light_sensor = new LightSensor();
    sensor_task_light_sensor->begin();

    Serial.println("[INIT] Sensors initialized");
    return true;
}

static bool prv_read_all_sensors(SensorData &data) {
    if (!sensor_task_environmental_sensor || !sensor_task_moisture_sensor) {
        return false;
    }

    data.temperature = sensor_task_environmental_sensor->readTemperature();
    data.humidity = sensor_task_environmental_sensor->readHumidity();
    data.moisture = sensor_task_moisture_sensor->readMoistureLevel();

    // Read light sensor with averaging (10 samples to avoid spurious readings)
    int lightRawAvg = sensor_task_light_sensor->readRawAverage(10);
    float lightPercentage = (lightRawAvg * 100.0f) / 4095.0f;

    // Use configured percentage threshold from plant-config.h
    using namespace PlantMonitor::Tasks;
    data.lightDetected = (lightPercentage >= LIGHT_DETECTION_THRESHOLD_PERCENT);

    if (isnan(data.temperature) || isnan(data.humidity)) {
        Serial.println("[SENSORS] Invalid readings");
        return false;
    }

    return true;
}

static void prv_sensor_task(void *pvParameters) {
    if (!prv_init_sensors()) {
        Serial.println("[SENSOR TASK] Init failed, task stopped");
        vTaskDelete(nullptr);
    }

    SensorData tempData;

    while (true) {
        if (prv_read_all_sensors(tempData)) {
            if (xSemaphoreTake(sensor_task_data_mutex, portMAX_DELAY)) {
                sensor_task_latest_data = tempData;
                xSemaphoreGive(sensor_task_data_mutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void startSensorTask(uint32_t stackSize, UBaseType_t priority, BaseType_t core) {
    sensor_task_data_mutex = xSemaphoreCreateMutex();

    xTaskCreatePinnedToCore(
        prv_sensor_task,
        "SensorTask",
        stackSize,
        nullptr,
        priority,
        nullptr,
        core);
}

bool getLatestSensorData(SensorData &out) {
    if (!sensor_task_data_mutex)
        return false;

    if (xSemaphoreTake(sensor_task_data_mutex, pdMS_TO_TICKS(50))) {
        out = sensor_task_latest_data;
        xSemaphoreGive(sensor_task_data_mutex);
        return true;
    }

    return false;
}

} // namespace Tasks
} // namespace PlantMonitor
