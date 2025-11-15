/**
 * @file storage_manager.cpp
 * @brief SD card storage management implementation
 *
 * Implements session-based CSV logging with buffered writes and periodic
 * fsync for durability. Handles SD card errors gracefully.
 */

#include "storage_manager.h"
#include "time_manager.h"
#include <SD_MMC.h>
#include <time.h>
#include <ArduinoJson.h>

// Session state
static File sessionFile;
static bool sessionActive = false;
static char currentFilename[128];
static char currentSessionID[32];
static char currentTestID[9];         // 8 chars + null terminator
static char currentDescription[65];   // 64 chars + null terminator
static char currentLabels[MAX_LABELS][MAX_LABEL_LENGTH];
static uint8_t currentLabelCount = 0;
static uint32_t sessionStartTime = 0;
static uint32_t sessionStartTimestamp = 0;
static uint32_t lastFsyncTime = 0;
static uint32_t samplesWritten = 0;

// Write buffer
static IMUSample writeBuffer[WRITE_BUFFER_SIZE];
static uint8_t bufferCount = 0;

// Forward declarations
static bool flushBuffer();
static bool createSessionFile(const char* testID);
static bool writeCSVHeader();
static void generateSessionID(char* sessionID, size_t size);
static bool writeMetadataEntry(uint32_t sessionDuration, float avgRate);

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
bool startSession(const char* testID, const char* description, const char* labels[], uint8_t labelCount) {
    if (sessionActive) {
        Serial.println("[Storage] ERROR: Session already active");
        return false;
    }

    if (!testID || !description || labelCount == 0 || labelCount > MAX_LABELS) {
        Serial.println("[Storage] ERROR: Invalid session parameters");
        return false;
    }

    Serial.println("[Storage] Starting new session...");

    // Generate unique session ID
    generateSessionID(currentSessionID, sizeof(currentSessionID));

    // Store test metadata globally for CSV rows and metadata.json
    strncpy(currentTestID, testID, sizeof(currentTestID) - 1);
    currentTestID[sizeof(currentTestID) - 1] = '\0';

    strncpy(currentDescription, description, sizeof(currentDescription) - 1);
    currentDescription[sizeof(currentDescription) - 1] = '\0';

    // Copy labels
    currentLabelCount = labelCount;
    for (uint8_t i = 0; i < labelCount; i++) {
        strncpy(currentLabels[i], labels[i], MAX_LABEL_LENGTH - 1);
        currentLabels[i][MAX_LABEL_LENGTH - 1] = '\0';
    }

    // Create CSV file (now uses test_id instead of test name)
    if (!createSessionFile(testID)) {
        return false;
    }

    // Write CSV header (clean column headers only, no metadata)
    if (!writeCSVHeader()) {
        sessionFile.close();
        return false;
    }

    // Initialize session state
    sessionActive = true;
    sessionStartTime = millis();
    sessionStartTimestamp = getTimestampMs();  // GPS or millis() fallback
    lastFsyncTime = millis();
    samplesWritten = 0;
    bufferCount = 0;

    // Log time source status
    TimeSource timeSource = getCurrentTimeSource();
    bool gpsLocked = isGPSLocked();
    const char* sourceStr = (timeSource == TIME_SOURCE_GPS) ? "GPS" : "millis";

    Serial.printf("[Storage] Session started: %s\n", currentSessionID);
    Serial.printf("[Storage] Test ID: %s\n", currentTestID);
    Serial.printf("[Storage] File: %s\n", currentFilename);
    Serial.printf("[Storage] Time source: %s (GPS locked: %s)\n",
                  sourceStr, gpsLocked ? "true" : "false");

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

    // Calculate session statistics
    uint32_t sessionDuration = millis() - sessionStartTime;
    float durationSeconds = sessionDuration / 1000.0f;
    float avgRate = (samplesWritten * 1000.0f) / sessionDuration;

    // Final sync to disk
    sessionFile.flush();
    sessionFile.close();

    // Write session metadata to metadata.json
    if (!writeMetadataEntry(sessionDuration, avgRate)) {
        Serial.println("[Storage] WARNING: Failed to write metadata, but CSV is saved");
    }

    // Print session summary
    Serial.println("\n=== Session Complete ===");
    Serial.printf("Session ID: %s\n", currentSessionID);
    Serial.printf("Test ID: %s\n", currentTestID);
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

        // Get GPS timestamp (Unix epoch ms) or millis() fallback
        uint64_t timestamp = getTimestampMs();

        // Write CSV row: test_id,timestamp_ms,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z
        int written = sessionFile.printf("%s,%llu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                                          currentTestID,
                                          timestamp,
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
static bool createSessionFile(const char* testID) {
    // Build filename: /data/<session_id>_<test_id>.csv
    snprintf(currentFilename, sizeof(currentFilename),
             "/data/%s_%s.csv", currentSessionID, testID);

    // Open file for writing
    sessionFile = SD_MMC.open(currentFilename, FILE_WRITE);
    if (!sessionFile) {
        Serial.printf("[Storage] ERROR: Failed to create file: %s\n", currentFilename);
        return false;
    }

    return true;
}

/**
 * @brief Write CSV header (internal)
 */
static bool writeCSVHeader() {
    // Write clean column headers only (no metadata comments)
    sessionFile.println("test_id,timestamp_ms,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z");

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

/**
 * @brief Write session metadata to metadata.json (internal)
 */
static bool writeMetadataEntry(uint32_t sessionDuration, float avgRate) {
    const char* metadataPath = "/data/metadata.json";
    StaticJsonDocument<4096> doc;

    // Try to load existing metadata.json
    File metaFile = SD_MMC.open(metadataPath, FILE_READ);
    if (metaFile) {
        DeserializationError error = deserializeJson(doc, metaFile);
        metaFile.close();
        if (error) {
            Serial.printf("[Storage] WARNING: Failed to parse existing metadata.json: %s\n", error.c_str());
            // Continue with empty document
        }
    }

    // Ensure "sessions" array exists
    if (!doc.containsKey("sessions")) {
        doc.createNestedArray("sessions");
    }

    JsonArray sessions = doc["sessions"];
    JsonObject session = sessions.createNestedObject();

    // Add session metadata
    session["session_id"] = currentSessionID;
    session["test_id"] = currentTestID;
    session["description"] = currentDescription;

    // Add labels array
    JsonArray labels = session.createNestedArray("labels");
    for (uint8_t i = 0; i < currentLabelCount; i++) {
        labels.add(currentLabels[i]);
    }

    // Add timestamps and time source info
    TimeSource timeSource = getCurrentTimeSource();
    bool gpsLocked = isGPSLocked();

    // Use ISO 8601 format for start_time when available
    String startTimeISO = getTimestampISO();
    session["start_time"] = startTimeISO;

    session["duration_ms"] = sessionDuration;
    session["samples"] = samplesWritten;
    session["actual_rate_hz"] = avgRate;
    session["filename"] = currentFilename;

    // Add time source metadata (M3L-81)
    session["time_source"] = (timeSource == TIME_SOURCE_GPS) ? "gps" : "millis";
    session["gps_locked"] = gpsLocked;

    // Write updated metadata back to file
    metaFile = SD_MMC.open(metadataPath, FILE_WRITE);
    if (!metaFile) {
        Serial.println("[Storage] ERROR: Failed to open metadata.json for writing");
        return false;
    }

    if (serializeJson(doc, metaFile) == 0) {
        Serial.println("[Storage] ERROR: Failed to write metadata.json");
        metaFile.close();
        return false;
    }

    metaFile.close();
    Serial.println("[Storage] Metadata written to metadata.json");
    return true;
}
