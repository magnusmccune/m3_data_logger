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
 * Sets up time management system with GPS/millis fallback support.
 */
void initTimeManager();

/**
 * @brief Update time manager state
 *
 * Should be called regularly in main loop to poll GPS and update lock status.
 * Polls GPS for new PVT data (Position/Velocity/Time) at 1Hz.
 * Automatically switches between GPS and millis() based on lock status.
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
 * Returns formatted timestamp string like "2025-11-14T14:30:52.123Z" when GPS locked,
 * or "millis_<seconds>.<milliseconds>" when using millis() fallback.
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
 * Returns true when GPS has acquired satellite lock and time is valid.
 * Requires fix type 2-5 (2D/3D/GNSS+DR/time-only) and 3+ satellites.
 *
 * @return true if GPS time is valid, false otherwise
 */
bool isGPSLocked();

#endif // TIME_MANAGER_H
