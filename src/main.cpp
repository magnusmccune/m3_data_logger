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
#include <SparkFun_Qwiic_Button.h>  // For button object methods in state handlers
#include <ArduinoJson.h>            // For QR code JSON parsing (M3L-60)
#include <tiny_code_reader.h>       // For QR scanner (M3L-60)

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
constexpr uint32_t LED_BLINK_SLOW_MS = 500;           // 1Hz blink (500ms on, 500ms off)
constexpr uint32_t LED_BLINK_FAST_MS = 100;           // 5Hz blink (100ms on, 100ms off)
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 5000;      // 5 seconds
constexpr uint32_t SETUP_LED_BLINK_MS = 200;          // Boot LED blink duration

// State timing tracking
uint32_t stateEntryTime = 0;      // Timestamp when current state was entered
uint32_t lastLEDToggle = 0;       // Last LED toggle for blink patterns
bool ledState = false;             // Current LED state for blinking

// Button interrupt handling
volatile bool buttonPressed = false;  // Flag set by ISR, checked in main loop
uint32_t lastButtonPressTime = 0;     // Track last button press for debouncing
constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;  // 50ms debounce window

// QR Code Metadata Storage (M3L-60)
char currentTestName[65] = "";        // Test name from QR code (max 64 chars + null terminator)
String currentLabels[10];             // Label array from QR code (max 10 labels)
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
 * @brief Update LED pattern based on current state
 *
 * Non-blocking LED control:
 * - IDLE: LED OFF
 * - AWAITING_QR: Slow blink (1Hz)
 * - RECORDING: Solid ON
 * - ERROR: Fast blink (5Hz)
 */
void updateLEDPattern() {
    uint32_t now = millis();

    switch (currentState) {
        case SystemState::IDLE:
            // LED OFF
            button.LEDoff();
            ledState = false;
            break;

        case SystemState::AWAITING_QR:
            // Slow blink (1Hz) - 500ms on, 500ms off
            if (now - lastLEDToggle >= LED_BLINK_SLOW_MS) {
                ledState = !ledState;
                if (ledState) {
                    button.LEDon(255);  // Full brightness when on
                } else {
                    button.LEDoff();
                }
                lastLEDToggle = now;
            }
            break;

        case SystemState::RECORDING:
            // Solid ON - full brightness
            button.LEDon(255);
            ledState = true;
            break;

        case SystemState::ERROR:
            // Fast blink (5Hz) - 100ms on, 100ms off
            if (now - lastLEDToggle >= LED_BLINK_FAST_MS) {
                ledState = !ledState;
                if (ledState) {
                    button.LEDon(255);  // Full brightness when on
                } else {
                    button.LEDoff();
                }
                lastLEDToggle = now;
            }
            break;
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
            // TODO (M3L-61): Stop IMU sampling
            // TODO (M3L-62): Close data file on SD card
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
            // TODO (M3L-61): Start IMU sampling at 100Hz
            // TODO (M3L-62): Create new data file on SD card
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
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.print("✗ JSON parse failed: ");
        Serial.println(error.c_str());
        return false;
    }
    
    // Extract test name (required field)
    const char* test = doc["test"];
    if (!test || strlen(test) == 0 || strlen(test) > 64) {
        Serial.println("✗ Invalid test name (must be 1-64 chars)");
        return false;
    }
    strncpy(currentTestName, test, 64);
    currentTestName[64] = '\0';
    
    // Extract labels array (required field, min 1 label)
    JsonArray labels = doc["labels"];
    if (!labels || labels.size() == 0 || labels.size() > 10) {
        Serial.println("✗ Invalid labels array (must have 1-10 labels)");
        return false;
    }
    
    // Parse individual labels
    labelCount = 0;
    for (JsonVariant label : labels) {
        const char* labelStr = label.as<const char*>();
        if (labelStr && strlen(labelStr) > 0 && strlen(labelStr) <= 32) {
            currentLabels[labelCount++] = String(labelStr);
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
    Serial.print("  Test: ");
    Serial.println(currentTestName);
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
            
            // QR code found, parse JSON metadata
            return parseQRMetadata((const char*)results.content_bytes);
        }
    }
    
    // No QR code detected yet (not an error, just keep polling)
    return false;
}

/**
 * @brief Blocking LED blink pattern for QR feedback
 * @param count Number of blinks (3=success, 5=failure)
 * 
 * NOTE: This is a blocking function (uses delay). Only call in state transitions,
 *       not during active state handling. The blink is fast (100ms on/off) to minimize
 *       blocking time.
 */
void blinkButtonLED(uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        button.LEDon(255);  // Full brightness
        delay(100);
        button.LEDoff();
        delay(100);
    }
}

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
            
            // Visual confirmation: blink LED briefly
            button.LEDon(255);  // Full brightness
            delay(100);  // Blocking OK for user feedback (100ms is acceptable)
            button.LEDoff();

            // Clear interrupt flags to prevent repeat triggers
            button.clearEventBits();

            // Transition to AWAITING_QR state
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
            
            // Visual confirmation: blink LED
            button.LEDon(255);
            delay(100);
            button.LEDoff();
            
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
            // Visual feedback: 3 fast blinks
            blinkButtonLED(3);
            
            // Transition to RECORDING state
            transitionState(SystemState::RECORDING, "QR code scanned successfully");
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
            
            // Visual confirmation: blink LED briefly
            button.LEDon(255);  // Full brightness
            delay(100);  // Blocking OK for user feedback
            button.LEDoff();

            // Clear interrupt flags
            button.clearEventBits();

            // Stop recording and return to IDLE
            transitionState(SystemState::IDLE, "recording stopped via button");
        } else {
            buttonPressed = false;  // Clear spurious interrupt
        }
    }

    // TODO (M3L-61): Sample IMU data at 100Hz
    // TODO (M3L-62): Write buffered data to SD card periodically
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
            
            // Visual confirmation: blink LED briefly
            button.LEDon(255);  // Full brightness
            delay(100);  // Blocking OK for user feedback
            button.LEDoff();

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
    Serial.println("║      M3 Data Logger - Initializing    ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.print("Firmware Version: ");
    Serial.println(FW_VERSION);
    Serial.print("Build: ");
    Serial.println(BUILD_DATE);
    Serial.println();

    // Print hardware information
    printHardwareInfo();

    // NOTE: Status LED removed - using Qwiic Button LED for all status indication
    Serial.println("✓ Status LED: Using Qwiic Button LED");
    Serial.println();

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

    // TODO (M3L-59): Initialize I2C sensors
    // TODO (M3L-60): Initialize Tiny Code Reader (QR scanner)
    // TODO (M3L-61): Initialize ISM330DHCX IMU

    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║   Initialization Complete - Ready     ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.println();
    Serial.println("Current State: IDLE");
    Serial.println("Waiting for sensor integration (M3L-57)...");
    Serial.println();

    // Blink button LED to indicate successful initialization
    for (int i = 0; i < 3; i++) {
        button.LEDon(255);  // Full brightness
        delay(SETUP_LED_BLINK_MS);  // Blocking OK during setup - no active state machine yet
        button.LEDoff();
        delay(SETUP_LED_BLINK_MS);  // Blocking OK during setup - no active state machine yet
    }
}

void loop() {
    // Update LED pattern (non-blocking)
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
