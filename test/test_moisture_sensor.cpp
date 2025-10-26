#include <Arduino.h>
#include <unity.h>
#include "drivers/sensors/moisture-sensor/moisture_sensor_hal.h"
#include "drivers/sensors/moisture-sensor/moisture_sensor_hal.cpp"

using namespace PlantMonitor::Drivers;

extern int mockAnalogValue;

MoistureSensorHAL *sensor;

/*!
 * \brief Setup called before each test
 */
void setUp() {
    sensor = new MoistureSensorHAL(3724, 0, analogRead); // dry/wet values + mock
    sensor->begin();
}

/*!
 * \brief Teardown called after each test
 */
void tearDown() {
    delete sensor;
}

/*!
 * \brief Test initialization
 */
void test_initialization() {
    TEST_ASSERT_TRUE_MESSAGE(sensor->begin(), "MoistureSensorHAL failed to initialize");
}

/*!
 * \brief Dry soil should return 0%
 */
void test_dry_soil_returns_zero() {
    mockAnalogValue = 3724; // dry value
    uint8_t level = sensor->readMoistureLevel();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, level, "Dry soil did not return 0% moisture level");
}

/*!
 * \brief Wet soil should return 100%
 */
void test_wet_soil_returns_hundred() {
    mockAnalogValue = 0; // wet value
    uint8_t level = sensor->readMoistureLevel();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(100, level, "Wet soil did not return 100% moisture level");
}

/*!
 * \brief Midpoint analog value should return roughly 50%
 */
void test_mid_value_returns_about_fifty() {
    mockAnalogValue = 1862; // midpoint
    uint8_t level = sensor->readMoistureLevel();
    TEST_ASSERT_INT_WITHIN_MESSAGE(10, 50, level, "Midpoint did not return approximately 50% (±10%)");
}

/*!
 * \brief Analog value higher than dry should saturate at 0%
 */
void test_saturation_high() {
    mockAnalogValue = 4000;
    uint8_t level = sensor->readMoistureLevel();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, level, "Analog value above dry did not saturate at 0%");
}

/*!
 * \brief Analog value below wet should saturate at 100%
 */
void test_saturation_low() {
    mockAnalogValue = -50;
    uint8_t level = sensor->readMoistureLevel();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(100, level, "Analog value below wet did not saturate at 100%");
}

/*!
 * \brief Multiple consecutive reads should stay within 0-100%
 */
void test_multiple_reads_stable() {
    mockAnalogValue = 1000;
    for (int i = 0; i < 10; i++) {
        uint8_t level = sensor->readMoistureLevel();
        TEST_ASSERT_TRUE_MESSAGE(level >= 0 && level <= 100, "Moisture level out of bounds (0-100%) on multiple reads");
    }
}

/*!
 * \brief Test that averaging function returns expected value
 */
void test_averaging_function() {
    // Use known sequence of values
    int values[] = { 1000, 1200, 1100, 1300, 1250 };
    int idx = 0;
    MoistureSensorHAL::AnalogReader avgReader = [&](uint8_t) {
        return values[idx++ % 5];
    };

    MoistureSensorHAL avgSensor(3724, 0, avgReader);
    avgSensor.begin();

    uint8_t level = avgSensor.readMoistureLevel();
    TEST_ASSERT_TRUE_MESSAGE(level >= 0 && level <= 100, "Averaged moisture level out of bounds (0-100%)");
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
    RUN_TEST(test_averaging_function);

    return UNITY_END();
}
