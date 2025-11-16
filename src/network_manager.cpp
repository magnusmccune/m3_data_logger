/**
 * @file network_manager.cpp
 * @brief WiFi and MQTT network configuration management implementation
 *
 * Implements hybrid storage using NVS (Preferences API) for critical device
 * settings and SD card JSON for full configuration. Provides serial command
 * interface for configuration management without reflashing.
 */

#include "network_manager.h"
#include <SD_MMC.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// NVS namespace for persistent storage
static Preferences prefs;

// Current configuration (cached in RAM)
static NetworkConfig currentConfig;
static bool configLoaded = false;

// Constants
static const char* CONFIG_FILE_PATH = "/config/network_config.json";
static const char* NVS_NAMESPACE = "m3logger";
static const char* NVS_KEY_DEVICE_ID = "device_id";
static const char* NVS_KEY_WIFI_SSID = "wifi_ssid";

/**
 * @brief Generate default device ID from ESP32 MAC address
 */
static void generateDefaultDeviceID(char* deviceID) {
    // Generate device_id from MAC address (format: m3l_XXXXXX, 10 chars max)
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(deviceID, DEVICE_ID_MAX_LEN, "m3l_%02x%02x%02x",
             mac[3], mac[4], mac[5]);  // Last 3 bytes of MAC = 6 hex chars
}

/**
 * @brief Create default network configuration
 */
static void createDefaultConfig(NetworkConfig* config) {
    memset(config, 0, sizeof(NetworkConfig));

    // Generate device ID from MAC
    generateDefaultDeviceID(config->device_id);

    // Set default MQTT port
    config->mqtt_port = 1883;
    config->mqtt_enabled = false;

    Serial.printf("[Network] Created default config with device_id: %s\n", config->device_id);
}

/**
 * @brief Write default config template to SD card
 */
static bool writeDefaultConfigFile() {
    NetworkConfig defaultConfig;
    createDefaultConfig(&defaultConfig);

    // Create /config directory if needed
    if (!SD_MMC.exists("/config")) {
        if (!SD_MMC.mkdir("/config")) {
            Serial.println("[Network] ERROR: Failed to create /config directory");
            return false;
        }
        Serial.println("[Network] Created /config directory");
    }

    // Create JSON document
    StaticJsonDocument<1024> doc;
    doc["version"] = "1.0";
    doc["device_id"] = defaultConfig.device_id;

    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["ssid"] = "";
    wifi["password"] = "";

    JsonObject mqtt = doc.createNestedObject("mqtt");
    mqtt["host"] = "";
    mqtt["port"] = 1883;
    mqtt["username"] = "";
    mqtt["password"] = "";
    mqtt["enabled"] = false;

    // Write to SD card
    File configFile = SD_MMC.open(CONFIG_FILE_PATH, FILE_WRITE);
    if (!configFile) {
        Serial.println("[Network] ERROR: Failed to create config file");
        return false;
    }

    if (serializeJsonPretty(doc, configFile) == 0) {
        Serial.println("[Network] ERROR: Failed to write config JSON");
        configFile.close();
        return false;
    }

    configFile.close();
    Serial.printf("[Network] Default config written to %s\n", CONFIG_FILE_PATH);
    return true;
}

/**
 * @brief Load configuration from SD card JSON file
 */
static bool loadConfigFromSD(NetworkConfig* config) {
    File configFile = SD_MMC.open(CONFIG_FILE_PATH, FILE_READ);
    if (!configFile) {
        Serial.println("[Network] Config file not found on SD card");
        return false;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();

    if (error) {
        Serial.printf("[Network] ERROR: Failed to parse config JSON: %s\n", error.c_str());
        return false;
    }

    // Parse device_id
    const char* deviceID = doc["device_id"];
    if (deviceID) {
        strncpy(config->device_id, deviceID, DEVICE_ID_MAX_LEN - 1);
        config->device_id[DEVICE_ID_MAX_LEN - 1] = '\0';
    }

    // Parse WiFi settings
    if (doc.containsKey("wifi")) {
        JsonObject wifi = doc["wifi"];

        const char* ssid = wifi["ssid"];
        if (ssid) {
            strncpy(config->wifi_ssid, ssid, WIFI_SSID_MAX_LEN - 1);
            config->wifi_ssid[WIFI_SSID_MAX_LEN - 1] = '\0';
        }

        const char* password = wifi["password"];
        if (password) {
            strncpy(config->wifi_password, password, WIFI_PASSWORD_MAX_LEN - 1);
            config->wifi_password[WIFI_PASSWORD_MAX_LEN - 1] = '\0';
        }
    }

    // Parse MQTT settings
    if (doc.containsKey("mqtt")) {
        JsonObject mqtt = doc["mqtt"];

        const char* host = mqtt["host"];
        if (host) {
            strncpy(config->mqtt_host, host, MQTT_HOST_MAX_LEN - 1);
            config->mqtt_host[MQTT_HOST_MAX_LEN - 1] = '\0';
        }

        config->mqtt_port = mqtt["port"] | 1883;  // Default 1883 if missing

        const char* username = mqtt["username"];
        if (username) {
            strncpy(config->mqtt_username, username, MQTT_USERNAME_MAX_LEN - 1);
            config->mqtt_username[MQTT_USERNAME_MAX_LEN - 1] = '\0';
        }

        const char* password = mqtt["password"];
        if (password) {
            strncpy(config->mqtt_password, password, MQTT_PASSWORD_MAX_LEN - 1);
            config->mqtt_password[MQTT_PASSWORD_MAX_LEN - 1] = '\0';
        }

        config->mqtt_enabled = mqtt["enabled"] | false;
    }

    Serial.println("[Network] Config loaded from SD card");
    return true;
}

/**
 * @brief Save configuration to SD card JSON file
 */
static bool saveConfigToSD(const NetworkConfig* config) {
    // Create JSON document
    StaticJsonDocument<1024> doc;
    doc["version"] = "1.0";
    doc["device_id"] = config->device_id;

    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["ssid"] = config->wifi_ssid;
    wifi["password"] = config->wifi_password;

    JsonObject mqtt = doc.createNestedObject("mqtt");
    mqtt["host"] = config->mqtt_host;
    mqtt["port"] = config->mqtt_port;
    mqtt["username"] = config->mqtt_username;
    mqtt["password"] = config->mqtt_password;
    mqtt["enabled"] = config->mqtt_enabled;

    // Write to SD card
    File configFile = SD_MMC.open(CONFIG_FILE_PATH, FILE_WRITE);
    if (!configFile) {
        Serial.println("[Network] ERROR: Failed to open config file for writing");
        return false;
    }

    if (serializeJsonPretty(doc, configFile) == 0) {
        Serial.println("[Network] ERROR: Failed to write config JSON");
        configFile.close();
        return false;
    }

    configFile.close();
    Serial.println("[Network] Config saved to SD card");
    return true;
}

/**
 * @brief Load device_id and wifi_ssid from NVS
 */
static void loadCriticalSettingsFromNVS(NetworkConfig* config) {
    if (!prefs.begin(NVS_NAMESPACE, true)) {  // Read-only mode
        Serial.println("[Network] WARNING: Failed to open NVS");
        return;
    }

    String deviceID = prefs.getString(NVS_KEY_DEVICE_ID, "");
    if (deviceID.length() > 0) {
        strncpy(config->device_id, deviceID.c_str(), DEVICE_ID_MAX_LEN - 1);
        config->device_id[DEVICE_ID_MAX_LEN - 1] = '\0';
    }

    String wifiSSID = prefs.getString(NVS_KEY_WIFI_SSID, "");
    if (wifiSSID.length() > 0) {
        strncpy(config->wifi_ssid, wifiSSID.c_str(), WIFI_SSID_MAX_LEN - 1);
        config->wifi_ssid[WIFI_SSID_MAX_LEN - 1] = '\0';
    }

    prefs.end();
}

/**
 * @brief Save device_id and wifi_ssid to NVS
 */
static bool saveCriticalSettingsToNVS(const NetworkConfig* config) {
    if (!prefs.begin(NVS_NAMESPACE, false)) {  // Read-write mode
        Serial.println("[Network] ERROR: Failed to open NVS for writing");
        return false;
    }

    prefs.putString(NVS_KEY_DEVICE_ID, config->device_id);
    prefs.putString(NVS_KEY_WIFI_SSID, config->wifi_ssid);

    prefs.end();
    Serial.println("[Network] Critical settings saved to NVS");
    return true;
}

// Public API implementation

bool initializeNetworkManager() {
    Serial.println("[Network] Initializing network manager...");

    // Ensure /config directory exists
    if (!SD_MMC.exists("/config")) {
        if (!SD_MMC.mkdir("/config")) {
            Serial.println("[Network] ERROR: Failed to create /config directory");
            return false;
        }
        Serial.println("[Network] Created /config directory");
    }

    // Create default config file if missing
    if (!SD_MMC.exists(CONFIG_FILE_PATH)) {
        Serial.println("[Network] Config file missing, creating default template");
        if (!writeDefaultConfigFile()) {
            return false;
        }
    }

    // Load configuration
    if (!loadNetworkConfig(&currentConfig)) {
        Serial.println("[Network] WARNING: Failed to load config, using defaults");
        createDefaultConfig(&currentConfig);
    }

    configLoaded = true;
    Serial.println("[Network] Network manager initialized");
    return true;
}

bool loadNetworkConfig(NetworkConfig* config) {
    if (!config) {
        return false;
    }

    // Start with defaults
    createDefaultConfig(config);

    // Try SD card first (full config)
    bool sdLoaded = loadConfigFromSD(config);

    // If SD load failed or incomplete, try NVS for critical settings
    if (!sdLoaded || strlen(config->device_id) == 0) {
        loadCriticalSettingsFromNVS(config);
    }

    // Ensure we have at least a device_id
    if (strlen(config->device_id) == 0) {
        generateDefaultDeviceID(config->device_id);
        Serial.printf("[Network] Generated device_id: %s\n", config->device_id);
    }

    return true;  // Always return true, even with partial config
}

bool saveNetworkConfig(const NetworkConfig* config) {
    if (!config) {
        return false;
    }

    // Validate before saving
    if (!validateNetworkConfig(config)) {
        Serial.println("[Network] ERROR: Config validation failed");
        return false;
    }

    // Save to both SD card and NVS
    bool sdSaved = saveConfigToSD(config);
    bool nvsSaved = saveCriticalSettingsToNVS(config);

    if (sdSaved && nvsSaved) {
        // Update cached config
        memcpy(&currentConfig, config, sizeof(NetworkConfig));
        Serial.println("[Network] Config saved successfully");
        return true;
    }

    Serial.println("[Network] ERROR: Failed to save config");
    return false;
}

bool validateNetworkConfig(const NetworkConfig* config) {
    if (!config) {
        return false;
    }

    // Validate device ID (1-10 alphanumeric + underscore)
    size_t deviceIdLen = strlen(config->device_id);
    if (deviceIdLen < 1 || deviceIdLen >= DEVICE_ID_MAX_LEN) {
        Serial.printf("[Network] ERROR: Invalid device_id length: %d (must be 1-10 chars)\n", deviceIdLen);
        return false;
    }

    for (size_t i = 0; i < deviceIdLen; i++) {
        char c = config->device_id[i];
        if (!isalnum(c) && c != '_') {
            Serial.printf("[Network] ERROR: Invalid character in device_id: '%c'\n", c);
            return false;
        }
    }

    // Validate WiFi SSID (1-16 chars, printable ASCII per IEEE 802.11)
    size_t ssidLen = strlen(config->wifi_ssid);
    if (ssidLen < 1 || ssidLen >= WIFI_SSID_MAX_LEN) {
        Serial.printf("[Network] ERROR: Invalid SSID length: %d (must be 1-16 chars)\n", ssidLen);
        return false;
    }

    // Validate WiFi password (8-16 chars for WPA2 + QR size limit)
    size_t pwdLen = strlen(config->wifi_password);
    if (pwdLen > 0 && (pwdLen < 8 || pwdLen >= WIFI_PASSWORD_MAX_LEN)) {
        Serial.printf("[Network] ERROR: Invalid password length: %d (must be 8-16 chars for WPA2)\n", pwdLen);
        return false;
    }

    // Validate MQTT port (1-65535)
    if (config->mqtt_port < MQTT_PORT_MIN || config->mqtt_port > MQTT_PORT_MAX) {
        Serial.printf("[Network] ERROR: Invalid MQTT port: %d\n", config->mqtt_port);
        return false;
    }

    // Validate MQTT host (1-40 chars)
    size_t hostLen = strlen(config->mqtt_host);
    if (hostLen < 1 || hostLen >= MQTT_HOST_MAX_LEN) {
        Serial.printf("[Network] ERROR: Invalid MQTT host length: %d (must be 1-40 chars)\n", hostLen);
        return false;
    }

    // Validate MQTT username (0-10 chars, optional)
    size_t usernameLen = strlen(config->mqtt_username);
    if (usernameLen >= MQTT_USERNAME_MAX_LEN) {
        Serial.printf("[Network] ERROR: Invalid MQTT username length: %d (must be 0-10 chars)\n", usernameLen);
        return false;
    }

    // Validate MQTT password (0-10 chars, optional)
    size_t mqttPwdLen = strlen(config->mqtt_password);
    if (mqttPwdLen >= MQTT_PASSWORD_MAX_LEN) {
        Serial.printf("[Network] ERROR: Invalid MQTT password length: %d (must be 0-10 chars)\n", mqttPwdLen);
        return false;
    }

    return true;
}

bool getNetworkConfigJSON(char* jsonBuffer, size_t bufferSize) {
    if (!jsonBuffer || bufferSize < 512) {
        return false;
    }

    if (!configLoaded) {
        strncpy(jsonBuffer, "{\"error\":\"Config not loaded\"}", bufferSize);
        return false;
    }

    StaticJsonDocument<1024> doc;
    doc["version"] = "1.0";
    doc["device_id"] = currentConfig.device_id;

    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["ssid"] = currentConfig.wifi_ssid;

    // Mask WiFi password (show first/last 2 chars only)
    size_t pwdLen = strlen(currentConfig.wifi_password);
    if (pwdLen > 0) {
        char maskedPwd[16];
        if (pwdLen <= 4) {
            strcpy(maskedPwd, "****");
        } else {
            snprintf(maskedPwd, sizeof(maskedPwd), "%c%c****%c%c",
                     currentConfig.wifi_password[0],
                     currentConfig.wifi_password[1],
                     currentConfig.wifi_password[pwdLen - 2],
                     currentConfig.wifi_password[pwdLen - 1]);
        }
        wifi["password"] = maskedPwd;
    } else {
        wifi["password"] = "";
    }

    JsonObject mqtt = doc.createNestedObject("mqtt");
    mqtt["host"] = currentConfig.mqtt_host;
    mqtt["port"] = currentConfig.mqtt_port;
    mqtt["username"] = currentConfig.mqtt_username;

    // Mask MQTT password
    size_t mqttPwdLen = strlen(currentConfig.mqtt_password);
    if (mqttPwdLen > 0) {
        char maskedPwd[16];
        if (mqttPwdLen <= 4) {
            strcpy(maskedPwd, "****");
        } else {
            snprintf(maskedPwd, sizeof(maskedPwd), "%c%c****%c%c",
                     currentConfig.mqtt_password[0],
                     currentConfig.mqtt_password[1],
                     currentConfig.mqtt_password[mqttPwdLen - 2],
                     currentConfig.mqtt_password[mqttPwdLen - 1]);
        }
        mqtt["password"] = maskedPwd;
    } else {
        mqtt["password"] = "";
    }

    mqtt["enabled"] = currentConfig.mqtt_enabled;

    // Serialize to buffer
    size_t written = serializeJsonPretty(doc, jsonBuffer, bufferSize);
    return written > 0;
}

bool resetNetworkConfig() {
    Serial.println("[Network] Resetting config to factory defaults...");

    NetworkConfig defaultConfig;
    createDefaultConfig(&defaultConfig);

    return saveNetworkConfig(&defaultConfig);
}

bool connectWiFi() {
    // Check if already connected
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[Network] WiFi already connected");
        return true;
    }

    // Check if we have credentials
    if (strlen(currentConfig.wifi_ssid) == 0) {
        Serial.println("[Network] No WiFi SSID configured");
        return false;
    }

    Serial.printf("[Network] Connecting to WiFi: %s\n", currentConfig.wifi_ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(currentConfig.wifi_ssid, currentConfig.wifi_password);

    // Non-blocking wait with timeout
    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT_MS) {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[Network] WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.println("[Network] WiFi connection timeout");
    return false;
}

/**
 * Handle serial network configuration commands
 *
 * Supports:
 *   config show - Display current config
 *   config set <field> <value> - Update field
 *   config reset - Reset to defaults
 *
 * Note: Field limits match QR generator to ensure config QR codes
 * generated by tools/qr_generator can be scanned and validated.
 */
void handleNetworkCommand(const String& command) {
    String cmd = command;
    cmd.trim();
    cmd.toLowerCase();

    if (cmd.startsWith("config show")) {
        char jsonBuffer[1024];
        if (getNetworkConfigJSON(jsonBuffer, sizeof(jsonBuffer))) {
            Serial.println("[Network] Current configuration:");
            Serial.println(jsonBuffer);
        } else {
            Serial.println("[Network] ERROR: Failed to get config JSON");
        }
        return;
    }

    if (cmd.startsWith("config reset")) {
        if (resetNetworkConfig()) {
            Serial.println("[Network] Config reset to factory defaults");
        } else {
            Serial.println("[Network] ERROR: Failed to reset config");
        }
        return;
    }

    if (cmd.startsWith("config set ")) {
        // Parse: config set <field> <value>
        String remaining = command.substring(11);  // Skip "config set "
        remaining.trim();

        int spaceIndex = remaining.indexOf(' ');
        if (spaceIndex == -1) {
            Serial.println("[Network] ERROR: Usage: config set <field> <value>");
            return;
        }

        String field = remaining.substring(0, spaceIndex);
        String value = remaining.substring(spaceIndex + 1);
        field.trim();
        value.trim();

        // Update current config
        NetworkConfig newConfig = currentConfig;
        bool updated = false;

        if (field.equalsIgnoreCase("device_id")) {
            strncpy(newConfig.device_id, value.c_str(), DEVICE_ID_MAX_LEN - 1);
            newConfig.device_id[DEVICE_ID_MAX_LEN - 1] = '\0';
            updated = true;
        } else if (field.equalsIgnoreCase("wifi.ssid")) {
            strncpy(newConfig.wifi_ssid, value.c_str(), WIFI_SSID_MAX_LEN - 1);
            newConfig.wifi_ssid[WIFI_SSID_MAX_LEN - 1] = '\0';
            updated = true;
        } else if (field.equalsIgnoreCase("wifi.password")) {
            strncpy(newConfig.wifi_password, value.c_str(), WIFI_PASSWORD_MAX_LEN - 1);
            newConfig.wifi_password[WIFI_PASSWORD_MAX_LEN - 1] = '\0';
            updated = true;
        } else if (field.equalsIgnoreCase("mqtt.host")) {
            strncpy(newConfig.mqtt_host, value.c_str(), MQTT_HOST_MAX_LEN - 1);
            newConfig.mqtt_host[MQTT_HOST_MAX_LEN - 1] = '\0';
            updated = true;
        } else if (field.equalsIgnoreCase("mqtt.port")) {
            newConfig.mqtt_port = value.toInt();
            updated = true;
        } else if (field.equalsIgnoreCase("mqtt.username")) {
            strncpy(newConfig.mqtt_username, value.c_str(), MQTT_USERNAME_MAX_LEN - 1);
            newConfig.mqtt_username[MQTT_USERNAME_MAX_LEN - 1] = '\0';
            updated = true;
        } else if (field.equalsIgnoreCase("mqtt.password")) {
            strncpy(newConfig.mqtt_password, value.c_str(), MQTT_PASSWORD_MAX_LEN - 1);
            newConfig.mqtt_password[MQTT_PASSWORD_MAX_LEN - 1] = '\0';
            updated = true;
        } else if (field.equalsIgnoreCase("mqtt.enabled")) {
            newConfig.mqtt_enabled = (value.equalsIgnoreCase("true") || value == "1");
            updated = true;
        } else {
            Serial.printf("[Network] ERROR: Unknown field: %s\n", field.c_str());
            Serial.println("[Network] Valid fields: device_id, wifi.ssid, wifi.password, mqtt.host, mqtt.port, mqtt.username, mqtt.password, mqtt.enabled");
            return;
        }

        if (updated) {
            if (saveNetworkConfig(&newConfig)) {
                Serial.printf("[Network] Updated %s\n", field.c_str());
            } else {
                Serial.println("[Network] ERROR: Failed to save config");
            }
        }
        return;
    }

    Serial.println("[Network] ERROR: Unknown command");
    Serial.println("[Network] Valid commands:");
    Serial.println("  config show");
    Serial.println("  config set <field> <value>");
    Serial.println("  config reset");
}
