#include <unity.h>
#include "utils/moving-average/moving-average.h"
#include "utils/moving-average/moving-average.cpp"
#include "utils/derivative-filter/derivative-filter.h"
#include "utils/derivative-filter/derivative-filter.cpp"

using PlantMonitor::Utils::DerivativeFilter;

void setUp() {}
void tearDown() {}

void test_first_call_returns_zero() {
    DerivativeFilter df;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, df.apply(42.0f));
}

void test_constant_input_returns_zero() {
    DerivativeFilter df;
    df.apply(10.0f); // first call
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, df.apply(10.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, df.apply(10.0f));
}

void test_linear_increase() {
    DerivativeFilter df(1.0f, 0);
    df.apply(0.0f);
    // derivative = (5 - 0) * 1.0 = 5.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, df.apply(5.0f));
    // derivative = (10 - 5) * 1.0 = 5.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, df.apply(10.0f));
}

void test_linear_decrease() {
    DerivativeFilter df(1.0f, 0);
    df.apply(100.0f);
    // derivative = (90 - 100) * 1.0 = -10.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.0f, df.apply(90.0f));
}

void test_scale_factor() {
    DerivativeFilter df(2.0f, 0);
    df.apply(0.0f);
    // derivative = (10 - 0) * 2.0 = 20.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, df.apply(10.0f));
}

void test_reset_clears_state() {
    DerivativeFilter df;
    df.apply(50.0f);
    df.apply(100.0f);
    df.reset();
    // After reset, first call should return 0 again
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, df.apply(200.0f));
}

void test_with_smoothing_window() {
    DerivativeFilter df(1.0f, 3);
    // With smoothing, the input is averaged before differentiation
    df.apply(0.0f);  // smoothed = 0, no prev -> 0
    df.apply(6.0f);  // smoothed = avg(0,6) = 3.0, prev = 0 -> 3.0
    float d3 = df.apply(12.0f); // smoothed = avg(0,6,12) = 6.0, prev = 3.0 -> 3.0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, d3);
}

void test_zero_scale_always_zero() {
    DerivativeFilter df(0.0f, 0);
    df.apply(0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, df.apply(999.0f));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_first_call_returns_zero);
    RUN_TEST(test_constant_input_returns_zero);
    RUN_TEST(test_linear_increase);
    RUN_TEST(test_linear_decrease);
    RUN_TEST(test_scale_factor);
    RUN_TEST(test_reset_clears_state);
    RUN_TEST(test_with_smoothing_window);
    RUN_TEST(test_zero_scale_always_zero);
    return UNITY_END();
}
