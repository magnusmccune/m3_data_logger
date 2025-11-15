/**
 * @file time_manager.cpp
 * @brief Time management implementation
 *
 * Baseline implementation using millis() fallback. GPS integration will be
 * added in M3L-79 to provide accurate UTC timestamps when available.
 */

#include "time_manager.h"

// Baseline implementation - GPS will be integrated in M3L-79
void initTimeManager() {
    Serial.println("[TIME] Time manager initialized (millis baseline)");
    Serial.println("[TIME] GPS integration pending in M3L-79");
}

void updateTime() {
    // No-op in baseline implementation
    // Will poll GPS and update lock status in M3L-79
}

uint64_t getTimestampMs() {
    // Baseline: return millis() as relative timestamp
    // M3L-79: return GPS Unix epoch ms when locked, else millis()
    return (uint64_t)millis();
}

String getTimestampISO() {
    // Baseline: placeholder format using millis()
    // M3L-79: convert GPS UTC time to ISO-8601 when locked

    uint64_t timestamp = getTimestampMs();
    uint32_t seconds = timestamp / 1000;
    uint32_t milliseconds = timestamp % 1000;

    // Placeholder format: "millis_<seconds>.<milliseconds>"
    // This will be replaced with proper ISO-8601 UTC in M3L-79
    char isoBuffer[64];
    snprintf(isoBuffer, sizeof(isoBuffer),
             "millis_%lu.%03lu",
             (unsigned long)seconds,
             (unsigned long)milliseconds);

    return String(isoBuffer);
}

TimeSource getCurrentTimeSource() {
    // Baseline: always return MILLIS
    // M3L-79: return GPS when locked, MILLIS otherwise
    return TIME_SOURCE_MILLIS;
}

bool isGPSLocked() {
    // Baseline: GPS not integrated yet
    // M3L-79: check GPS fix status
    return false;
}
