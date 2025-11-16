/**
 * @file sensor_manager.h
 * @brief IMU sensor management for ISM330DHCX 6DoF sensor
 *
 * Handles initialization, configuration, and data acquisition from the
 * SparkFun ISM330DHCX IMU sensor. Provides 100Hz sampling with circular
 * buffering to prevent data loss during SD card writes.
 *
 * Hardware: SparkFun 6DoF ISM330DHCX (SEN-19764), I2C address 0x6B
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>

// IMU sample structure
struct IMUSample {
    uint32_t timestamp_ms;      // Timestamp when sample was read
    float lat;                  // Latitude (decimal degrees, 0.0 if no GPS fix)
    float lon;                  // Longitude (decimal degrees, 0.0 if no GPS fix)
    float accel_x;              // Acceleration X-axis (g)
    float accel_y;              // Acceleration Y-axis (g)
    float accel_z;              // Acceleration Z-axis (g)
    float gyro_x;               // Gyroscope X-axis (degrees/sec)
    float gyro_y;               // Gyroscope Y-axis (degrees/sec)
    float gyro_z;               // Gyroscope Z-axis (degrees/sec)
};

// Sensor configuration constants
constexpr uint16_t SAMPLE_RATE_HZ = 100;            // Target sampling rate
constexpr uint16_t SAMPLE_INTERVAL_MS = 1000 / SAMPLE_RATE_HZ;  // 10ms per sample
constexpr uint8_t CIRCULAR_BUFFER_SIZE = 20;        // Buffer 200ms of data

/**
 * @brief Initialize the IMU sensor
 *
 * Configures the ISM330DHCX with:
 * - Accelerometer: ±4g range, 100Hz ODR
 * - Gyroscope: 500 DPS range, 100Hz ODR
 * - FIFO disabled (using software circular buffer)
 *
 * @return true if initialization successful, false otherwise
 */
bool initializeIMU();

/**
 * @brief Read a single sample from the IMU
 *
 * Reads accelerometer and gyroscope data with timestamp. This function
 * should be called at 100Hz (every 10ms) for accurate data collection.
 *
 * @param sample Pointer to IMUSample structure to fill
 * @return true if read successful, false if sensor error
 */
bool readIMUSample(IMUSample* sample);

/**
 * @brief Start IMU data collection
 *
 * Resets internal buffers and timing. Call this when entering RECORDING state.
 */
void startSampling();

/**
 * @brief Stop IMU data collection
 *
 * Call this when exiting RECORDING state.
 */
void stopSampling();

/**
 * @brief Check if new sample is available based on timing
 *
 * Uses non-blocking timing to determine if it's time to read the next sample.
 *
 * @return true if enough time has elapsed for next sample
 */
bool isSampleReady();

/**
 * @brief Get current sampling statistics
 *
 * @param actualRate Pointer to store actual sample rate (Hz)
 * @param lossRate Pointer to store sample loss percentage (0.0-100.0)
 */
/**
 * @brief Get next buffered sample from circular buffer
 *
 * Retrieves the next available sample from the internal circular buffer.
 * Use this to drain buffered samples during recording.
 *
 * @param sample Pointer to IMUSample structure to fill
 * @return true if sample retrieved, false if buffer empty
 */
bool getBufferedSample(IMUSample* sample);

void getSamplingStats(float* actualRate, float* lossRate);

#endif // SENSOR_MANAGER_H
