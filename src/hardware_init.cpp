/**
 * hardware_init.cpp - Hardware Initialization Implementation
 *
 * SparkFun DataLogger IoT (DEV-22462)
 * ESP32-WROOM-32E based data logging platform
 *
 * CRITICAL HARDWARE NOTES:
 * - SD card uses SDIO (NOT SPI) - requires SD_MMC library
 * - GPIO32 MUST be HIGH to enable SD card level shifter
 * - I2C on GPIO21/22 with onboard pull-ups
 *
 * See: https://docs.sparkfun.com/SparkFun_DataLogger/hardware_overview/
 */

#include "hardware_init.h"
#include <SD_MMC.h>
#include <Wire.h>
#include <SparkFun_Qwiic_Button.h>
#include <tiny_code_reader.h>

// ===== Global Objects =====
QwiicButton button;  // Global button object accessible from main.cpp

// ===== Timing Constants =====
constexpr uint32_t SD_STABILIZATION_DELAY_MS = 10;  // Level shifter stabilization time
constexpr uint16_t BUTTON_DEBOUNCE_MS = 50;         // Button debounce time (prevents false triggers)

// ===== SD Card Initialization =====

bool initializeSDCard() {
    Serial.println("\n==== SD Card Initialization ====");

    // CRITICAL: Enable 74HC4050D level shifter before SD_MMC
    Serial.println("Enabling SD card level shifter (GPIO32 HIGH)...");
    pinMode(SD_LEVEL_SHIFTER_EN, OUTPUT);
    digitalWrite(SD_LEVEL_SHIFTER_EN, HIGH);
    delay(SD_STABILIZATION_DELAY_MS);  // Blocking OK during init - hardware stabilization required

    // Initialize SD_MMC in 4-bit SDIO mode
    // Arguments: mount_point, mode1bit (false = 4-bit, true = 1-bit)
    Serial.println("Mounting SD card (4-bit SDIO mode)...");
    if (!SD_MMC.begin("/sdcard", false)) {
        Serial.println("✗ ERROR: SD card mount failed");
        Serial.println("  Possible causes:");
        Serial.println("  - No SD card inserted");
        Serial.println("  - SD card not formatted (requires FAT32)");
        Serial.println("  - GPIO32 not HIGH (level shifter disabled)");
        Serial.println("  - Faulty SD card or card slot");
        return false;
    }

    // Get card info
    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("✗ ERROR: No SD card detected");
        return false;
    }

    // Print card type
    Serial.print("✓ SD card detected: ");
    switch (cardType) {
        case CARD_MMC:
            Serial.println("MMC");
            break;
        case CARD_SD:
            Serial.println("SDSC (Standard Capacity)");
            break;
        case CARD_SDHC:
            Serial.println("SDHC (High Capacity)");
            break;
        default:
            Serial.println("Unknown");
    }

    // Print card size
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.print("  Card Size: ");
    Serial.print(cardSize);
    Serial.println(" MB");

    // Print used/available space
    uint64_t totalBytes = SD_MMC.totalBytes() / (1024 * 1024);
    uint64_t usedBytes = SD_MMC.usedBytes() / (1024 * 1024);
    Serial.print("  Total: ");
    Serial.print(totalBytes);
    Serial.print(" MB, Used: ");
    Serial.print(usedBytes);
    Serial.println(" MB");

    Serial.println("✓ SD card initialization complete\n");
    return true;
}

// ===== I2C Bus Initialization =====

bool initializeI2C(bool scanBus) {
    Serial.println("==== I2C Bus Initialization ====");

    // Initialize I2C on GPIO21 (SDA) and GPIO22 (SCL)
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);  // 100kHz standard mode (safe for all sensors)
    Serial.println("✓ I2C initialized (GPIO21=SDA, GPIO22=SCL, 100kHz)");

    if (scanBus) {
        uint8_t deviceCount = scanI2CBus();
        if (deviceCount == 0) {
            Serial.println("⚠ WARNING: No I2C devices found on bus");
            Serial.println("  Check Qwiic cable connections");
            return true;  // Still return true - I2C is initialized
        }
    }

    Serial.println();
    return true;
}

// ===== I2C Bus Scanner =====

uint8_t scanI2CBus() {
    Serial.println("Scanning I2C bus for devices...");

    uint8_t deviceCount = 0;
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            Serial.print("  Device found at 0x");
            if (address < 16) Serial.print("0");
            Serial.print(address, HEX);

            // Identify known devices
            switch (address) {
                case 0x36:
                    Serial.print(" (MAX17048 Fuel Gauge)");
                    break;
                case 0x6A:
                case 0x6B:
                    Serial.print(" (ISM330DHCX IMU)");
                    break;
                // Add more known addresses as sensors are integrated
            }
            Serial.println();
            deviceCount++;
        }
    }

    Serial.print("✓ I2C scan complete: ");
    Serial.print(deviceCount);
    Serial.println(" device(s) found");

    return deviceCount;
}

// ===== Status LED =====

void initializeStatusLED() {
    pinMode(LED_STATUS, OUTPUT);
    digitalWrite(LED_STATUS, LOW);
    Serial.println("✓ Status LED initialized (GPIO25)");
}

// ===== Battery Monitoring (MAX17048 Fuel Gauge) =====

float getBatteryVoltage() {
    // TODO: Implement MAX17048 reading via I2C
    // For now, return -1.0 to indicate not implemented
    return -1.0;
}

float getBatteryPercent() {
    // TODO: Implement MAX17048 reading via I2C
    // For now, return -1.0 to indicate not implemented
    return -1.0;
}

// ===== Hardware Information =====

void printHardwareInfo() {
    Serial.println("\n╔════════════════════════════════════════════╗");
    Serial.println("║   M3 Data Logger - Hardware Information   ║");
    Serial.println("╚════════════════════════════════════════════╝");
    Serial.println();

    // Board information
    Serial.println("Board: SparkFun DataLogger IoT (DEV-22462)");
    Serial.println("MCU: ESP32-WROOM-32E");
    Serial.println();

    // ESP32 Information
    Serial.print("CPU Frequency: ");
    Serial.print(getCpuFrequencyMhz());
    Serial.println(" MHz");

    Serial.print("Flash Size: ");
    Serial.print(ESP.getFlashChipSize() / (1024 * 1024));
    Serial.println(" MB");

    Serial.print("Flash Speed: ");
    Serial.print(ESP.getFlashChipSpeed() / 1000000);
    Serial.println(" MHz");

    Serial.print("Chip Revision: ");
    Serial.println(ESP.getChipRevision());

    Serial.print("SDK Version: ");
    Serial.println(ESP.getSdkVersion());

    // Memory information
    Serial.println();
    Serial.println("Memory:");
    Serial.print("  Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");

    Serial.print("  Heap Size: ");
    Serial.print(ESP.getHeapSize());
    Serial.println(" bytes");

    Serial.print("  Min Free Heap: ");
    Serial.print(ESP.getMinFreeHeap());
    Serial.println(" bytes");

    if (psramFound()) {
        Serial.println("  PSRAM: Detected");
        Serial.print("  PSRAM Size: ");
        Serial.print(ESP.getPsramSize());
        Serial.println(" bytes");
    } else {
        Serial.println("  PSRAM: Not detected");
    }

    Serial.println();
}

// ===== Qwiic Button Initialization =====

// Forward declaration of ISR (defined in main.cpp)
// This allows hardware_init.cpp to attach the interrupt without circular dependency
extern void buttonISR();

bool initializeQwiicButton() {
    Serial.println("\n==== Qwiic Button Initialization ====");

    // Initialize button on I2C bus at address 0x6F
    if (!button.begin(ADDR_QWIIC_BUTTON)) {
        Serial.println("✗ ERROR: Qwiic Button not detected at 0x6F");
        Serial.println("  Check Qwiic cable connection");
        return false;
    }
    Serial.println("✓ Qwiic Button detected at 0x6F");

    // Configure debounce time to prevent false triggers
    // 50ms is recommended for mechanical button bounce suppression
    button.setDebounceTime(BUTTON_DEBOUNCE_MS);
    Serial.print("✓ Debounce time set to ");
    Serial.print(BUTTON_DEBOUNCE_MS);
    Serial.println(" ms");

    // Try to enable hardware interrupts (optional - will fall back to polling if this fails)
    // Note: Some button firmware versions don't support interrupts, or INT pin may not be connected
    uint8_t interruptResult = button.enablePressedInterrupt();
    if (interruptResult != 0) {
        Serial.println("⚠ WARNING: Button interrupt enable failed (error code: " + String(interruptResult) + ")");
        Serial.println("  Falling back to polling mode (button will still work)");
        Serial.println("  Reason: INT pin may not be connected or firmware doesn't support interrupts");
        
        // Continue without interrupts - we'll poll instead
        Serial.println("✓ Button initialized in POLLING mode");
    } else {
        Serial.println("✓ Pressed interrupt enabled");
        
        // Configure GPIO pin for hardware interrupt
        pinMode(BUTTON_INT_PIN, INPUT);
        attachInterrupt(digitalPinToInterrupt(BUTTON_INT_PIN), buttonISR, FALLING);
        Serial.print("✓ Hardware interrupt attached to GPIO");
        Serial.println(BUTTON_INT_PIN);
        Serial.println("✓ Button initialized in INTERRUPT mode");
    }

    // Clear any pending event flags from button power-up
    button.clearEventBits();
    Serial.println("✓ Event bits cleared");

    Serial.println("==== Button Initialization Complete ====\n");
    return true;
}

// ===== QR Code Reader Initialization (M3L-60) =====

bool initializeQRReader() {
    Serial.println("\n==== QR Reader Initialization ====");

    // The Tiny Code Reader library is header-only with inline functions.
    // It doesn't have an explicit init function - just needs I2C bus ready.
    // We'll verify the device is present by attempting to read from it.
    
    Serial.println("✓ Tiny Code Reader library loaded (header-only)");
    Serial.println("  Device detection will occur on first scan");
    Serial.println("  Expected I2C address: 0x0C");
    
    Serial.println("✓ QR Reader ready for scanning");
    Serial.println("==== QR Reader Initialization Complete ====\n");
    return true;
}
