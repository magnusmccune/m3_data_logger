/**
 * @file sensor_manager.cpp
 * @brief IMU sensor management implementation
 *
 * Handles ISM330DHCX 6DoF IMU sensor with circular buffering for reliable
 * 100Hz data collection without sample loss during SD card writes.
 */

#include "sensor_manager.h"
#include <SparkFun_ISM330DHCX.h>
#include <Wire.h>

// IMU sensor instance
static SparkFun_ISM330DHCX imu;

// Circular buffer for samples
static IMUSample circularBuffer[CIRCULAR_BUFFER_SIZE];
static uint8_t bufferWriteIndex = 0;
static uint8_t bufferReadIndex = 0;
static uint8_t bufferCount = 0;

// Timing and statistics
static uint32_t lastSampleTime = 0;
static bool samplingActive = false;
static uint32_t samplesCollected = 0;
static uint32_t samplesLost = 0;
static uint32_t sessionStartTime = 0;

// Forward declarations for internal functions
static bool addSampleToBuffer(const IMUSample& sample);
static bool getNextSample(IMUSample* sample);

/**
 * @brief Initialize the IMU sensor
 */
bool initializeIMU() {
    Serial.println("[IMU] Initializing ISM330DHCX...");

    // Initialize I2C communication
    Wire.begin();
    Wire.setClock(400000);  // 400kHz I2C for faster reads

    // Initialize IMU with I2C address 0x6B
    if (!imu.begin()) {
        Serial.println("[IMU] ERROR: Failed to communicate with ISM330DHCX at 0x6B");
        return false;
    }

    // Software reset to known state
    imu.deviceReset();
    delay(100);  // Wait for reset to complete

    // Configure accelerometer: ±4g range, 100Hz ODR
    imu.setAccelDataRate(ISM_XL_ODR_104Hz);  // Closest to 100Hz
    imu.setAccelFullScale(ISM_4g);

    // Configure gyroscope: 500 DPS range, 100Hz ODR
    imu.setGyroDataRate(ISM_GY_ODR_104Hz);   // Closest to 100Hz
    imu.setGyroFullScale(ISM_500dps);

    // Disable FIFO - we use software circular buffer
    imu.setFIFOMode(ISM_BYPASS_MODE);

    Serial.println("[IMU] ISM330DHCX initialized successfully");
    Serial.println("[IMU] Config: Accel ±4g @ 104Hz, Gyro 500dps @ 104Hz");

    return true;
}

/**
 * @brief Read a single sample from the IMU
 */
bool readIMUSample(IMUSample* sample) {
    if (!sample) {
        return false;
    }

    // Check if new data is available
    sfe_ism_data_t accelData;
    sfe_ism_data_t gyroData;

    // Read accelerometer data
    if (!imu.getAccel(&accelData)) {
        Serial.println("[IMU] WARNING: Failed to read accelerometer data");
        return false;
    }

    // Read gyroscope data
    if (!imu.getGyro(&gyroData)) {
        Serial.println("[IMU] WARNING: Failed to read gyroscope data");
        return false;
    }

    // Populate sample structure
    sample->timestamp_ms = millis();
    sample->accel_x = accelData.xData;
    sample->accel_y = accelData.yData;
    sample->accel_z = accelData.zData;
    sample->gyro_x = gyroData.xData;
    sample->gyro_y = gyroData.yData;
    sample->gyro_z = gyroData.zData;

    // Track sampling statistics
    if (samplingActive) {
        samplesCollected++;

        // Check for buffer overflow (sample loss)
        if (!addSampleToBuffer(*sample)) {
            samplesLost++;
            Serial.println("[IMU] WARNING: Circular buffer full, sample lost!");
            return false;
        }
    }

    return true;
}

/**
 * @brief Start IMU data collection
 */
void startSampling() {
    Serial.println("[IMU] Starting data collection...");

    // Reset buffers and counters
    bufferWriteIndex = 0;
    bufferReadIndex = 0;
    bufferCount = 0;
    samplesCollected = 0;
    samplesLost = 0;
    sessionStartTime = millis();
    lastSampleTime = millis();
    samplingActive = true;

    Serial.println("[IMU] Sampling started");
}

/**
 * @brief Stop IMU data collection
 */
void stopSampling() {
    samplingActive = false;

    // Print final statistics
    uint32_t sessionDuration = millis() - sessionStartTime;
    float actualRate = 0.0f;
    float lossRate = 0.0f;

    if (sessionDuration > 0) {
        actualRate = (samplesCollected * 1000.0f) / sessionDuration;
    }

    if (samplesCollected > 0) {
        lossRate = (samplesLost * 100.0f) / (samplesCollected + samplesLost);
    }

    Serial.println("[IMU] Sampling stopped");
    Serial.printf("[IMU] Total samples: %u, Lost: %u (%.2f%%)\n",
                  samplesCollected, samplesLost, lossRate);
    Serial.printf("[IMU] Actual rate: %.1f Hz\n", actualRate);
}

/**
 * @brief Check if new sample is available based on timing
 */
bool isSampleReady() {
    if (!samplingActive) {
        return false;
    }

    // Check if enough time has elapsed for next sample
    uint32_t currentTime = millis();
    uint32_t elapsed = currentTime - lastSampleTime;

    if (elapsed >= SAMPLE_INTERVAL_MS) {
        lastSampleTime = currentTime;
        return true;
    }

    return false;
}

/**
 * @brief Get current sampling statistics
 */
void getSamplingStats(float* actualRate, float* lossRate) {
    if (!actualRate || !lossRate) {
        return;
    }

    uint32_t sessionDuration = millis() - sessionStartTime;

    // Calculate actual sampling rate
    if (sessionDuration > 0 && samplingActive) {
        *actualRate = (samplesCollected * 1000.0f) / sessionDuration;
    } else {
        *actualRate = 0.0f;
    }

    // Calculate sample loss rate
    uint32_t totalSamples = samplesCollected + samplesLost;
    if (totalSamples > 0) {
        *lossRate = (samplesLost * 100.0f) / totalSamples;
    } else {
        *lossRate = 0.0f;
    }
}

/**
 * @brief Add sample to circular buffer (internal)
 */
static bool addSampleToBuffer(const IMUSample& sample) {
    // Check if buffer is full
    if (bufferCount >= CIRCULAR_BUFFER_SIZE) {
        return false;
    }

    // Add sample to buffer
    circularBuffer[bufferWriteIndex] = sample;
    bufferWriteIndex = (bufferWriteIndex + 1) % CIRCULAR_BUFFER_SIZE;
    bufferCount++;

    return true;
}

/**
 * @brief Get next sample from circular buffer (internal)
 */
static bool getNextSample(IMUSample* sample) {
    // Check if buffer is empty
    if (bufferCount == 0) {
        return false;
    }

    // Read sample from buffer
    *sample = circularBuffer[bufferReadIndex];
    bufferReadIndex = (bufferReadIndex + 1) % CIRCULAR_BUFFER_SIZE;
    bufferCount--;

    return true;
}
