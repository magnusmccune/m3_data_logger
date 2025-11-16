/**
 * @file battery_manager.h
 * @brief MAX17048 LiPo fuel gauge driver for battery monitoring
 *
 * Hardware:
 * - MAX17048 fuel gauge (I2C address 0x36)
 * - Onboard on SparkFun DataLogger IoT (DEV-22462)
 * - Monitors single-cell LiPo battery (3.7V nominal, 4.2V max)
 *
 * Features:
 * - Voltage measurement (resolution: 1.25mV)
 * - State of charge (SOC) estimation (0-100%)
 * - Low battery threshold (15% default)
 * - QuickStart calibration for improved accuracy
 *
 * Usage:
 * 1. Call initBattery() in setup() after Wire.begin()
 * 2. Call getBatteryVoltage() / getBatteryPercentage() as needed
 * 3. Check isBatteryLow() before entering power-intensive operations
 * 4. Call logBatteryStatus() for debugging output
 *
 * Reference: M3L-83 Battery Optimization & Deep Sleep Implementation
 */

#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <Arduino.h>

// Battery thresholds
constexpr float BATTERY_LOW_THRESHOLD = 15.0;  // Percent
constexpr float BATTERY_CRITICAL_THRESHOLD = 5.0;  // Percent
constexpr float BATTERY_MIN_VOLTAGE = 3.0;  // Volts (safe cutoff)
constexpr float BATTERY_MAX_VOLTAGE = 4.2;  // Volts (fully charged)

/**
 * @brief Initialize MAX17048 fuel gauge
 * @return true if initialization successful, false otherwise
 *
 * Performs:
 * - I2C communication test at 0x36
 * - QuickStart calibration for accurate SOC
 * - Initial voltage and SOC reading
 *
 * Must be called after Wire.begin() in hardware init sequence.
 */
bool initBattery();

/**
 * @brief Get battery voltage in volts
 * @return Voltage (V), or -1.0 on error
 *
 * Resolution: 1.25mV per LSB
 * Typical range: 3.0V (empty) to 4.2V (full)
 */
float getBatteryVoltage();

/**
 * @brief Get battery state of charge percentage
 * @return SOC (0-100%), or -1.0 on error
 *
 * Uses MAX17048 internal algorithm with compensation.
 * Accuracy: ±1% typical, ±3% worst case
 */
float getBatteryPercentage();

/**
 * @brief Check if battery is below low threshold
 * @return true if SOC < BATTERY_LOW_THRESHOLD (15%)
 *
 * Use before starting power-intensive operations.
 */
bool isBatteryLow();

/**
 * @brief Check if battery is critically low
 * @return true if SOC < BATTERY_CRITICAL_THRESHOLD (5%)
 *
 * System should enter deep sleep or shut down if true.
 */
bool isBatteryCritical();

/**
 * @brief Log battery status to Serial
 *
 * Output format:
 * [BATTERY] Voltage: 3.85V | SOC: 67% | Status: OK
 */
void logBatteryStatus();

/**
 * @brief Get battery status as JSON-formatted string
 * @param buffer Output buffer (min 128 bytes recommended)
 * @param bufferSize Size of output buffer
 * @return Number of bytes written, or -1 on error
 *
 * Output format:
 * {"voltage": 3.85, "percentage": 67, "low": false, "critical": false}
 */
int getBatteryStatusJSON(char* buffer, size_t bufferSize);

#endif // BATTERY_MANAGER_H
