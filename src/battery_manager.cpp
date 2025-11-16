/**
 * @file battery_manager.cpp
 * @brief MAX17048 fuel gauge implementation
 *
 * Reference: M3L-83 Battery Optimization & Deep Sleep Implementation
 */

#include "battery_manager.h"
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>

// Global fuel gauge instance
SFE_MAX1704X fuelGauge(MAX1704X_MAX17048);

// Initialization flag
static bool batteryInitialized = false;

bool initBattery() {
    Serial.println("[BATTERY] Initializing MAX17048 fuel gauge...");

    // Begin I2C communication (0x36)
    if (!fuelGauge.begin()) {
        Serial.println("[BATTERY] ERROR: Failed to communicate with MAX17048");
        Serial.println("[BATTERY]        Check I2C connection at address 0x36");
        batteryInitialized = false;
        return false;
    }

    Serial.println("[BATTERY] MAX17048 detected");

    // QuickStart: Force fuel gauge to restart calculations
    // Improves SOC accuracy after power-on
    fuelGauge.quickStart();
    delay(200);  // Allow QuickStart to complete

    // Read initial values
    float voltage = fuelGauge.getVoltage();
    float soc = fuelGauge.getSOC();

    if (voltage < 0 || soc < 0) {
        Serial.println("[BATTERY] ERROR: Failed to read initial values");
        batteryInitialized = false;
        return false;
    }

    Serial.printf("[BATTERY] Initial voltage: %.2fV\n", voltage);
    Serial.printf("[BATTERY] Initial SOC: %.1f%%\n", soc);

    // Check for critically low battery
    if (soc < BATTERY_CRITICAL_THRESHOLD) {
        Serial.println("[BATTERY] WARNING: Battery critically low!");
    } else if (soc < BATTERY_LOW_THRESHOLD) {
        Serial.println("[BATTERY] WARNING: Battery low");
    } else {
        Serial.println("[BATTERY] Initialized successfully");
    }

    batteryInitialized = true;
    return true;
}

float getBatteryVoltage() {
    if (!batteryInitialized) {
        Serial.println("[BATTERY] ERROR: Not initialized");
        return -1.0;
    }

    float voltage = fuelGauge.getVoltage();

    // Sanity check (LiPo voltage range: 2.8V - 4.3V)
    if (voltage < 2.8 || voltage > 4.3) {
        Serial.printf("[BATTERY] WARNING: Voltage out of range: %.2fV\n", voltage);
        return -1.0;
    }

    return voltage;
}

float getBatteryPercentage() {
    if (!batteryInitialized) {
        Serial.println("[BATTERY] ERROR: Not initialized");
        return -1.0;
    }

    float soc = fuelGauge.getSOC();

    // Sanity check
    if (soc < 0 || soc > 100) {
        Serial.printf("[BATTERY] WARNING: SOC out of range: %.1f%%\n", soc);
        return -1.0;
    }

    return soc;
}

bool isBatteryLow() {
    float soc = getBatteryPercentage();
    if (soc < 0) {
        return false;  // Cannot determine, assume OK
    }
    return soc < BATTERY_LOW_THRESHOLD;
}

bool isBatteryCritical() {
    float soc = getBatteryPercentage();
    if (soc < 0) {
        return false;  // Cannot determine, assume OK
    }
    return soc < BATTERY_CRITICAL_THRESHOLD;
}

void logBatteryStatus() {
    if (!batteryInitialized) {
        Serial.println("[BATTERY] Not initialized");
        return;
    }

    float voltage = getBatteryVoltage();
    float soc = getBatteryPercentage();

    if (voltage < 0 || soc < 0) {
        Serial.println("[BATTERY] ERROR: Failed to read status");
        return;
    }

    const char* status;
    if (soc < BATTERY_CRITICAL_THRESHOLD) {
        status = "CRITICAL";
    } else if (soc < BATTERY_LOW_THRESHOLD) {
        status = "LOW";
    } else {
        status = "OK";
    }

    Serial.printf("[BATTERY] Voltage: %.2fV | SOC: %.1f%% | Status: %s\n",
                  voltage, soc, status);
}

int getBatteryStatusJSON(char* buffer, size_t bufferSize) {
    if (!batteryInitialized) {
        return -1;
    }

    float voltage = getBatteryVoltage();
    float soc = getBatteryPercentage();

    if (voltage < 0 || soc < 0) {
        return -1;
    }

    bool low = isBatteryLow();
    bool critical = isBatteryCritical();

    int written = snprintf(buffer, bufferSize,
        "{\"voltage\":%.2f,\"percentage\":%.1f,\"low\":%s,\"critical\":%s}",
        voltage, soc,
        low ? "true" : "false",
        critical ? "true" : "false");

    if (written >= (int)bufferSize) {
        Serial.println("[BATTERY] WARNING: JSON output truncated");
        return -1;
    }

    return written;
}
