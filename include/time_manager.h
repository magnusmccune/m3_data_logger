/**
 * @file time_manager.h
 * @brief Time management with GPS/millis fallback
 *
 * Provides unified time interface with automatic fallback from GPS to millis().
 * GPS provides accurate UTC time when available, otherwise uses millis() for
 * relative timing. Part of M3L-77 GPS time sync epic.
 *
 * Hardware: SparkFun SAM-M8Q GPS (GPS-15210), I2C address 0x42
 */

#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>

/**
 * @brief Time source enumeration
 */
enum TimeSource {
    TIME_SOURCE_GPS,        // GPS-derived UTC time (accurate)
    TIME_SOURCE_MILLIS      // millis() fallback (relative timing)
};

/**
 * @brief Initialize time manager
 *
 * Sets up baseline millis() timing. GPS integration will be added in M3L-79.
 */
void initTimeManager();

/**
 * @brief Update time manager state
 *
 * Should be called regularly in main loop to update GPS time and check lock status.
 * Currently a no-op, will poll GPS in M3L-79.
 */
void updateTime();

/**
 * @brief Get current timestamp in milliseconds
 *
 * Returns Unix epoch milliseconds when GPS locked, otherwise returns millis().
 * Use this for all timestamping in the application.
 *
 * @return Timestamp in milliseconds
 */
uint64_t getTimestampMs();

/**
 * @brief Get current timestamp in ISO-8601 format
 *
 * Returns formatted timestamp string like "2025-11-14T14:30:52Z".
 * Currently returns placeholder based on millis(), will use GPS time in M3L-79.
 *
 * @return ISO-8601 formatted timestamp string
 */
String getTimestampISO();

/**
 * @brief Get current time source
 *
 * Indicates whether timestamps are derived from GPS or millis().
 *
 * @return TIME_SOURCE_GPS if GPS locked, TIME_SOURCE_MILLIS otherwise
 */
TimeSource getCurrentTimeSource();

/**
 * @brief Check if GPS has valid time lock
 *
 * Returns false in baseline implementation, will check GPS status in M3L-79.
 *
 * @return true if GPS time is valid, false otherwise
 */
bool isGPSLocked();

#endif // TIME_MANAGER_H
