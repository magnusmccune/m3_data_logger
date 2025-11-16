/**
 * @file power_manager.h
 * @brief ESP32 deep sleep power management for battery optimization
 *
 * Power Consumption Targets:
 * - IDLE (active): ~30mA (GPS polling + ESP32 idle)
 * - IDLE (deep sleep): <1mA (ESP32 + peripherals powered down)
 * - RECORDING: ~50-60mA (IMU + GPS + SD active)
 *
 * Deep Sleep Strategy:
 * - Enter deep sleep after 5 seconds in IDLE state (no button press)
 * - Wakeup via ext0 on button GPIO (falling edge)
 * - Preserve state in RTC memory (survives deep sleep)
 * - Re-initialize peripherals on wakeup
 *
 * Hardware Considerations:
 * - Button interrupt on GPIO33 (BUTTON_INT_PIN)
 * - SD card must be properly closed before sleep (SD_MMC.end())
 * - GPS will require cold start after wake (~30 seconds)
 * - I2C devices retain state, but need re-init
 *
 * Battery Life Improvement:
 * - Before: 30mA × 24h = 720mAh/day (3 days on 2000mAh battery)
 * - After: 1mA × 24h = 24mAh/day (21+ days on 2000mAh battery)
 * - Assumes 95% time in IDLE, 5% time in RECORDING
 *
 * Reference: M3L-83 Battery Optimization & Deep Sleep Implementation
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include "esp_sleep.h"

// Deep sleep configuration
constexpr uint32_t IDLE_TIMEOUT_MS = 5000;  // Enter deep sleep after 5 seconds in IDLE

// RTC memory structure (survives deep sleep, max 8KB)
// Keep minimal to reduce power consumption
typedef struct {
    uint32_t magic;           // Validation marker (0xDEADBEEF)
    uint32_t bootCount;       // Number of wakeups
    uint8_t lastState;        // SystemState before sleep (for recovery)
    bool validData;           // true if RTC data is valid
} RTC_DATA_ATTR RTCMemory;

// Magic number for RTC data validation
constexpr uint32_t RTC_MAGIC = 0xDEADBEEF;

/**
 * @brief Enter ESP32 deep sleep mode
 *
 * Performs:
 * 1. Flush SD card buffers and close file handles
 * 2. Save state machine state to RTC memory
 * 3. Configure ext0 wakeup on button GPIO (falling edge)
 * 4. Enter deep sleep (current: ~10µA, target: <1mA with peripherals)
 *
 * CRITICAL: Must call flushBuffer() and endSession() before this
 * function to prevent data loss.
 *
 * Wakeup sources:
 * - Button press on GPIO33 (ext0, LOW level)
 * - Timer wakeup (optional, disabled for now)
 *
 * @param wakeupPin GPIO pin for ext0 wakeup (default: BUTTON_INT_PIN)
 */
void enterDeepSleep(uint8_t wakeupPin = 33);

/**
 * @brief Check if ESP32 was woken by button press
 * @return true if woken by ext0 (button), false otherwise
 *
 * Possible wakeup causes:
 * - ESP_SLEEP_WAKEUP_EXT0: Button press (expected)
 * - ESP_SLEEP_WAKEUP_UNDEFINED: First boot or reset
 * - ESP_SLEEP_WAKEUP_TIMER: Timer wakeup (if enabled)
 *
 * Call this in setup() to determine initialization strategy.
 */
bool wasWokenByButton();

/**
 * @brief Get human-readable wakeup reason
 * @return String describing wakeup cause
 *
 * Useful for debugging deep sleep behavior.
 */
const char* getWakeupReason();

/**
 * @brief Save state machine state to RTC memory
 * @param state Current SystemState enum value
 *
 * RTC memory survives deep sleep (not reset).
 * Used to restore state on wakeup if needed.
 *
 * Note: Current implementation always wakes to IDLE,
 * but RTC state allows future recovery logic.
 */
void saveStateToRTC(uint8_t state);

/**
 * @brief Restore state machine state from RTC memory
 * @return Saved state, or 0 (IDLE) if RTC data invalid
 *
 * Validates RTC magic number before returning data.
 * Returns IDLE (0) if:
 * - First boot (no valid RTC data)
 * - RTC magic number invalid
 * - RTC validData flag is false
 */
uint8_t restoreStateFromRTC();

/**
 * @brief Get boot count from RTC memory
 * @return Number of times device has woken from deep sleep
 *
 * Increments on each wakeup. Useful for debugging
 * and tracking deep sleep cycles.
 */
uint32_t getBootCount();

/**
 * @brief Initialize power management (call in setup)
 *
 * Checks wakeup reason and initializes RTC memory.
 * Logs boot count and wakeup cause to Serial.
 */
void initPowerManager();

#endif // POWER_MANAGER_H
