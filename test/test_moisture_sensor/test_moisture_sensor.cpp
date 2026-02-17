#include <Arduino.h>
#include <unity.h>
#include "drivers/sensors/moisture-sensor/moisture-sensor-hal.h"
#include "drivers/sensors/moisture-sensor/moisture-sensor-hal.cpp"

using namespace PlantMonitor::Drivers;

MoistureSensorHAL *sensor;

void setUp() {
    mockAnalogValue = 0;
    sensor = new MoistureSensorHAL(3724, 0, analogRead);
    sensor->begin();
}

void tearDown() {
    delete sensor;
    sensor = nullptr;
}

void test_initialization() {
    TEST_ASSERT_TRUE_MESSAGE(sensor->begin(), "MoistureSensorHAL failed to initialize");
}

void test_dry_soil_returns_zero() {
    mockAnalogValue = 3724;
    uint8_t level = sensor->readMoistureLevel();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, level, "Dry soil did not return 0%");
}

void test_wet_soil_returns_hundred() {
    mockAnalogValue = 0;
    uint8_t level = sensor->readMoistureLevel();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(100, level, "Wet soil did not return 100%");
}

void test_mid_value_returns_about_fifty() {
    mockAnalogValue = 1862;
    uint8_t level = sensor->readMoistureLevel();
    TEST_ASSERT_INT_WITHIN_MESSAGE(10, 50, level, "Midpoint did not return ~50%");
}

void test_saturation_high() {
    mockAnalogValue = 4000;
    uint8_t level = sensor->readMoistureLevel();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, level, "Above dry threshold did not saturate at 0%");
}

void test_saturation_low() {
    mockAnalogValue = -50;
    uint8_t level = sensor->readMoistureLevel();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(100, level, "Below wet threshold did not saturate at 100%");
}

void test_multiple_reads_stable() {
    mockAnalogValue = 1000;
    for (int i = 0; i < 10; i++) {
        uint8_t level = sensor->readMoistureLevel();
        TEST_ASSERT_TRUE_MESSAGE(level <= 100, "Moisture level out of 0-100 bounds");
    }
}

void test_custom_reader_averaging() {
    int values[] = {1000, 1200, 1100, 1300, 1250};
    int idx = 0;
    MoistureSensorHAL::AnalogReader avgReader = [&](uint8_t) {
        return values[idx++ % 5];
    };

    MoistureSensorHAL avgSensor(3724, 0, avgReader);
    avgSensor.begin();

    uint8_t level = avgSensor.readMoistureLevel();
    TEST_ASSERT_TRUE_MESSAGE(level <= 100, "Averaged moisture level out of bounds");
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_initialization);
    RUN_TEST(test_dry_soil_returns_zero);
    RUN_TEST(test_wet_soil_returns_hundred);
    RUN_TEST(test_mid_value_returns_about_fifty);
    RUN_TEST(test_saturation_high);
    RUN_TEST(test_saturation_low);
    RUN_TEST(test_multiple_reads_stable);
    RUN_TEST(test_custom_reader_averaging);
    return UNITY_END();
}
