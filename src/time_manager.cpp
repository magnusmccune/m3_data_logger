/**
 * @file time_manager.cpp
 * @brief Time management implementation
 *
 * Provides unified time interface with GPS/millis fallback. GPS provides
 * accurate UTC time when available, otherwise uses millis() for relative timing.
 */

#include "time_manager.h"
#include "hardware_init.h"  // For access to global gps object

// ===== Internal State =====

static bool gpsAvailable = false;        // GPS hardware detected during init
static bool gpsLocked = false;           // GPS currently has valid time lock
static uint64_t lastGPSTime = 0;         // Last valid GPS time in Unix epoch ms
static uint32_t lastGPSUpdate = 0;       // millis() when GPS time was last updated
static TimeSource currentSource = TIME_SOURCE_MILLIS;  // Current time source

// ===== Initialization =====

void initTimeManager() {
    Serial.println("[TIME] Time manager initialized");
    Serial.println("[TIME] GPS integration active (M3L-79)");

    // GPS availability will be determined on first updateTime() call
    // This allows GPS to be initialized after time_manager
}

// ===== Time Update (GPS Polling) =====

void updateTime() {
    // Check if GPS is available on first call
    static bool firstCall = true;
    if (firstCall) {
        // Try to get GPS protocol version to verify GPS is available
        // This is a lightweight check that doesn't block
        if (gps.getProtocolVersionHigh() > 0) {
            gpsAvailable = true;
            Serial.println("[TIME] GPS hardware detected");
        } else {
            gpsAvailable = false;
            Serial.println("[TIME] GPS not available, using millis() fallback");
        }
        firstCall = false;
    }

    // If GPS not available, use millis() fallback
    if (!gpsAvailable) {
        currentSource = TIME_SOURCE_MILLIS;
        return;
    }

    // Poll GPS for new PVT data (non-blocking)
    // getPVT() returns true if new data is available
    if (gps.getPVT()) {
        // Check fix type - we need at least time-only fix (type 5) or better
        uint8_t fixType = gps.getFixType();
        uint8_t satellites = gps.getSIV();

        // Fix types: 0=none, 1=dead reckoning, 2=2D, 3=3D, 4=GNSS+DR, 5=time-only
        // We accept 2D, 3D, GNSS+DR, or time-only (types 2-5)
        bool hasValidFix = (fixType >= 2 && fixType <= 5);

        // Also verify time is valid with confirmedTime flag
        bool timeValid = gps.getTimeValid();

        if (hasValidFix && timeValid && satellites >= 3) {
            // Extract GPS time components
            uint16_t year = gps.getYear();
            uint8_t month = gps.getMonth();
            uint8_t day = gps.getDay();
            uint8_t hour = gps.getHour();
            uint8_t minute = gps.getMinute();
            uint8_t second = gps.getSecond();
            uint32_t nanosecond = gps.getNanosecond();

            // Convert to Unix epoch milliseconds
            // GPS time is UTC, which aligns with Unix epoch
            lastGPSTime = convertToUnixEpochMs(year, month, day, hour, minute, second, nanosecond);
            lastGPSUpdate = millis();

            // Update lock status
            if (!gpsLocked) {
                Serial.print("[TIME] GPS lock acquired! ");
                Serial.print(satellites);
                Serial.println(" satellites");
                gpsLocked = true;
            }

            currentSource = TIME_SOURCE_GPS;

        } else {
            // Lost lock or invalid time
            if (gpsLocked) {
                Serial.print("[TIME] GPS lock lost (fix type: ");
                Serial.print(fixType);
                Serial.print(", satellites: ");
                Serial.print(satellites);
                Serial.println(")");
                gpsLocked = false;
            }

            currentSource = TIME_SOURCE_MILLIS;
        }
    }
    // If getPVT() returns false, no new data available - use last known state
}

// ===== Helper: Convert GPS time to Unix epoch milliseconds =====

static uint64_t convertToUnixEpochMs(uint16_t year, uint8_t month, uint8_t day,
                                      uint8_t hour, uint8_t minute, uint8_t second,
                                      uint32_t nanosecond) {
    // Calculate days since Unix epoch (1970-01-01)
    // This is a simplified calculation that works for dates after 2000

    // Days in each month (non-leap year)
    const uint16_t daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // Calculate total days
    uint32_t totalDays = 0;

    // Add days for complete years since 1970
    for (uint16_t y = 1970; y < year; y++) {
        bool isLeap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
        totalDays += isLeap ? 366 : 365;
    }

    // Add days for complete months in current year
    for (uint8_t m = 1; m < month; m++) {
        totalDays += daysInMonth[m];
        // Add leap day if February in a leap year
        if (m == 2) {
            bool isLeap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
            if (isLeap) totalDays++;
        }
    }

    // Add days in current month (subtract 1 because day 1 = 0 days elapsed)
    totalDays += (day - 1);

    // Convert to seconds
    uint64_t totalSeconds = (uint64_t)totalDays * 86400ULL;  // 86400 seconds per day
    totalSeconds += (uint64_t)hour * 3600ULL;
    totalSeconds += (uint64_t)minute * 60ULL;
    totalSeconds += (uint64_t)second;

    // Convert to milliseconds and add nanosecond contribution
    uint64_t totalMs = totalSeconds * 1000ULL;
    totalMs += (nanosecond / 1000000UL);  // Convert nanoseconds to milliseconds

    return totalMs;
}

// ===== Public Interface Functions =====

uint64_t getTimestampMs() {
    if (currentSource == TIME_SOURCE_GPS && gpsLocked) {
        // Return GPS time adjusted for elapsed millis() since last GPS update
        // This provides accurate timestamps even between GPS updates (1Hz)
        uint32_t elapsed = millis() - lastGPSUpdate;
        return lastGPSTime + (uint64_t)elapsed;
    } else {
        // Fallback to millis() for relative timing
        return (uint64_t)millis();
    }
}

String getTimestampISO() {
    if (currentSource == TIME_SOURCE_GPS && gpsLocked) {
        // Convert GPS Unix epoch to ISO-8601 format: "2025-11-14T14:30:52.123Z"
        uint64_t timestamp = getTimestampMs();
        uint64_t totalSeconds = timestamp / 1000;
        uint32_t milliseconds = timestamp % 1000;

        // Calculate date/time components from Unix epoch
        // This is a reverse of convertToUnixEpochMs()
        uint32_t days = totalSeconds / 86400;
        uint32_t secondsInDay = totalSeconds % 86400;

        uint32_t hour = secondsInDay / 3600;
        uint32_t minute = (secondsInDay % 3600) / 60;
        uint32_t second = secondsInDay % 60;

        // Calculate year, month, day
        uint16_t year = 1970;
        uint32_t daysRemaining = days;

        while (true) {
            bool isLeap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
            uint32_t daysInYear = isLeap ? 366 : 365;

            if (daysRemaining >= daysInYear) {
                daysRemaining -= daysInYear;
                year++;
            } else {
                break;
            }
        }

        // Calculate month and day
        const uint16_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        bool isLeap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));

        uint8_t month = 1;
        for (month = 1; month <= 12; month++) {
            uint32_t daysThisMonth = daysInMonth[month - 1];
            if (month == 2 && isLeap) daysThisMonth = 29;

            if (daysRemaining >= daysThisMonth) {
                daysRemaining -= daysThisMonth;
            } else {
                break;
            }
        }

        uint8_t day = daysRemaining + 1;

        // Format as ISO-8601: "2025-11-14T14:30:52.123Z"
        char isoBuffer[32];
        snprintf(isoBuffer, sizeof(isoBuffer),
                 "%04u-%02u-%02uT%02u:%02u:%02u.%03luZ",
                 year, month, day, hour, minute, second,
                 (unsigned long)milliseconds);

        return String(isoBuffer);

    } else {
        // Fallback to millis() placeholder format
        uint64_t timestamp = getTimestampMs();
        uint32_t seconds = timestamp / 1000;
        uint32_t milliseconds = timestamp % 1000;

        char isoBuffer[64];
        snprintf(isoBuffer, sizeof(isoBuffer),
                 "millis_%lu.%03lu",
                 (unsigned long)seconds,
                 (unsigned long)milliseconds);

        return String(isoBuffer);
    }
}

TimeSource getCurrentTimeSource() {
    return currentSource;
}

bool isGPSLocked() {
    return gpsLocked;
}
