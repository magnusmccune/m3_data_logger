/**
 * @file network_manager.h
 * @brief WiFi and MQTT network configuration management
 *
 * Implements hybrid storage for network credentials using ESP32 NVS (Preferences)
 * for critical device-specific settings and SD card for full configuration JSON.
 * Supports serial command interface for configuration management.
 *
 * Storage strategy:
 * - NVS: device_id, last_wifi_ssid (survives SD card swap)
 * - SD Card: /config/network_config.json (full WiFi + MQTT config)
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>

// Network configuration constants
constexpr uint16_t MQTT_PORT_MIN = 1;
constexpr uint16_t MQTT_PORT_MAX = 65535;

// Field length limits (aligned with QR generator for config QR compatibility)
// Note: Sizes include null terminator
constexpr uint8_t DEVICE_ID_MAX_LEN = 11;      // 10 chars + \0
constexpr uint8_t WIFI_SSID_MAX_LEN = 17;      // 16 chars + \0 (IEEE 802.11 allows 32, reduced for 220-byte QR limit)
constexpr uint8_t WIFI_PASSWORD_MAX_LEN = 17;  // 16 chars + \0 (WPA2 min 8, max 16 for QR size)
constexpr uint8_t MQTT_HOST_MAX_LEN = 41;      // 40 chars + \0 (fits most hostnames)
constexpr uint8_t MQTT_USERNAME_MAX_LEN = 11;  // 10 chars + \0 (optional)
constexpr uint8_t MQTT_PASSWORD_MAX_LEN = 11;  // 10 chars + \0 (optional)
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 5000;  // 5 second WiFi connection timeout

/**
 * @brief Network configuration structure
 * Field sizes aligned with QR generator to ensure scanned configs validate correctly
 *
 * Contains WiFi credentials and MQTT broker settings. All strings are
 * null-terminated C strings with fixed buffer sizes for stack allocation.
 */
struct NetworkConfig {
    char device_id[DEVICE_ID_MAX_LEN];
    char wifi_ssid[WIFI_SSID_MAX_LEN];
    char wifi_password[WIFI_PASSWORD_MAX_LEN];
    char mqtt_host[MQTT_HOST_MAX_LEN];
    uint16_t mqtt_port;
    char mqtt_username[MQTT_USERNAME_MAX_LEN];
    char mqtt_password[MQTT_PASSWORD_MAX_LEN];
    bool mqtt_enabled;
};

/**
 * @brief Initialize network manager
 *
 * Sets up NVS (Preferences) for device-specific storage and loads configuration
 * from SD card if available. Creates default config template on first boot.
 * Must be called after SD card initialization.
 *
 * @return true if initialization successful, false otherwise
 */
bool initializeNetworkManager();

/**
 * @brief Load network configuration from storage
 *
 * Reads full config from SD card (/config/network_config.json). If SD config
 * is missing or invalid, falls back to NVS for device_id and wifi_ssid only.
 *
 * @param config Pointer to NetworkConfig struct to populate
 * @return true if config loaded (even if incomplete), false on total failure
 */
bool loadNetworkConfig(NetworkConfig* config);

/**
 * @brief Save network configuration to storage
 *
 * Writes full config to SD card JSON file and updates NVS with device_id
 * and wifi_ssid for quick access. Validates config before saving.
 *
 * @param config Pointer to NetworkConfig struct to save
 * @return true if save successful, false on validation or write error
 */
bool saveNetworkConfig(const NetworkConfig* config);

/**
 * @brief Validate network configuration
 *
 * Checks all required fields and value ranges:
 * - Device ID: 8-32 alphanumeric + underscore
 * - WiFi SSID: 1-32 chars
 * - WiFi password: 8-64 chars (WPA2 requirement)
 * - MQTT host: 1-64 chars (DNS or IP format)
 * - MQTT port: 1-65535
 *
 * @param config Pointer to NetworkConfig struct to validate
 * @return true if config valid, false otherwise
 */
bool validateNetworkConfig(const NetworkConfig* config);

/**
 * @brief Get current configuration as JSON string
 *
 * Serializes current config for display via serial interface. Masks WiFi
 * and MQTT passwords (shows only first/last 2 chars).
 *
 * @param jsonBuffer Pointer to char buffer for JSON output (min 512 bytes)
 * @param bufferSize Size of jsonBuffer
 * @return true if serialization successful, false on error
 */
bool getNetworkConfigJSON(char* jsonBuffer, size_t bufferSize);

/**
 * @brief Reset configuration to factory defaults
 *
 * Creates default config with empty credentials and generates new device_id
 * based on ESP32 MAC address. Writes to SD card and NVS.
 *
 * @return true if reset successful, false on error
 */
bool resetNetworkConfig();

/**
 * @brief Attempt WiFi connection with stored credentials
 *
 * Non-blocking connection attempt with 5 second timeout. Uses credentials
 * from loaded configuration. Safe to call repeatedly (checks current status).
 *
 * @return true if connected, false on timeout or error
 */
bool connectWiFi();

/**
 * @brief Handle serial command for network configuration
 *
 * Parses and executes network config commands:
 * - "config show" - Display current config (masked passwords)
 * - "config set <field> <value>" - Update single field (wifi.ssid, mqtt.host, etc.)
 * - "config reset" - Reset to factory defaults
 *
 * @param command Full command string from Serial input
 */
void handleNetworkCommand(const String& command);

#endif // NETWORK_MANAGER_H
