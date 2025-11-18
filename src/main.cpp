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
#include "battery_manager.h"        // For MAX17048 fuel gauge (M3L-83)
#include "power_manager.h"          // For deep sleep power management (M3L-83)
#include "sensor_manager.h"         // For IMU data collection (M3L-61)
#include "storage_manager.h"        // For SD card CSV logging (M3L-63)
#include "time_manager.h"           // For GPS time sync and status (M3L-79)
#include "network_manager.h"        // For WiFi and MQTT configuration (M3L-71)
#include <SparkFun_Qwiic_Button.h>  // For button object methods in state handlers
#include <ArduinoJson.h>            // For QR code JSON parsing (M3L-60)
#include <tiny_code_reader.h>       // For QR scanner (M3L-60)
#include <Adafruit_NeoPixel.h>      // For RGB LED control (M3L-80)
#include <WiFi.h>                   // For WiFi connection testing in CONFIG state (M3L-72)

// Firmware version
#define FW_VERSION "0.2.0-dev"
#define BUILD_DATE __DATE__ " " __TIME__

// State machine (to be implemented in M3L-57)
enum class SystemState {
    IDLE,           // Waiting for button press
    AWAITING_QR,    // QR scanner active, 30s timeout
    RECORDING,      // IMU data logging to SD card
    CONFIG,         // Configuration mode (QR-based device setup)
    ERROR           // Recoverable error state
};;

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
constexpr uint32_t COLOR_CONFIG = 0x8000FF;       // Purple: CONFIG state (device setup)

// State timing tracking
uint32_t stateEntryTime = 0;      // Timestamp when current state was entered
uint32_t lastLEDToggle = 0;       // Last LED toggle for blink patterns
bool ledState = false;             // Current LED state for blinking
uint32_t lastGPSColor = 0;        // Last GPS color for smooth transitions

// Button interrupt handling
volatile bool buttonPressed = false;  // Flag set by ISR, checked in main loop
uint32_t lastButtonPressTime = 0;     // Track last button press for debouncing
uint32_t buttonPressStartTime = 0;    // Track button hold start time for CONFIG mode
constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;  // 50ms debounce window
constexpr uint32_t CONFIG_BUTTON_HOLD_MS = 3000;  // 3s button hold to enter CONFIG mode

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
        case SystemState::CONFIG:      return "CONFIG";
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

        case SystemState::CONFIG: {
            // Purple double-blink pattern (250ms ON, 250ms OFF, 250ms ON, 500ms gap = 1250ms cycle)
            rgbLED.setBrightness(LED_BRIGHTNESS_INDOOR);

            uint32_t cyclePos = now % 1250;
            if (cyclePos < 250 || (cyclePos >= 500 && cyclePos < 750)) {
                rgbLED.setPixelColor(0, COLOR_CONFIG);  // Purple ON
            } else {
                rgbLED.setPixelColor(0, 0);  // OFF
            }
            rgbLED.show();
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
                              newState == SystemState::CONFIG ||
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
        case SystemState::CONFIG:
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

        case SystemState::CONFIG:
            // No cleanup needed (WiFi disconnect handled in handleConfigState)
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

        case SystemState::CONFIG:
            // LED will be set to purple double-blink by updateLEDPattern()
            Serial.println("→ Entered CONFIG state: Scan configuration QR code (30s timeout)");
            Serial.println("   Hold button for 3s from IDLE to enter CONFIG mode");
            Serial.println("   Press button again to cancel");
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
 * @brief Parse configuration QR code JSON and populate NetworkConfig struct
 *
 * Expected format:
 * {
 *   "type": "device_config",
 *   "version": "1.0",
 *   "wifi": {"ssid": "...", "password": "..."},
 *   "mqtt": {"host": "...", "port": 1883, "username": "...", "password": "...", "device_id": "..."}
 * }
 *
 * @param json JSON string from QR code
 * @param config NetworkConfig struct to populate
 * @return true if valid config QR parsed successfully, false otherwise
 */
bool parseConfigQR(const char* json, NetworkConfig* config) {
    if (!config) {
        Serial.println("[CONFIG] ERROR: Null config pointer");
        return false;
    }

    StaticJsonDocument<512> doc;  // Larger buffer for config QR
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        // Suppress EmptyInput errors (normal during QR polling with no QR present)
        if (error != DeserializationError::EmptyInput) {
            Serial.println("[CONFIG] ✗ Error: Invalid JSON syntax");
            Serial.print("  Details: ");
            Serial.println(error.c_str());
        }
        return false;
    }

    // Reject metadata QRs (check for test_id field)
    if (doc.containsKey("test_id")) {
        Serial.println("[CONFIG] ✗ Error: This is a metadata QR, not a config QR");
        Serial.println("  Hint: Metadata QRs are for recording sessions, not device configuration");
        return false;
    }

    // Validate schema: type field
    const char* type = doc["type"];
    if (!type || strcmp(type, "device_config") != 0) {
        Serial.println("[CONFIG] ✗ Error: Missing or invalid 'type' field (expected 'device_config')");
        return false;
    }

    // Validate schema: version field
    const char* version = doc["version"];
    if (!version || strcmp(version, "1.0") != 0) {
        Serial.println("[CONFIG] ✗ Error: Missing or invalid 'version' field (expected '1.0')");
        return false;
    }

    // Extract WiFi settings
    JsonObject wifi = doc["wifi"];
    if (!wifi) {
        Serial.println("[CONFIG] ✗ Error: Missing 'wifi' object");
        return false;
    }

    const char* ssid = wifi["ssid"];
    if (!ssid || strlen(ssid) < 1 || strlen(ssid) >= WIFI_SSID_MAX_LEN) {
        Serial.printf("[CONFIG] ✗ Error: Invalid WiFi SSID (must be 1-%d chars)\n", WIFI_SSID_MAX_LEN - 1);
        return false;
    }

    const char* password = wifi["password"];
    if (!password || strlen(password) < 8 || strlen(password) >= WIFI_PASSWORD_MAX_LEN) {
        Serial.printf("[CONFIG] ✗ Error: Invalid WiFi password (must be 8-%d chars for WPA2)\n", WIFI_PASSWORD_MAX_LEN - 1);
        return false;
    }

    // Extract MQTT settings
    JsonObject mqtt = doc["mqtt"];
    if (!mqtt) {
        Serial.println("[CONFIG] ✗ Error: Missing 'mqtt' object");
        return false;
    }

    const char* host = mqtt["host"];
    if (!host || strlen(host) < 1 || strlen(host) >= MQTT_HOST_MAX_LEN) {
        Serial.printf("[CONFIG] ✗ Error: Invalid MQTT host (must be 1-%d chars)\n", MQTT_HOST_MAX_LEN - 1);
        return false;
    }

    uint16_t port = mqtt["port"] | 0;
    if (port < MQTT_PORT_MIN || port > MQTT_PORT_MAX) {
        Serial.printf("[CONFIG] ✗ Error: Invalid MQTT port %d (must be %d-%d)\n", port, MQTT_PORT_MIN, MQTT_PORT_MAX);
        return false;
    }

    const char* device_id = mqtt["device_id"];
    if (!device_id || strlen(device_id) < 1 || strlen(device_id) >= DEVICE_ID_MAX_LEN) {
        Serial.printf("[CONFIG] ✗ Error: Invalid device_id (must be 1-%d chars)\n", DEVICE_ID_MAX_LEN - 1);
        return false;
    }

    // Optional MQTT username/password
    const char* username = mqtt["username"] | "";
    const char* mqtt_password = mqtt["password"] | "";

    if (strlen(username) >= MQTT_USERNAME_MAX_LEN) {
        Serial.printf("[CONFIG] ✗ Error: MQTT username too long (max %d chars)\n", MQTT_USERNAME_MAX_LEN - 1);
        return false;
    }

    if (strlen(mqtt_password) >= MQTT_PASSWORD_MAX_LEN) {
        Serial.printf("[CONFIG] ✗ Error: MQTT password too long (max %d chars)\n", MQTT_PASSWORD_MAX_LEN - 1);
        return false;
    }

    // Populate NetworkConfig struct
    strncpy(config->wifi_ssid, ssid, WIFI_SSID_MAX_LEN - 1);
    config->wifi_ssid[WIFI_SSID_MAX_LEN - 1] = '\0';

    strncpy(config->wifi_password, password, WIFI_PASSWORD_MAX_LEN - 1);
    config->wifi_password[WIFI_PASSWORD_MAX_LEN - 1] = '\0';

    strncpy(config->mqtt_host, host, MQTT_HOST_MAX_LEN - 1);
    config->mqtt_host[MQTT_HOST_MAX_LEN - 1] = '\0';

    config->mqtt_port = port;

    strncpy(config->device_id, device_id, DEVICE_ID_MAX_LEN - 1);
    config->device_id[DEVICE_ID_MAX_LEN - 1] = '\0';

    strncpy(config->mqtt_username, username, MQTT_USERNAME_MAX_LEN - 1);
    config->mqtt_username[MQTT_USERNAME_MAX_LEN - 1] = '\0';

    strncpy(config->mqtt_password, mqtt_password, MQTT_PASSWORD_MAX_LEN - 1);
    config->mqtt_password[MQTT_PASSWORD_MAX_LEN - 1] = '\0';

    config->mqtt_enabled = true;  // MQTT enabled if config QR scanned

    // Final validation using network_manager validation function
    if (!validateNetworkConfig(config)) {
        Serial.println("[CONFIG] ✗ Error: Config validation failed");
        return false;
    }

    // Log parsed config (mask passwords)
    Serial.println("[CONFIG] ✓ Configuration QR validated:");
    Serial.printf("  WiFi SSID: %s\n", config->wifi_ssid);
    Serial.println("  WiFi Password: ********");
    Serial.printf("  MQTT Host: %s\n", config->mqtt_host);
    Serial.printf("  MQTT Port: %d\n", config->mqtt_port);
    Serial.printf("  Device ID: %s\n", config->device_id);
    if (strlen(config->mqtt_username) > 0) {
        Serial.printf("  MQTT Username: %s\n", config->mqtt_username);
        Serial.println("  MQTT Password: ********");
    }

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
    uint32_t currentTime = millis();

    // Check for button press (flag set by ISR)
    if (buttonPressed) {
        // Debounce check
        if (currentTime - lastButtonPressTime < BUTTON_DEBOUNCE_MS) {
            buttonPressed = false;
            return;
        }

        // Start tracking button press
        buttonPressStartTime = currentTime;
        buttonPressed = false;  // Clear ISR flag immediately
        Serial.println("[IDLE] Button press started, tracking hold duration...");
    }

    // Check if we're tracking a button press
    if (buttonPressStartTime > 0) {
        uint32_t pressDuration = currentTime - buttonPressStartTime;

        // Check if button is still pressed (polling mode compatible)
        bool stillPressed = button.isPressed();

        if (!stillPressed) {
            // Button was released
            button.clearEventBits();
            lastButtonPressTime = currentTime;
            buttonPressStartTime = 0;

            if (pressDuration >= CONFIG_BUTTON_HOLD_MS) {
                // Released after 3s hold → Long press
                Serial.println("[IDLE] Long press detected (3s hold, then released)");
                transitionState(SystemState::CONFIG, "long button press");
                return;  // Don't continue to deep sleep check
            } else if (pressDuration >= BUTTON_DEBOUNCE_MS) {
                // Released before 3s → Short press
                Serial.println("[IDLE] Short press detected");
                transitionState(SystemState::AWAITING_QR, "button pressed");
                return;  // Don't continue to deep sleep check
            }
            // Else: Released too quickly (< debounce time), ignore
        } else if (pressDuration >= CONFIG_BUTTON_HOLD_MS) {
            // Button STILL pressed after 3s → Long press (trigger while holding)
            button.clearEventBits();
            lastButtonPressTime = currentTime;
            buttonPressStartTime = 0;
            Serial.println("[IDLE] Long press detected (3s hold)");
            transitionState(SystemState::CONFIG, "long button press");
            return;  // Don't continue to deep sleep check
        }
        // Else: Still tracking, button is being held
    }

    // Deep sleep timeout (M3L-83)
    // Enter deep sleep after IDLE_TIMEOUT_MS to save battery
    uint32_t idleTime = currentTime - stateEntryTime;
    if (idleTime >= IDLE_TIMEOUT_MS) {
        Serial.println("\n[IDLE] Deep sleep timeout reached");
        Serial.printf("[IDLE] Idle time: %lu ms\n", idleTime);

        // Save state to RTC memory (optional, currently not used on wake)
        saveStateToRTC((uint8_t)currentState);

        // Enter deep sleep (will wake on hardware RESET button only)
        // Note: This function does not return - device will reset on wake
        enterDeepSleep(BUTTON_INT_PIN);
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

    // Poll QR reader (non-blocking, check every 250ms to reduce console spam)
    static uint32_t lastQRPoll = 0;
    if (currentTime - lastQRPoll >= 250) {
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

    // Get current GPS location ONCE per loop iteration (M3L-82)
    // This avoids repeated I2C calls (100-500μs each) that could block 100Hz IMU sampling
    float currentLat = 0.0f;
    float currentLon = 0.0f;
    getGPSLocation(currentLat, currentLon);

    // Drain circular buffer to SD card
    IMUSample sample;
    while (getBufferedSample(&sample)) {
        // Populate with cached GPS location
        sample.lat = currentLat;
        sample.lon = currentLon;
        
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

/**
 * @brief Handle CONFIG state
 *
 * CONFIG state flow:
 * 1. Scan for configuration QR code (30s timeout)
 * 2. Parse and validate config JSON
 * 3. Test WiFi connection with new credentials
 * 4. If WiFi test succeeds: Save config and return to IDLE
 * 5. If WiFi test fails: Rollback (keep old config) and return to IDLE
 *
 * User can cancel by pressing button again
 */
void handleConfigState() {
    uint32_t currentTime = millis();
    uint32_t timeInState = currentTime - stateEntryTime;

    // Check for button press to cancel config mode
    if (buttonPressed) {
        // Debounce check FIRST
        if (currentTime - lastButtonPressTime < BUTTON_DEBOUNCE_MS) {
            buttonPressed = false;  // Ignore bounced press
            return;
        }

        // Verify button press via I2C
        if (button.hasBeenClicked()) {
            buttonPressed = false;
            lastButtonPressTime = currentTime;

            // Clear interrupt flags
            button.clearEventBits();

            Serial.println("[CONFIG] Configuration cancelled by user");
            transitionState(SystemState::IDLE, "config cancelled via button");
            return;
        } else {
            buttonPressed = false;  // Clear spurious interrupt
        }
    }

    // Check for 30-second timeout (same as AWAITING_QR)
    if (timeInState >= QR_SCAN_TIMEOUT_MS) {
        Serial.println("[CONFIG] Configuration timeout (30s)");
        transitionState(SystemState::IDLE, "config timeout (30s)");
        return;
    }

    // Poll QR reader (non-blocking, check every 250ms to reduce console spam)
    static uint32_t lastQRPoll = 0;
    if (currentTime - lastQRPoll >= 250) {
        lastQRPoll = currentTime;

        // Scan for QR code
        tiny_code_reader_results_t results;
        if (tiny_code_reader_read(&results)) {
            // Only process if QR code has content (avoid spam from empty reads)
            if (results.content_length > 0) {
                // Convert byte array to null-terminated string
                char qrData[257];  // Max 256 bytes + null terminator
                size_t len = results.content_length < 256 ? results.content_length : 256;
                memcpy(qrData, results.content_bytes, len);
                qrData[len] = '\0';

                Serial.println("\n[CONFIG] QR code detected");
                Serial.printf("[CONFIG] Length: %d bytes\n", len);
                Serial.println("[CONFIG] Parsing configuration...");

            // Parse configuration QR
            NetworkConfig newConfig;
            if (parseConfigQR(qrData, &newConfig)) {
                // Config parsed successfully - now test WiFi connection
                Serial.println("[CONFIG] Testing WiFi connection...");
                
                // Temporarily disconnect existing WiFi (if connected)
                WiFi.disconnect(true);
                delay(100);

                // Try to connect with new credentials
                WiFi.begin(newConfig.wifi_ssid, newConfig.wifi_password);
                
                uint32_t connectStart = millis();
                const uint32_t WIFI_TEST_TIMEOUT_MS = 5000;  // 5 second timeout
                bool connected = false;

                while (millis() - connectStart < WIFI_TEST_TIMEOUT_MS) {
                    if (WiFi.status() == WL_CONNECTED) {
                        connected = true;
                        break;
                    }
                    delay(100);
                }

                if (connected) {
                    // WiFi test SUCCESSFUL - save config
                    Serial.println("[CONFIG] ✓ WiFi connection successful!");
                    Serial.printf("[CONFIG] IP Address: %s\n", WiFi.localIP().toString().c_str());
                    Serial.printf("[CONFIG] Signal: %d dBm\n", WiFi.RSSI());

                    // Save configuration to NVS and SD
                    if (saveNetworkConfig(&newConfig)) {
                        Serial.println("[CONFIG] ✓ Configuration saved successfully");
                        
                        // Disconnect WiFi (will reconnect on next boot or MQTT init)
                        WiFi.disconnect(true);
                        
                        // Success - return to IDLE
                        transitionState(SystemState::IDLE, "config saved successfully");
                    } else {
                        Serial.println("[CONFIG] ✗ Failed to save configuration");
                        WiFi.disconnect(true);
                        transitionState(SystemState::ERROR, "config save failed");
                    }
                } else {
                    // WiFi test FAILED - rollback (keep old config)
                    Serial.println("[CONFIG] ✗ WiFi connection failed");
                    Serial.println("[CONFIG] Possible causes:");
                    Serial.println("  - Incorrect WiFi password");
                    Serial.println("  - SSID not in range");
                    Serial.println("  - Router configuration issue");
                    Serial.println("[CONFIG] Old configuration retained (no changes made)");

                    WiFi.disconnect(true);
                    
                    // Return to IDLE without saving
                    transitionState(SystemState::IDLE, "WiFi test failed - config not saved");
                }
            } else {
                // Config parsing failed - stay in CONFIG for retry
                Serial.println("[CONFIG] Invalid configuration QR code");
                Serial.println("[CONFIG] Please scan a valid configuration QR code");
                // Don't transition - allow user to scan another QR
            }
            }  // End of if (results.content_length > 0)
        }  // End of if (tiny_code_reader_read(&results))
    }  // End of if (currentTime - lastQRPoll >= 250)
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

    // Initialize power manager and check wakeup reason (M3L-83)
    initPowerManager();

    // Check if woken by button press - if so, transition directly to AWAITING_QR
    // This skips the normal IDLE state and acts as if button was just pressed
    bool wokenByButton = wasWokenByButton();

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

    // Initialize MAX17048 battery fuel gauge (M3L-83)
    if (!initBattery()) {
        Serial.println("⚠ WARNING: Battery fuel gauge initialization failed");
        Serial.println("   Battery monitoring disabled");
    } else {
        logBatteryStatus();  // Display initial battery status
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

    // Initialize network manager (M3L-71)
    if (!initializeNetworkManager()) {
        Serial.println("⚠ WARNING: Network manager initialization failed");
        Serial.println("   WiFi and MQTT functionality disabled");
    } else {
        // Attempt WiFi auto-connect with 5s timeout (non-blocking)
        Serial.println("[Network] Attempting WiFi auto-connect...");
        if (connectWiFi()) {
            Serial.println("✓ WiFi connected successfully");
        } else {
            Serial.println("⚠ WiFi connection failed or not configured");
            Serial.println("   Continuing in offline mode (SD-only recording)");
        }
    }
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

    // Device always wakes from hardware RESET button (EN pin)
    // No need to check wakeup source or clear button state
    // Qwiic button state is fresh after boot
    Serial.println("[BOOT] Ready for user input");
}

void loop() {
    // Handle serial commands (non-blocking, char-by-char) - M3L-71
    static String commandBuffer = "";
    static bool promptShown = false;

    // Process incoming serial characters
    while (Serial.available() > 0) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            // Command complete - process it
            if (commandBuffer.length() > 0) {
                Serial.println();  // Move to new line

                String command = commandBuffer;
                command.trim();

                // Check if it's a network config command
                if (command.startsWith("config ")) {
                    handleNetworkCommand(command);
                } else if (command.equalsIgnoreCase("help")) {
                    Serial.println("[Main] Available commands:");
                    Serial.println("  config show - Display network configuration");
                    Serial.println("  config set <field> <value> - Update configuration");
                    Serial.println("  config reset - Reset to factory defaults");
                    Serial.println("  help - Show this help message");
                } else if (command.length() > 0) {
                    Serial.printf("[Main] Unknown command: %s\n", command.c_str());
                    Serial.println("[Main] Type 'help' for available commands");
                }

                commandBuffer = "";
                promptShown = false;  // Show prompt again after command
            }
        } else if (c == 127 || c == 8) {  // Backspace or Delete
            if (commandBuffer.length() > 0) {
                commandBuffer.remove(commandBuffer.length() - 1);
                Serial.write(8);    // Move cursor back
                Serial.write(' ');  // Erase character
                Serial.write(8);    // Move cursor back again
            }
        } else if (c >= 32 && c <= 126) {  // Printable ASCII only
            commandBuffer += c;
            Serial.write(c);  // Echo character
        }
    }

    // Show prompt when idle and no command is being typed
    if (!promptShown && commandBuffer.length() == 0 && Serial.availableForWrite() > 0) {
        static unsigned long lastPromptTime = 0;
        unsigned long now = millis();

        // Show prompt once every 30 seconds when idle
        if (now - lastPromptTime > 30000) {
            Serial.print("\n> ");  // Simple prompt
            promptShown = true;
            lastPromptTime = now;
        }
    }

    // Update time manager (GPS polling for M3L-79)
    updateTime();

    // Update LED pattern (non-blocking, dual-channel GPS + state)
    updateLEDPattern();

    // Poll button status if interrupts aren't available (fallback mode)
    // Check every 50ms to avoid excessive I2C traffic
    static unsigned long lastPoll = 0;
    static bool lastButtonState = false;  // Track previous button state
    unsigned long now = millis();

    if (now - lastPoll >= 50) {
        lastPoll = now;

        // Check if button is CURRENTLY pressed (not clicked/released)
        bool currentButtonState = button.isPressed();

        // Detect button press (transition from not-pressed to pressed)
        if (currentButtonState && !lastButtonState) {
            buttonPressed = true;  // Set flag on press (not on release)
        }

        lastButtonState = currentButtonState;  // Update state for next poll
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

        case SystemState::CONFIG:
            handleConfigState();
            break;

        case SystemState::ERROR:
            handleErrorState();
            break;
    }

    // Heartbeat logging (every 5 seconds)
    static unsigned long lastHeartbeat = 0;

    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeat = now;

        Serial.print("\u2665 Heartbeat: ");
        Serial.print(now / 1000);
        Serial.print("s uptime | Free Heap: ");
        Serial.print(ESP.getFreeHeap());
        Serial.print(" bytes | State: ");
        Serial.print(stateToString(currentState));
        Serial.print(" | Time in state: ");
        Serial.print((now - stateEntryTime) / 1000);
        Serial.println("s");
    }

    // Battery status logging (every 30 seconds) - M3L-83
    static unsigned long lastBatteryLog = 0;
    constexpr uint32_t BATTERY_LOG_INTERVAL_MS = 30000;  // 30 seconds

    if (now - lastBatteryLog >= BATTERY_LOG_INTERVAL_MS) {
        lastBatteryLog = now;
        logBatteryStatus();
    }

    // TODO (M3L-60): QR code event handling will be added
    // TODO (M3L-61): IMU data buffering and SD writes will be added
}
