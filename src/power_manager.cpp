/**
 * @file power_manager.cpp
 * @brief ESP32 deep sleep power management implementation
 *
 * Reference: M3L-83 Battery Optimization & Deep Sleep Implementation
 */

#include "power_manager.h"
#include <SD_MMC.h>

// RTC memory (survives deep sleep)
RTC_DATA_ATTR RTCMemory rtcMem = {0};

void enterDeepSleep(uint8_t wakeupPin) {
    Serial.println("\n[POWER] Preparing for deep sleep...");

    // Flush and close SD card (CRITICAL: prevents corruption)
    Serial.println("[POWER] Closing SD card...");
    SD_MMC.end();
    delay(100);  // Allow SD card to fully close

    // Save current boot count
    rtcMem.bootCount++;
    rtcMem.validData = true;

    Serial.printf("[POWER] Boot count: %lu\n", rtcMem.bootCount);
    Serial.printf("[POWER] Configuring wakeup on GPIO%d (falling edge)...\n", wakeupPin);

    // Configure ext0 wakeup (button press, active LOW)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)wakeupPin, 0);  // 0 = LOW level

    // Optional: Timer wakeup (disabled for now)
    // esp_sleep_enable_timer_wakeup(10 * 1000000);  // 10 seconds

    Serial.println("[POWER] Entering deep sleep NOW");
    Serial.flush();  // Ensure message is printed
    delay(100);

    // Enter deep sleep (will not return until wakeup)
    esp_deep_sleep_start();
}

bool wasWokenByButton() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    return (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0);
}

const char* getWakeupReason() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            return "External interrupt (button press)";
        case ESP_SLEEP_WAKEUP_EXT1:
            return "External interrupt (RTC_GPIO)";
        case ESP_SLEEP_WAKEUP_TIMER:
            return "Timer wakeup";
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            return "Touchpad wakeup";
        case ESP_SLEEP_WAKEUP_ULP:
            return "ULP program wakeup";
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            return "First boot or reset";
    }
}

void saveStateToRTC(uint8_t state) {
    rtcMem.magic = RTC_MAGIC;
    rtcMem.lastState = state;
    rtcMem.validData = true;
}

uint8_t restoreStateFromRTC() {
    // Validate RTC data
    if (rtcMem.magic != RTC_MAGIC || !rtcMem.validData) {
        Serial.println("[POWER] RTC data invalid (first boot)");
        rtcMem.magic = RTC_MAGIC;
        rtcMem.bootCount = 0;
        rtcMem.lastState = 0;  // IDLE
        rtcMem.validData = true;
        return 0;  // Return IDLE
    }

    return rtcMem.lastState;
}

uint32_t getBootCount() {
    if (rtcMem.magic != RTC_MAGIC) {
        return 0;  // First boot
    }
    return rtcMem.bootCount;
}

void initPowerManager() {
    Serial.println("\n==== Power Manager Initialization ====");

    // Check wakeup cause
    const char* reason = getWakeupReason();
    Serial.printf("Wakeup reason: %s\n", reason);

    // Initialize or validate RTC memory
    if (rtcMem.magic != RTC_MAGIC) {
        Serial.println("First boot - initializing RTC memory");
        rtcMem.magic = RTC_MAGIC;
        rtcMem.bootCount = 0;
        rtcMem.lastState = 0;  // IDLE
        rtcMem.validData = true;
    } else {
        Serial.printf("Boot count: %lu\n", rtcMem.bootCount);
    }

    if (wasWokenByButton()) {
        Serial.println("✓ Woken by button press - proceeding to AWAITING_QR");
    }

    Serial.println();
}
