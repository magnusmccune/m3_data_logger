/**
 * @file test_sensor_init.cpp
 * @brief Minimal test for sensor manager initialization
 *
 * Tests that sensor manager compiles and basic structure is valid.
 * Full hardware tests require actual IMU sensor.
 */

#include <Arduino.h>
#include <unity.h>
#include "sensor_manager.h"

void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
}

/**
 * Test that IMUSample struct has correct size
 * Expected: 7 floats (28 bytes) + 1 uint32_t (4 bytes) = 32 bytes
 */
void test_imu_sample_structure_size(void) {
    IMUSample sample;
    TEST_ASSERT_EQUAL(32, sizeof(sample));
}

/**
 * Test that sample rate constants are reasonable
 */
void test_sample_rate_constants(void) {
    TEST_ASSERT_EQUAL(100, SAMPLE_RATE_HZ);
    TEST_ASSERT_EQUAL(10, SAMPLE_INTERVAL_MS);
    TEST_ASSERT_EQUAL(20, CIRCULAR_BUFFER_SIZE);
}

/**
 * Test that sensor manager functions exist and link correctly
 * This is a compilation/linkage test, not a runtime test
 */
void test_sensor_functions_exist(void) {
    // Just verify functions exist and can be called
    // Actual behavior requires hardware

    // These will fail without real hardware, but that's expected
    // The test verifies the API exists
    bool result = initializeIMU();
    (void)result;  // Suppress unused warning

    TEST_PASS();
}

void setup() {
    delay(2000);  // Wait for serial monitor

    UNITY_BEGIN();

    RUN_TEST(test_imu_sample_structure_size);
    RUN_TEST(test_sample_rate_constants);
    RUN_TEST(test_sensor_functions_exist);

    UNITY_END();
}

void loop() {
    // Nothing here
}
