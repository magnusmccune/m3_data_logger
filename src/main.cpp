/**
 * M3 Data Logger - Main Application
 *
 * Hardware: SparkFun DataLogger IoT (DEV-22462)
 * - ESP32-WROOM-32E microcontroller
 * - MicroSD card slot (4-bit SDIO via SD_MMC)
 * - Qwiic I2C connector for sensors
 *
 * Sensors (via Qwiic/I2C):
 * - ISM330DHCX 6DoF IMU
 * - Qwiic Button with LED
 * - Tiny Code Reader (QR scanner)
 *
 * CRITICAL HARDWARE NOTES:
 * - SD card requires GPIO32 HIGH before SD_MMC.begin()
 * - Must use SD_MMC library (NOT SD library)
 * - I2C on GPIO21 (SDA) and GPIO22 (SCL)
 *
 * See: hardware_init.h for detailed pin assignments
 */

#include <Arduino.h>
#include "hardware_init.h"
#include "sensor_manager.h"         // For IMU data collection (M3L-61)
#include "storage_manager.h"        // For SD card CSV logging (M3L-63)
#include "time_manager.h"           // For GPS time sync and status (M3L-79)
#include <SparkFun_Qwiic_Button.h>  // For button object methods in state handlers
#include <ArduinoJson.h>            // For QR code JSON parsing (M3L-60)
#include <tiny_code_reader.h>       // For QR scanner (M3L-60)
#include <Adafruit_NeoPixel.h>      // For RGB LED control (M3L-80)

// Firmware version
#define FW_VERSION "0.2.0-dev"
#define BUILD_DATE __DATE__ " " __TIME__

// State machine (to be implemented in M3L-57)
enum class SystemState {
    IDLE,           // Waiting for button press
    AWAITING_QR,    // QR scanner active, 30s timeout
    RECORDING,      // IMU data logging to SD card
    ERROR           // Recoverable error state
};

SystemState currentState = SystemState::IDLE;

// State machine timing constants
constexpr uint32_t QR_SCAN_TIMEOUT_MS = 30000;        // 30 seconds for QR code scan
constexpr uint32_t ERROR_RECOVERY_TIMEOUT_MS = 60000; // 60 seconds before auto-recovery
constexpr uint32_t LED_BLINK_SLOW_MS = 1000;          // 0.5Hz blink (1s on, 1s off) for AWAITING_QR
constexpr uint32_t LED_BLINK_FAST_MS = 100;           // 5Hz blink (100ms on, 100ms off) for ERROR
constexpr uint32_t LED_BREATHING_MS = 3000;           // 3s breathing cycle for IDLE
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 5000;      // 5 seconds
constexpr uint32_t SETUP_LED_BLINK_MS = 200;          // Boot LED blink duration

// RGB LED brightness levels
constexpr uint8_t LED_BRIGHTNESS_IDLE = 10;       // 4% brightness for IDLE (breathing)
constexpr uint8_t LED_BRIGHTNESS_INDOOR = 25;     // 10% brightness for AWAITING_QR/RECORDING
constexpr uint8_t LED_BRIGHTNESS_OUTDOOR = 100;   // 40% brightness for ERROR (high visibility)

// RGB LED colors (GPS status)
constexpr uint32_t COLOR_GPS_LOCKED = 0x00FF00;   // Green: GPS locked (accurate time)
constexpr uint32_t COLOR_GPS_ACQUIRING = 0xFFAA00; // Yellow: GPS acquiring (searching)
constexpr uint32_t COLOR_GPS_MILLIS = 0x0080FF;   // Blue: No GPS (millis fallback)
constexpr uint32_t COLOR_ERROR = 0xFF0000;        // Red: ERROR state (overrides GPS)

// State timing tracking
uint32_t stateEntryTime = 0;      // Timestamp when current state was entered
uint32_t lastLEDToggle = 0;       // Last LED toggle for blink patterns
bool ledState = false;             // Current LED state for blinking
uint32_t lastGPSColor = 0;        // Last GPS color for smooth transitions

// Button interrupt handling
volatile bool buttonPressed = false;  // Flag set by ISR, checked in main loop
uint32_t lastButtonPressTime = 0;     // Track last button press for debouncing
constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;  // 50ms debounce window

// QR Code Metadata Storage (M3L-60, M3L-64)
char currentTestID[9] = "";           // Test ID from QR code (8 chars + null, e.g., "A3F9K2M7")
char currentDescription[65] = "";     // Test description from QR code (max 64 chars + null)
char currentLabels[10][33];           // Label array from QR code (max 10 labels, 32 chars + null)
uint8_t labelCount = 0;               // Number of valid labels extracted
bool metadataValid = false;           // Set to true after successful QR scan and parse

// ===== Interrupt Service Routines =====

/**
 * @brief Button interrupt service routine
 *
 * CRITICAL CONSTRAINTS:
 * - Must use IRAM_ATTR for ESP32 (places function in IRAM for fast access)
 * - NO I2C operations (will crash)
 * - NO Serial.print (unreliable)
 * - Keep execution time under 10μs
 * - Only set flags, do NOT process logic here
 *
 * This ISR is triggered by GPIO33 FALLING edge when button pressed.
 * The main loop checks the buttonPressed flag and handles state transitions.
 */
void IRAM_ATTR buttonISR() {
    buttonPressed = true;  // Set flag for main loop to process
}

// ===== Helper Functions =====

/**
 * @brief Convert SystemState enum to string for logging
 * @param state The state to convert
 * @return String representation of the state
 */
const char* stateToString(SystemState state) {
    switch (state) {
        case SystemState::IDLE:        return "IDLE";
        case SystemState::AWAITING_QR: return "AWAITING_QR";
        case SystemState::RECORDING:   return "RECORDING";
        case SystemState::ERROR:       return "ERROR";
        default:                       return "UNKNOWN";
    }
}

/**
 * @brief Get GPS status color for RGB LED
 *
 * Determines LED color based on GPS lock status and time source:
 * - Green: GPS locked (accurate UTC time)
 * - Yellow: GPS acquiring (searching for satellites)
 * - Blue: No GPS (millis fallback)
 * - Red: ERROR state (overrides GPS status)
 *
 * @return 24-bit RGB color value
 */
uint32_t getGPSColor() {
    // ERROR state always shows red (highest priority)
    if (currentState == SystemState::ERROR) {
        return COLOR_ERROR;
    }

    // Check GPS lock status
    if (isGPSLocked()) {
        return COLOR_GPS_LOCKED;  // Green: GPS has valid lock
    }

    // Check time source to distinguish acquiring vs. no GPS
    TimeSource source = getCurrentTimeSource();
    if (source == TIME_SOURCE_GPS) {
        // GPS initialized but not locked yet
        return COLOR_GPS_ACQUIRING;  // Yellow: GPS acquiring satellites
    } else {
        // Using millis() fallback (GPS not available or not locked)
        return COLOR_GPS_MILLIS;  // Blue: millis fallback
    }
}

/**
 * @brief Apply state machine pattern to RGB LED
 *
 * Dual-channel indication:
 * - Color: GPS status (from getGPSColor())
 * - Pattern: State machine
 *   - IDLE: Breathing (3s cycle, 4% max brightness)
 *   - AWAITING_QR: Slow blink (0.5Hz, 10% brightness)
 *   - RECORDING: Solid ON (10% brightness)
 *   - ERROR: Fast blink (5Hz, 40% brightness, red)
 *
 * Non-blocking implementation using millis() timing.
 */
void updateLEDPattern() {
    uint32_t now = millis();
    uint32_t color = getGPSColor();

    // Smooth color transitions (fade over 100ms)
    if (color != lastGPSColor) {
        lastGPSColor = color;
        // Immediate color change for simplicity (smooth fade is complex)
    }

    switch (currentState) {
        case SystemState::IDLE: {
            // Breathing pattern (3s cycle)
            rgbLED.setBrightness(LED_BRIGHTNESS_IDLE);

            // Calculate breathing brightness (sine wave approximation)
            uint32_t phase = now % LED_BREATHING_MS;
            float breathe = 0.5f + 0.5f * sin(2.0f * PI * phase / LED_BREATHING_MS);
            uint8_t brightness = (uint8_t)(LED_BRIGHTNESS_IDLE * breathe);

            rgbLED.setBrightness(brightness);
            rgbLED.setPixelColor(0, color);
            rgbLED.show();
            break;
        }

        case SystemState::AWAITING_QR: {
            // Slow blink (0.5Hz) - 1s on, 1s off
            rgbLED.setBrightness(LED_BRIGHTNESS_INDOOR);

            if (now - lastLEDToggle >= LED_BLINK_SLOW_MS) {
                ledState = !ledState;
                lastLEDToggle = now;
            }

            if (ledState) {
                rgbLED.setPixelColor(0, color);
            } else {
                rgbLED.setPixelColor(0, 0);  // OFF
            }
            rgbLED.show();
            break;
        }

        case SystemState::RECORDING: {
            // Solid ON - full brightness
            rgbLED.setBrightness(LED_BRIGHTNESS_INDOOR);
            rgbLED.setPixelColor(0, color);
            rgbLED.show();
            ledState = true;
            break;
        }

        case SystemState::ERROR: {
            // Fast blink (5Hz) - 100ms on, 100ms off
            rgbLED.setBrightness(LED_BRIGHTNESS_OUTDOOR);  // High visibility

            if (now - lastLEDToggle >= LED_BLINK_FAST_MS) {
                ledState = !ledState;
                lastLEDToggle = now;
            }

            if (ledState) {
                rgbLED.setPixelColor(0, COLOR_ERROR);  // Always red in ERROR
            } else {
                rgbLED.setPixelColor(0, 0);  // OFF
            }
            rgbLED.show();
            break;
        }
    }
}

/**
 * @brief Transition to a new system state with validation and logging
 *
 * Performs state transition with:
 * - Transition validation (prevent invalid transitions)
 * - Exit actions for old state
 * - Entry actions for new state
 * - Serial logging with timestamp and reason
 *
 * @param newState The target state to transition to
 * @param reason Human-readable reason for the transition
 */
void transitionState(SystemState newState, const char* reason) {
    SystemState oldState = currentState;

    // Skip if already in target state
    if (oldState == newState) {
        return;
    }

    // Validate state transitions
    bool validTransition = false;

    // Define valid state transitions
    switch (oldState) {
        case SystemState::IDLE:
            validTransition = (newState == SystemState::AWAITING_QR || 
                              newState == SystemState::ERROR);
            break;
        case SystemState::AWAITING_QR:
            validTransition = (newState == SystemState::RECORDING || 
                              newState == SystemState::IDLE ||
                              newState == SystemState::ERROR);
            break;
        case SystemState::RECORDING:
            validTransition = (newState == SystemState::IDLE || 
                              newState == SystemState::ERROR);
            break;
        case SystemState::ERROR:
            validTransition = (newState == SystemState::IDLE);
            break;
    }

    if (!validTransition) {
        Serial.print("[ERROR] Invalid state transition: ");
        Serial.print(stateToString(oldState));
        Serial.print(" → ");
        Serial.print(stateToString(newState));
        Serial.println();
        return;  // Reject invalid transition
    }

    // Log state transition
    uint32_t uptime = millis();
    Serial.print("[");
    Serial.print(uptime);
    Serial.print(" ms] STATE_CHANGE: ");
    Serial.print(stateToString(oldState));
    Serial.print(" → ");
    Serial.print(stateToString(newState));
    Serial.print(" (");
    Serial.print(reason);
    Serial.print(") | Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");

    // Exit actions for old state
    switch (oldState) {
        case SystemState::IDLE:
            // No cleanup needed
            break;

        case SystemState::AWAITING_QR:
            // TODO (M3L-60): Deactivate QR scanner
            break;

        case SystemState::RECORDING:
            // Note: Cleanup handled in handleRecordingState() before transition
            // (stopSampling() and endSession() called there)
            break;

        case SystemState::ERROR:
            // No cleanup needed
            break;
    }

    // Update state
    currentState = newState;
    stateEntryTime = millis();

    // Entry actions for new state
    switch (newState) {
        case SystemState::IDLE:
            // LED will be set to OFF by updateLEDPattern()
            Serial.println("→ Entered IDLE state: Waiting for button press");
            break;

        case SystemState::AWAITING_QR:
            // LED will be set to slow blink by updateLEDPattern()
            Serial.println("→ Entered AWAITING_QR state: Activate QR scanner (30s timeout)");
            // TODO (M3L-60): Activate QR scanner
            break;

        case SystemState::RECORDING:
            // LED will be set to solid ON by updateLEDPattern()
            Serial.println("→ Entered RECORDING state: Begin IMU data logging");
            // Start IMU sampling at 100Hz (M3L-61)
            startSampling();
            // Note: Session file already created in handleAwaitingQRState() after QR scan
            break;

        case SystemState::ERROR:
            // LED will be set to fast blink by updateLEDPattern()
            Serial.println("→ Entered ERROR state: 60s auto-recovery timer started");
            break;
    }

    // Immediately update LED for new state
    // Reset LED timing (don't force LED state - let updateLEDPattern() handle it)
    lastLEDToggle = millis();
    updateLEDPattern();
}

// ===== QR Code Scanning Functions (M3L-60) =====

/**
 * @brief Parse and validate QR code JSON metadata
 * @param json JSON string from QR code
 * @return true if valid metadata extracted, false otherwise
 */
bool parseQRMetadata(const char* json) {
    StaticJsonDocument<384> doc;  // Increased from 256 to support new QR format
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        Serial.println("✗ Error: Invalid JSON syntax");
        Serial.print("  Details: ");
        Serial.println(error.c_str());
        return false;
    }

    // Extract test_id (required field, 8 alphanumeric chars)
    const char* test_id = doc["test_id"];
    if (!test_id) {
        Serial.println("✗ Error: Missing 'test_id' field");
        return false;
    }
    if (strlen(test_id) != 8) {
        Serial.println("✗ Error: 'test_id' must be exactly 8 characters");
        return false;
    }
    // Validate alphanumeric (no special chars)
    for (size_t i = 0; i < 8; i++) {
        if (!isalnum(test_id[i])) {
            Serial.println("✗ Error: 'test_id' must be alphanumeric only");
            return false;
        }
    }
    strncpy(currentTestID, test_id, sizeof(currentTestID) - 1);
    currentTestID[sizeof(currentTestID) - 1] = '\0';

    // Extract description (required field)
    const char* description = doc["description"];
    if (!description) {
        Serial.println("✗ Error: Missing 'description' field");
        return false;
    }
    if (strlen(description) == 0) {
        Serial.println("✗ Error: 'description' field cannot be empty");
        return false;
    }
    if (strlen(description) > 64) {
        Serial.println("✗ Error: 'description' field too long (max 64 chars)");
        return false;
    }
    strncpy(currentDescription, description, sizeof(currentDescription) - 1);
    currentDescription[sizeof(currentDescription) - 1] = '\0';

    // Extract labels array (required field, min 1 label)
    JsonArray labels = doc["labels"];
    if (!labels) {
        Serial.println("✗ Error: Missing 'labels' field");
        return false;
    }
    if (labels.size() == 0) {
        Serial.println("✗ Error: 'labels' array cannot be empty");
        return false;
    }
    if (labels.size() > 10) {
        Serial.println("✗ Error: 'labels' array too large (max 10 labels)");
        return false;
    }
    
    // Parse individual labels
    labelCount = 0;
    for (JsonVariant label : labels) {
        const char* labelStr = label.as<const char*>();
        if (labelStr && strlen(labelStr) > 0 && strlen(labelStr) <= 32) {
            strncpy(currentLabels[labelCount], labelStr, 32);
            currentLabels[labelCount][32] = '\0';
            labelCount++;
        } else {
            Serial.print("✗ Invalid label (must be 1-32 chars): ");
            Serial.println(labelStr ? labelStr : "<null>");
        }
    }
    
    if (labelCount == 0) {
        Serial.println("✗ No valid labels found in array");
        return false;
    }
    
    metadataValid = true;
    
    // Log extracted metadata
    Serial.println("✓ QR metadata validated and extracted:");
    Serial.print("  Test ID: ");
    Serial.println(currentTestID);
    Serial.print("  Description: ");
    Serial.println(currentDescription);
    Serial.print("  Labels (");
    Serial.print(labelCount);
    Serial.print("): ");
    for (uint8_t i = 0; i < labelCount; i++) {
        Serial.print(currentLabels[i]);
        if (i < labelCount - 1) Serial.print(", ");
    }
    Serial.println();
    
    return true;
}

/**
 * @brief Non-blocking QR code scan
 * @return true if QR code detected and parsed successfully, false otherwise
 */
bool scanQRCode() {
    tiny_code_reader_results_t results;
    
    // Attempt to read QR code (non-blocking)
    if (tiny_code_reader_read(&results)) {
        if (results.content_length > 0) {
            Serial.println("✓ QR code detected, parsing metadata...");
            
            // Debug: Display raw JSON content and size
            Serial.print("  Raw JSON (");
            Serial.print(results.content_length);
            Serial.print(" bytes): ");

            // Use Serial.write() to print exact bytes without relying on null termination
            // This ensures we print exactly what we received, avoiding truncation
            Serial.write(results.content_bytes, results.content_length);
            Serial.println();  // Add newline after content

            // Additional debug: Check for non-printable characters
            bool hasNonPrintable = false;
            for (uint16_t i = 0; i < results.content_length; i++) {
                char c = results.content_bytes[i];
                if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
                    hasNonPrintable = true;
                    Serial.print("  Warning: Non-printable character at position ");
                    Serial.print(i);
                    Serial.print(" (byte value: ");
                    Serial.print((int)c);
                    Serial.println(")");
                }
            }
            if (!hasNonPrintable) {
                Serial.println("  Content contains only printable characters");
            }
            
            // QR code found, parse JSON metadata
            return parseQRMetadata((const char*)results.content_bytes);
        }
    }
    
    // No QR code detected yet (not an error, just keep polling)
    return false;
}

// blinkButtonLED removed - RGB LED handles all visual feedback (M3L-80)

// ===== State Handler Functions =====

/**
 * @brief Handle IDLE state
 *
 * In IDLE state:
 * - LED is OFF
 * - System waits for button press interrupt
 * - No active sensors
 */
void handleIdleState() {
    // Check for button press (flag set by ISR)
    if (buttonPressed) {
        uint32_t currentTime = millis();
        
        // Debounce check FIRST
        if (currentTime - lastButtonPressTime < BUTTON_DEBOUNCE_MS) {
            buttonPressed = false;  // Ignore bounced press
            return;
        }

        // Verify button press via I2C (NOT in ISR - safe here)
        if (button.hasBeenClicked()) {
            buttonPressed = false;  // Clear flag AFTER successful I2C verification
            lastButtonPressTime = currentTime;

            // Clear interrupt flags to prevent repeat triggers
            button.clearEventBits();

            // Transition to AWAITING_QR state
            // RGB LED will show new state pattern automatically via updateLEDPattern()
            transitionState(SystemState::AWAITING_QR, "button pressed");
        } else {
            buttonPressed = false;  // Clear spurious interrupt
        }
    }
}

/**
 * @brief Handle AWAITING_QR state
 *
 * In AWAITING_QR state:
 * - LED blinks slowly (1Hz)
 * - QR scanner is active
 * - 30-second timeout if no QR code scanned
 *
 * Transitions:
 * - QR code scanned → RECORDING
 * - Timeout (30s) → IDLE
 */
void handleAwaitingQRState() {
    uint32_t currentTime = millis();
    uint32_t timeInState = currentTime - stateEntryTime;
    
    // Check for button press to cancel QR scan
    if (buttonPressed) {
        // Debounce check FIRST
        if (currentTime - lastButtonPressTime < BUTTON_DEBOUNCE_MS) {
            buttonPressed = false;  // Ignore bounced press
            return;
        }
        
        // Verify button press via I2C
        if (button.hasBeenClicked()) {
            buttonPressed = false;  // Clear flag AFTER successful I2C verification
            lastButtonPressTime = currentTime;

            // Clear interrupt flags
            button.clearEventBits();

            transitionState(SystemState::IDLE, "QR scan cancelled via button");
            return;
        } else {
            buttonPressed = false;  // Clear spurious interrupt
        }
    }

    // Check for 30-second timeout
    if (timeInState >= QR_SCAN_TIMEOUT_MS) {
        transitionState(SystemState::IDLE, "QR scan timeout (30s)");
        return;
    }

    // Poll QR reader (non-blocking, check every 100ms to reduce I2C traffic)
    static uint32_t lastQRPoll = 0;
    if (currentTime - lastQRPoll >= 100) {
        lastQRPoll = currentTime;
        
        if (scanQRCode()) {
            // SUCCESS: Valid QR code with metadata parsed
            // RGB LED will show RECORDING state pattern automatically

            // Start data logging session (M3L-64)
            // Convert char array to const char* array for storage manager
            const char* labelPtrs[10];
            for (uint8_t i = 0; i < labelCount; i++) {
                labelPtrs[i] = currentLabels[i];  // Safe - pointer to static array
            }

            if (startSession(currentTestID, currentDescription, labelPtrs, labelCount)) {
                Serial.println("[Session] Recording session started");
                // Transition to RECORDING state
                transitionState(SystemState::RECORDING, "QR code scanned successfully");
            } else {
                Serial.println("[Session] ERROR: Failed to start recording session");
                transitionState(SystemState::ERROR, "session start failed");
            }
            return;
        }
    }
    
    // If scanQRCode returns false, it means either:
    // 1. No QR code detected yet (keep waiting)
    // 2. QR code detected but invalid JSON (stay in AWAITING_QR for retry)
    // The parseQRMetadata function already logs the error, so no action needed here
}

/**
 * @brief Handle RECORDING state
 *
 * In RECORDING state:
 * - LED is solid ON
 * - IMU data is being sampled at 100Hz
 * - Data is written to SD card
 * - System waits for button press to stop recording
 */
void handleRecordingState() {
    // Check for button press to stop recording
    if (buttonPressed) {
        uint32_t currentTime = millis();

        // Debounce check FIRST
        if (currentTime - lastButtonPressTime < BUTTON_DEBOUNCE_MS) {
            buttonPressed = false;  // Ignore bounced press
            return;
        }

        // Verify button press via I2C
        if (button.hasBeenClicked()) {
            buttonPressed = false;  // Clear flag AFTER successful I2C verification
            lastButtonPressTime = currentTime;

            // Clear interrupt flags
            button.clearEventBits();

            // Stop sampling and end session (M3L-64)
            stopSampling();
            if (!endSession()) {
                Serial.println("[Session] WARNING: Error ending session");
            }

            // Stop recording and return to IDLE
            transitionState(SystemState::IDLE, "recording stopped via button");
        } else {
            buttonPressed = false;  // Clear spurious interrupt
        }
    }

    // Sample IMU data at 100Hz (M3L-61)
    if (isSampleReady()) {
        IMUSample sample;
        readIMUSample(&sample);  // Adds to circular buffer
    }

    // Drain circular buffer to SD card
    IMUSample sample;
    while (getBufferedSample(&sample)) {
        if (!writeSample(sample)) {
            Serial.println("[Recording] ERROR: Failed to write sample to SD");
            break;
        }
    }

    // Print sampling statistics every 5 seconds
    static uint32_t lastStatsTime = 0;
    uint32_t currentTime = millis();
    if (currentTime - lastStatsTime >= 5000) {
        lastStatsTime = currentTime;

        float actualRate = 0.0f;
        float lossRate = 0.0f;
        getSamplingStats(&actualRate, &lossRate);

        Serial.printf("[Recording] Sample rate: %.1f Hz, Loss: %.2f%%\n",
                      actualRate, lossRate);
    }
}

/**
 * @brief Handle ERROR state
 *
 * In ERROR state:
 * - LED blinks rapidly (5Hz)
 * - System attempts auto-recovery after 60 seconds
 * - Can also be manually cleared by button press
 */
void handleErrorState() {
    uint32_t currentTime = millis();
    uint32_t timeInState = currentTime - stateEntryTime;

    // Check for manual recovery via button press
    if (buttonPressed) {
        // Debounce check FIRST
        if (currentTime - lastButtonPressTime < BUTTON_DEBOUNCE_MS) {
            buttonPressed = false;  // Ignore bounced press
            return;
        }

        // Verify button press via I2C
        if (button.hasBeenClicked()) {
            buttonPressed = false;  // Clear flag AFTER successful I2C verification
            lastButtonPressTime = currentTime;

            // Clear interrupt flags
            button.clearEventBits();

            // Manual recovery - return to IDLE
            transitionState(SystemState::IDLE, "manual recovery via button");
            return;
        } else {
            buttonPressed = false;  // Clear spurious interrupt
        }
    }

    // Auto-recovery after 60 seconds
    if (timeInState >= ERROR_RECOVERY_TIMEOUT_MS) {
        transitionState(SystemState::IDLE, "auto-recovery timeout (60s)");
        return;
    }
}

void setup() {
    // Initialize serial communication for debugging
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        ; // Wait for serial port to connect (max 3 seconds)
    }

    Serial.println();
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║      M3 Data Logger - Initializing     ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.print("Firmware Version: ");
    Serial.println(FW_VERSION);
    Serial.print("Build: ");
    Serial.println(BUILD_DATE);
    Serial.println();

    // Print hardware information
    printHardwareInfo();

    // Initialize RGB LED (M3L-80)
    if (!initializeRGBLED()) {
        Serial.println("⚠ WARNING: RGB LED initialization failed");
        Serial.println("   Visual status indication disabled");
    }

    // CRITICAL: Initialize SD card with level shifter activation (GPIO32)
    if (!initializeSDCard()) {
        Serial.println("⚠ FATAL: Cannot proceed without SD card");
        Serial.println("   System will continue for debugging, but logging disabled");
        currentState = SystemState::ERROR;
    }

    // Initialize I2C bus for Qwiic sensors
    if (!initializeI2C(true)) {  // true = scan bus
        Serial.println("⚠ WARNING: I2C initialization issues detected");
    }

    // Initialize Qwiic Button with interrupt (M3L-58)
    if (!initializeQwiicButton()) {
        Serial.println("⚠ WARNING: Button initialization failed");
        Serial.println("   Button press functionality disabled");
    }

    // Initialize Tiny Code Reader for QR scanning (M3L-60)
    if (!initializeQRReader()) {
        Serial.println("⚠ WARNING: QR Reader initialization failed");
        Serial.println("   QR code scanning functionality disabled");
    }

    // Initialize IMU sensor (M3L-61)
    if (!initializeIMU()) {
        Serial.println("⚠ WARNING: IMU initialization failed");
        Serial.println("   Sensor data collection disabled");
    }

    // Initialize storage manager (M3L-63)
    if (!initializeStorage()) {
        Serial.println("⚠ WARNING: Storage manager initialization failed");
        Serial.println("   Data logging disabled");
    }

    // Initialize GPS module (M3L-79)
    if (!initializeGPS()) {
        Serial.println("⚠ WARNING: GPS initialization failed");
        Serial.println("   GPS time sync disabled, using millis() fallback");
    }

    // Initialize time manager (M3L-78)
    initTimeManager();
    Serial.println("✓ Time manager initialized");
    Serial.println();

    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║   Initialization Complete - Ready      ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.println();
    Serial.println("Current State: IDLE");
    Serial.println("Waiting for button press to start QR scan...");
    Serial.println();

    // Show startup pattern on RGB LED (3 quick flashes)
    for (int i = 0; i < 3; i++) {
        rgbLED.setPixelColor(0, COLOR_GPS_MILLIS);  // Blue startup indicator
        rgbLED.setBrightness(LED_BRIGHTNESS_INDOOR);
        rgbLED.show();
        delay(SETUP_LED_BLINK_MS);  // Blocking OK during setup - no active state machine yet
        rgbLED.setPixelColor(0, 0);  // OFF
        rgbLED.show();
        delay(SETUP_LED_BLINK_MS);  // Blocking OK during setup - no active state machine yet
    }
}

void loop() {
    // Update time manager (GPS polling for M3L-79)
    updateTime();

    // Update LED pattern (non-blocking, dual-channel GPS + state)
    updateLEDPattern();

    // Poll button status if interrupts aren't available (fallback mode)
    // Check every 50ms to avoid excessive I2C traffic
    static unsigned long lastPoll = 0;
    unsigned long now = millis();
    
    if (!buttonPressed && (now - lastPoll >= 50)) {  // Poll if no interrupt pending
        lastPoll = now;
        
        // Check if button has been clicked via I2C polling
        if (button.hasBeenClicked()) {
            buttonPressed = true;  // Set flag as if interrupt fired
            Serial.println("[POLLING] Button press detected via I2C poll");
        }
    }

    // Call appropriate state handler
    switch (currentState) {
        case SystemState::IDLE:
            handleIdleState();
            break;

        case SystemState::AWAITING_QR:
            handleAwaitingQRState();
            break;

        case SystemState::RECORDING:
            handleRecordingState();
            break;

        case SystemState::ERROR:
            handleErrorState();
            break;
    }

    // Heartbeat logging (every 5 seconds)
    static unsigned long lastHeartbeat = 0;

    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeat = now;

        Serial.print("♥ Heartbeat: ");
        Serial.print(now / 1000);
        Serial.print("s uptime | Free Heap: ");
        Serial.print(ESP.getFreeHeap());
        Serial.print(" bytes | State: ");
        Serial.print(stateToString(currentState));
        Serial.print(" | Time in state: ");
        Serial.print((now - stateEntryTime) / 1000);
        Serial.println("s");
    }

    // TODO (M3L-60): QR code event handling will be added
    // TODO (M3L-61): IMU data buffering and SD writes will be added
}
