#include <unity.h>
#include "utils/moving-average/moving-average.h"
#include "utils/moving-average/moving-average.cpp"

using PlantMonitor::Utils::MovingAverage;

void setUp() {}
void tearDown() {}

void test_empty_average_returns_zero() {
    MovingAverage ma(5);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ma.getAverage());
}

void test_single_sample() {
    MovingAverage ma(5);
    ma.addSample(10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, ma.getAverage());
}

void test_partial_fill() {
    MovingAverage ma(5);
    ma.addSample(2.0f);
    ma.addSample(4.0f);
    ma.addSample(6.0f);
    // Average of 2, 4, 6 = 4.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, ma.getAverage());
}

void test_full_window() {
    MovingAverage ma(3);
    ma.addSample(1.0f);
    ma.addSample(2.0f);
    ma.addSample(3.0f);
    // Average of 1, 2, 3 = 2.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, ma.getAverage());
}

void test_circular_wraparound() {
    MovingAverage ma(3);
    ma.addSample(1.0f);
    ma.addSample(2.0f);
    ma.addSample(3.0f);
    // Buffer full: [1, 2, 3]
    ma.addSample(10.0f);
    // Buffer now: [10, 2, 3] -> average = (10+2+3)/3 = 5.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, ma.getAverage());
}

void test_wraparound_replaces_oldest() {
    MovingAverage ma(3);
    ma.addSample(10.0f);
    ma.addSample(20.0f);
    ma.addSample(30.0f);
    ma.addSample(40.0f);
    ma.addSample(50.0f);
    // Last 3 values: 30, 40, 50 -> average = 40.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 40.0f, ma.getAverage());
}

void test_clear_resets_state() {
    MovingAverage ma(5);
    ma.addSample(100.0f);
    ma.addSample(200.0f);
    ma.clear();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ma.getAverage());
    // After clear, adding a sample works normally
    ma.addSample(7.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.0f, ma.getAverage());
}

void test_window_size_one() {
    MovingAverage ma(1);
    ma.addSample(5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, ma.getAverage());
    ma.addSample(99.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 99.0f, ma.getAverage());
}

void test_large_number_of_samples() {
    MovingAverage ma(10);
    for (int i = 0; i < 1000; i++) {
        ma.addSample(static_cast<float>(i));
    }
    // Last 10 values: 990..999 -> average = 994.5
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 994.5f, ma.getAverage());
}

void test_negative_values() {
    MovingAverage ma(3);
    ma.addSample(-10.0f);
    ma.addSample(-20.0f);
    ma.addSample(-30.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -20.0f, ma.getAverage());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_empty_average_returns_zero);
    RUN_TEST(test_single_sample);
    RUN_TEST(test_partial_fill);
    RUN_TEST(test_full_window);
    RUN_TEST(test_circular_wraparound);
    RUN_TEST(test_wraparound_replaces_oldest);
    RUN_TEST(test_clear_resets_state);
    RUN_TEST(test_window_size_one);
    RUN_TEST(test_large_number_of_samples);
    RUN_TEST(test_negative_values);
    return UNITY_END();
}
