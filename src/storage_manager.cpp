/**
 * @file storage_manager.cpp
 * @brief SD card storage management implementation
 *
 * Implements session-based CSV logging with buffered writes and periodic
 * fsync for durability. Handles SD card errors gracefully.
 */

#include "storage_manager.h"
#include <SD_MMC.h>
#include <time.h>

// Session state
static File sessionFile;
static bool sessionActive = false;
static char currentFilename[128];
static char currentSessionID[32];
static uint32_t sessionStartTime = 0;
static uint32_t sessionStartTimestamp = 0;
static uint32_t lastFsyncTime = 0;
static uint32_t samplesWritten = 0;

// Write buffer
static IMUSample writeBuffer[WRITE_BUFFER_SIZE];
static uint8_t bufferCount = 0;

// Forward declarations
static bool flushBuffer();
static bool createSessionFile(const char* testName);
static bool writeCSVHeader(const char* testName, const char* labels[], uint8_t labelCount);
static void generateSessionID(char* sessionID, size_t size);

/**
 * @brief Initialize storage manager
 */
bool initializeStorage() {
    Serial.println("[Storage] Checking SD card...");

    // Verify SD card is mounted (should already be done in hardware_init)
    if (!SD_MMC.begin("/sdcard", true)) {
        Serial.println("[Storage] ERROR: SD card not mounted");
        return false;
    }

    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("[Storage] SD card size: %llu MB\n", cardSize);

    // Create /data directory if it doesn't exist
    if (!SD_MMC.exists("/data")) {
        if (!SD_MMC.mkdir("/data")) {
            Serial.println("[Storage] ERROR: Failed to create /data directory");
            return false;
        }
        Serial.println("[Storage] Created /data directory");
    }

    Serial.println("[Storage] Storage manager initialized");
    return true;
}

/**
 * @brief Start a new recording session
 */
bool startSession(const char* testName, const char* labels[], uint8_t labelCount) {
    if (sessionActive) {
        Serial.println("[Storage] ERROR: Session already active");
        return false;
    }

    if (!testName || labelCount == 0 || labelCount > MAX_LABELS) {
        Serial.println("[Storage] ERROR: Invalid session parameters");
        return false;
    }

    Serial.println("[Storage] Starting new session...");

    // Generate unique session ID
    generateSessionID(currentSessionID, sizeof(currentSessionID));

    // Create CSV file
    if (!createSessionFile(testName)) {
        return false;
    }

    // Write CSV header with metadata
    if (!writeCSVHeader(testName, labels, labelCount)) {
        sessionFile.close();
        return false;
    }

    // Initialize session state
    sessionActive = true;
    sessionStartTime = millis();
    sessionStartTimestamp = millis();  // Could use RTC if available
    lastFsyncTime = millis();
    samplesWritten = 0;
    bufferCount = 0;

    Serial.printf("[Storage] Session started: %s\n", currentSessionID);
    Serial.printf("[Storage] File: %s\n", currentFilename);

    return true;
}

/**
 * @brief Write a single IMU sample to the session file
 */
bool writeSample(const IMUSample& sample) {
    if (!sessionActive) {
        Serial.println("[Storage] ERROR: No active session");
        return false;
    }

    // Add sample to buffer
    writeBuffer[bufferCount] = sample;
    bufferCount++;

    // Flush buffer if full
    if (bufferCount >= WRITE_BUFFER_SIZE) {
        if (!flushBuffer()) {
            return false;
        }
    }

    // Periodic fsync for durability
    uint32_t currentTime = millis();
    if (currentTime - lastFsyncTime >= FSYNC_INTERVAL_MS) {
        // Flush any pending samples first
        if (bufferCount > 0) {
            if (!flushBuffer()) {
                return false;
            }
        }

        // Sync to disk
        sessionFile.flush();
        lastFsyncTime = currentTime;
        Serial.println("[Storage] Periodic fsync completed");
    }

    return true;
}

/**
 * @brief End the current recording session
 */
bool endSession() {
    if (!sessionActive) {
        Serial.println("[Storage] WARNING: No active session to end");
        return true;
    }

    Serial.println("[Storage] Ending session...");

    // Flush any remaining buffered samples
    if (bufferCount > 0) {
        if (!flushBuffer()) {
            Serial.println("[Storage] ERROR: Failed to flush final buffer");
            sessionActive = false;
            sessionFile.close();
            return false;
        }
    }

    // Final sync to disk
    sessionFile.flush();
    sessionFile.close();

    // Calculate session statistics
    uint32_t sessionDuration = millis() - sessionStartTime;
    float durationSeconds = sessionDuration / 1000.0f;
    float avgRate = (samplesWritten * 1000.0f) / sessionDuration;

    // Print session summary
    Serial.println("\n=== Session Complete ===");
    Serial.printf("Session ID: %s\n", currentSessionID);
    Serial.printf("Duration: %.1f seconds\n", durationSeconds);
    Serial.printf("Samples: %u (%.1f Hz avg)\n", samplesWritten, avgRate);
    Serial.printf("File: %s\n", currentFilename);
    Serial.println("=========================\n");

    sessionActive = false;
    return true;
}

/**
 * @brief Check if session is currently active
 */
bool isSessionActive() {
    return sessionActive;
}

/**
 * @brief Get session statistics
 */
void getSessionStats(uint32_t* samplesWritten_out, uint32_t* sessionDuration, char* filename) {
    if (samplesWritten_out) {
        *samplesWritten_out = samplesWritten;
    }

    if (sessionDuration) {
        *sessionDuration = millis() - sessionStartTime;
    }

    if (filename) {
        strncpy(filename, currentFilename, 127);
        filename[127] = '\0';
    }
}

/**
 * @brief Flush write buffer to SD card (internal)
 */
static bool flushBuffer() {
    if (bufferCount == 0) {
        return true;
    }

    // Write each sample as CSV row
    for (uint8_t i = 0; i < bufferCount; i++) {
        const IMUSample& sample = writeBuffer[i];

        // Calculate relative timestamp from session start
        uint32_t relativeTime = sample.timestamp_ms - sessionStartTimestamp;

        // Write CSV row: timestamp_ms,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z
        int written = sessionFile.printf("%lu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                                          relativeTime,
                                          sample.accel_x, sample.accel_y, sample.accel_z,
                                          sample.gyro_x, sample.gyro_y, sample.gyro_z);

        if (written <= 0) {
            Serial.println("[Storage] ERROR: Failed to write sample to SD card");
            return false;
        }

        samplesWritten++;
    }

    bufferCount = 0;
    return true;
}

/**
 * @brief Create session CSV file (internal)
 */
static bool createSessionFile(const char* testName) {
    // Build filename: /data/YYYYMMDD_HHMMSS_<test_name>.csv
    // For now, use session ID (timestamp-based) instead of full datetime
    snprintf(currentFilename, sizeof(currentFilename),
             "/data/%s_%s.csv", currentSessionID, testName);

    // Sanitize ONLY the test name part (after "/data/<session_id>_")
    // Find the start of the test name (after the last underscore before .csv)
    size_t testNameStart = strlen("/data/") + strlen(currentSessionID) + 1; // +1 for underscore
    for (size_t i = testNameStart; currentFilename[i] && currentFilename[i] != '.'; i++) {
        if (currentFilename[i] == ' ' || currentFilename[i] == '/' ||
            currentFilename[i] == '\\' || currentFilename[i] == ':' ||
            currentFilename[i] == '*' || currentFilename[i] == '?' ||
            currentFilename[i] == '"' || currentFilename[i] == '<' ||
            currentFilename[i] == '>' || currentFilename[i] == '|' ||
            currentFilename[i] < 32) {
            currentFilename[i] = '_';
        }
    }

    // Open file for writing
    sessionFile = SD_MMC.open(currentFilename, FILE_WRITE);
    if (!sessionFile) {
        Serial.printf("[Storage] ERROR: Failed to create file: %s\n", currentFilename);
        return false;
    }

    return true;
}

/**
 * @brief Write CSV header with metadata (internal)
 */
static bool writeCSVHeader(const char* testName, const char* labels[], uint8_t labelCount) {
    // Write metadata comments
    sessionFile.printf("# Session ID: %s\n", currentSessionID);
    sessionFile.printf("# Test: %s\n", testName);

    // Write labels as comma-separated list
    sessionFile.print("# Labels: ");
    for (uint8_t i = 0; i < labelCount; i++) {
        sessionFile.print(labels[i]);
        if (i < labelCount - 1) {
            sessionFile.print(",");
        }
    }
    sessionFile.println();

    // Write start time (Unix timestamp if RTC available, or millis())
    sessionFile.printf("# Start Time: %lu\n", sessionStartTimestamp);

    // Write column headers
    sessionFile.println("timestamp_ms,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z");

    return true;
}

/**
 * @brief Generate unique session ID (internal)
 */
static void generateSessionID(char* sessionID, size_t size) {
    // Use millis() as a simple timestamp-based ID
    // Format: YYYYMMDD_HHMMSS (simplified: just use millis for now)
    uint32_t timestamp = millis();

    // Simple timestamp-based ID (in production, would use RTC)
    snprintf(sessionID, size, "%010lu", timestamp);
}
