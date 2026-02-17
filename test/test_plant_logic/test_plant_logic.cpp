#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <unity.h>

// Include source units under test (and their transitive deps)
#include "utils/moving-average/moving-average.cpp"
#include "utils/derivative-filter/derivative-filter.cpp"
#include "utils/timer/periodic-timer.cpp"
#include "utils/configuration/config.cpp"

// Include sensor-task.h for SensorData definition before our stub
#include "tasks/sensor/sensor-task.h"

// Provide the getLatestSensorData stub required by plant-state-machine.cpp
namespace PlantMonitor {
namespace Tasks {

static SensorData s_mockSensorData = {25.0f, 50.0f, 50.0f, true};
static bool s_mockSensorAvailable = true;

bool getLatestSensorData(SensorData &out) {
    if (!s_mockSensorAvailable) return false;
    out = s_mockSensorData;
    return true;
}

} // namespace Tasks
} // namespace PlantMonitor

#include "tasks/plant/plant-state-machine.cpp"

using namespace PlantMonitor::Tasks;

void setUp() {
    Preferences::resetAllMockStorage();
    mockMillisValue = 0;
}

void tearDown() {}

// ============ areSensorsInRange tests ============

static PlantThresholds makeThresholds() {
    return {15.0f, 30.0f, 30.0f, 80.0f, 20.0f, 80.0f, 4.0f, 24.0f};
}

void test_all_sensors_in_range() {
    SensorData data = {22.0f, 50.0f, 50.0f, true};
    TEST_ASSERT_TRUE(areSensorsInRange(data, makeThresholds()));
}

void test_temperature_below_min() {
    SensorData data = {10.0f, 50.0f, 50.0f, true};
    TEST_ASSERT_FALSE(areSensorsInRange(data, makeThresholds()));
}

void test_temperature_above_max() {
    SensorData data = {35.0f, 50.0f, 50.0f, true};
    TEST_ASSERT_FALSE(areSensorsInRange(data, makeThresholds()));
}

void test_humidity_below_min() {
    SensorData data = {22.0f, 10.0f, 50.0f, true};
    TEST_ASSERT_FALSE(areSensorsInRange(data, makeThresholds()));
}

void test_humidity_above_max() {
    SensorData data = {22.0f, 95.0f, 50.0f, true};
    TEST_ASSERT_FALSE(areSensorsInRange(data, makeThresholds()));
}

void test_moisture_below_min() {
    SensorData data = {22.0f, 50.0f, 5.0f, true};
    TEST_ASSERT_FALSE(areSensorsInRange(data, makeThresholds()));
}

void test_moisture_above_max() {
    SensorData data = {22.0f, 50.0f, 95.0f, true};
    TEST_ASSERT_FALSE(areSensorsInRange(data, makeThresholds()));
}

void test_boundary_at_min() {
    SensorData data = {15.0f, 30.0f, 20.0f, true};
    TEST_ASSERT_TRUE(areSensorsInRange(data, makeThresholds()));
}

void test_boundary_at_max() {
    SensorData data = {30.0f, 80.0f, 80.0f, true};
    TEST_ASSERT_TRUE(areSensorsInRange(data, makeThresholds()));
}

void test_multiple_sensors_out_of_range() {
    SensorData data = {5.0f, 5.0f, 5.0f, false};
    TEST_ASSERT_FALSE(areSensorsInRange(data, makeThresholds()));
}

// ============ loadThresholdsFromConfig tests ============

void test_load_thresholds_valid_config() {
    AppConfig cfg;
    cfg.params = {0.0f, 15.0f, 30.0f, 40.0f, 70.0f, 25.0f, 75.0f, 6.0f, 1.0f};
    PlantThresholds t;
    TEST_ASSERT_TRUE(loadThresholdsFromConfig(cfg, t));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 15.0f, t.tempMin);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 30.0f, t.tempMax);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f, t.humidityMin);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 70.0f, t.humidityMax);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.0f, t.moistureMin);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 75.0f, t.moistureMax);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 6.0f, t.lightMin);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.0f, t.lightMax);
}

void test_load_thresholds_insufficient_params() {
    AppConfig cfg;
    cfg.params = {0.0f, 15.0f, 30.0f}; // only 3 params, need 8
    PlantThresholds t;
    TEST_ASSERT_FALSE(loadThresholdsFromConfig(cfg, t));
}

void test_load_thresholds_exactly_eight_params() {
    AppConfig cfg;
    cfg.params = {0.0f, 10.0f, 35.0f, 20.0f, 90.0f, 10.0f, 90.0f, 12.0f};
    PlantThresholds t;
    TEST_ASSERT_TRUE(loadThresholdsFromConfig(cfg, t));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, t.tempMin);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.0f, t.lightMin);
}

// ============ plantStateToString tests ============

void test_state_to_string_happy() {
    TEST_ASSERT_EQUAL_STRING("HAPPY", plantStateToString(PlantState::PLANT_HAPPY));
}

void test_state_to_string_angry() {
    TEST_ASSERT_EQUAL_STRING("ANGRY", plantStateToString(PlantState::PLANT_ANGRY));
}

void test_state_to_string_dying() {
    TEST_ASSERT_EQUAL_STRING("DYING", plantStateToString(PlantState::PLANT_DYING));
}

int main() {
    UNITY_BEGIN();

    // areSensorsInRange
    RUN_TEST(test_all_sensors_in_range);
    RUN_TEST(test_temperature_below_min);
    RUN_TEST(test_temperature_above_max);
    RUN_TEST(test_humidity_below_min);
    RUN_TEST(test_humidity_above_max);
    RUN_TEST(test_moisture_below_min);
    RUN_TEST(test_moisture_above_max);
    RUN_TEST(test_boundary_at_min);
    RUN_TEST(test_boundary_at_max);
    RUN_TEST(test_multiple_sensors_out_of_range);

    // loadThresholdsFromConfig
    RUN_TEST(test_load_thresholds_valid_config);
    RUN_TEST(test_load_thresholds_insufficient_params);
    RUN_TEST(test_load_thresholds_exactly_eight_params);

    // plantStateToString
    RUN_TEST(test_state_to_string_happy);
    RUN_TEST(test_state_to_string_angry);
    RUN_TEST(test_state_to_string_dying);

    return UNITY_END();
}
