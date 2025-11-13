/**
 * @file storage_manager.h
 * @brief SD card storage management for CSV data logging
 *
 * Handles session-based CSV file creation with metadata headers, buffered
 * writes for performance, and error handling for SD card issues.
 *
 * File format: /data/YYYYMMDD_HHMMSS_<test_name>.csv
 */

#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include "sensor_manager.h"

// Storage configuration constants
constexpr uint8_t WRITE_BUFFER_SIZE = 20;       // Match sensor circular buffer
constexpr uint32_t FSYNC_INTERVAL_MS = 5000;    // Periodic fsync for durability
constexpr uint8_t MAX_LABELS = 10;              // Max metadata labels
constexpr uint8_t MAX_LABEL_LENGTH = 32;        // Max chars per label
constexpr uint8_t MAX_TEST_NAME_LENGTH = 64;    // Max test name chars

/**
 * @brief Initialize storage manager
 *
 * Verifies SD card is mounted and creates /data directory if needed.
 *
 * @return true if SD card ready, false otherwise
 */
bool initializeStorage();

/**
 * @brief Start a new recording session
 *
 * Creates CSV file with unique timestamp-based session ID and metadata header.
 * Format: /data/YYYYMMDD_HHMMSS_<test_name>.csv
 *
 * @param testName Test name from QR metadata (1-64 chars)
 * @param labels Array of label strings from QR metadata
 * @param labelCount Number of labels (1-10)
 * @return true if session started successfully, false otherwise
 */
bool startSession(const char* testID, const char* description, const char* labels[], uint8_t labelCount);;

/**
 * @brief Write a single IMU sample to the session file
 *
 * Samples are buffered and written in batches for performance. Automatically
 * handles fsync every 5 seconds for durability.
 *
 * @param sample IMU sample to write
 * @return true if write successful, false on error
 */
bool writeSample(const IMUSample& sample);

/**
 * @brief End the current recording session
 *
 * Flushes remaining buffered samples, syncs file to disk, and closes file.
 * Prints session summary to serial.
 *
 * @return true if session ended cleanly, false on error
 */
bool endSession();

/**
 * @brief Check if session is currently active
 *
 * @return true if session active, false otherwise
 */
bool isSessionActive();

/**
 * @brief Get session statistics
 *
 * @param samplesWritten Pointer to store total samples written
 * @param sessionDuration Pointer to store session duration in ms
 * @param filename Pointer to buffer for filename (min 128 chars)
 */
void getSessionStats(uint32_t* samplesWritten, uint32_t* sessionDuration, char* filename);

#endif // STORAGE_MANAGER_H
