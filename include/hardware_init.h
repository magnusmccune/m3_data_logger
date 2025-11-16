/**
 * hardware_init.h - Hardware Initialization for SparkFun DataLogger IoT
 *
 * CRITICAL HARDWARE REQUIREMENTS:
 *
 * 1. SD CARD (4-bit SDIO mode):
 *    - MUST use SD_MMC library (NOT SD library)
 *    - GPIO32 MUST be set HIGH before SD_MMC.begin()
 *    - GPIO32 enables 74HC4050D level shifter (U4 on board)
 *    - Failure to set GPIO32 = SD card will not mount
 *
 * 2. I2C BUS (Qwiic sensors):
 *    - SDA: GPIO21 (with 2.2kΩ pull-up on board)
 *    - SCL: GPIO22 (with 2.2kΩ pull-up on board)
 *    - Standard mode: 100kHz
 *
 * 3. PINS TO AVOID:
 *    - GPIO6-11: Connected to SPI flash (DO NOT USE)
 *    - GPIO2,4,12,13,14,15: SD card SDIO (dedicated)
 *    - GPIO0: Boot button (avoid conflicts)
 *
 * Reference: https://docs.sparkfun.com/SparkFun_DataLogger/hardware_overview/
 */

#ifndef HARDWARE_INIT_H
#define HARDWARE_INIT_H

#include <Arduino.h>

// ===== Pin Definitions =====

// SD Card (SDIO 4-bit mode)
#define SD_LEVEL_SHIFTER_EN  32   // CRITICAL: Must be HIGH before SD_MMC.begin()
#define SD_CLK               14   // SDIO Clock
#define SD_CMD               15   // SDIO Command
#define SD_D0                2    // SDIO Data 0
#define SD_D1                4    // SDIO Data 1
#define SD_D2                12   // SDIO Data 2
#define SD_D3                13   // SDIO Data 3

// I2C Bus (Qwiic)
#define I2C_SDA              21
#define I2C_SCL              22

// LEDs
#define LED_STATUS           25   // Status LED (UNUSED - using Qwiic Button LED instead)
#define LED_RGB              26   // RGB LED (NeoPixel)

// Buttons
#define BTN_BOOT             0    // Boot button (avoid in app logic)

// UART (Serial1)
#define UART_TX              16
#define UART_RX              17

// Power Management
#define QWIIC_PWR_EN         -1   // May be available (check schematic)

// I2C Device Addresses (Built-in)
#define ADDR_FUEL_GAUGE      0x36 // MAX17048 battery fuel gauge

// I2C Device Addresses (Qwiic peripherals)
#define ADDR_QWIIC_BUTTON    0x6F // SparkFun Qwiic Button - Red LED
#define ADDR_QR_READER       0x0C // Tiny Code Reader (QR scanner)
#define ADDR_GPS             0x42 // SparkFun SAM-M8Q GPS (ublox)

// Button Interrupt Pin
#define BUTTON_INT_PIN       33   // Interrupt-capable GPIO for button press detection

// ===== Function Declarations =====

/**
 * @brief Initialize SD card with proper level shifter activation
 *
 * CRITICAL: This function MUST be called before any SD card operations.
 * It activates the 74HC4050D level shifter via GPIO32.
 *
 * @return true if SD card mounted successfully, false otherwise
 */
bool initializeSDCard();

/**
 * @brief Initialize I2C bus for Qwiic sensors
 *
 * Sets up Wire library on GPIO21/22 and scans for connected devices.
 *
 * @param scanBus If true, performs I2C scan and prints detected devices
 * @return true if I2C initialized (even if no devices found), false on error
 */
bool initializeI2C(bool scanBus = true);

/**
 * @brief Scan I2C bus and print detected device addresses
 *
 * Useful for debugging sensor connections.
 *
 * @return Number of devices found on I2C bus
 */
uint8_t scanI2CBus();

/**
 * @brief Initialize RGB LED (NeoPixel)
 *
 * Sets up GPIO26 NeoPixel for dual-channel indication:
 * - Color: GPS status (green/yellow/blue/red)
 * - Pattern: State machine (breathing/blink/solid/fast)
 *
 * @return true if RGB LED initialized successfully
 */
bool initializeRGBLED();

/**
 * @brief Initialize status LED (deprecated)
 *
 * Kept for compatibility, sets GPIO25 to LOW.
 * Use initializeRGBLED() instead for RGB LED on GPIO26.
 */
void initializeStatusLED();

/**
 * @brief Print hardware information to Serial
 *
 * Displays ESP32 info, memory, flash size, etc.
 */
void printHardwareInfo();

/**
 * @brief Initialize Qwiic Button with interrupt-driven press detection
 *
 * Configures the Qwiic Button at I2C address 0x6F with:
 * - 50ms debounce time (prevents false triggers)
 * - Pressed interrupt enabled
 * - Hardware interrupt on GPIO33 (falling edge)
 *
 * This function MUST be called after initializeI2C().
 *
 * @return true if button initialized successfully, false otherwise
 */
bool initializeQwiicButton();

/**
 * @brief Initialize Tiny Code Reader (QR scanner)
 *
 * Configures the Tiny Code Reader at I2C address 0x0C for QR code scanning.
 * This function MUST be called after initializeI2C().
 *
 * @return true if QR reader initialized successfully, false otherwise
 */
bool initializeQRReader();

/**
 * @brief Initialize GPS module (SAM-M8Q)
 *
 * Configures the SparkFun SAM-M8Q GPS at I2C address 0x42 for time synchronization.
 * This function MUST be called after initializeI2C().
 *
 * Cold start: 30+ seconds to acquire satellite lock
 * Warm start: 5-10 seconds with valid almanac data
 * Indoor: May not achieve lock, millis() fallback used automatically
 *
 * @return true if GPS initialized successfully, false otherwise
 */
bool initializeGPS();

// ===== External Object Declarations =====

// Forward declare QwiicButton class to avoid including full header
class QwiicButton;

// Forward declare SFE_UBLOX_GNSS class to avoid including full header
class SFE_UBLOX_GNSS;

// Forward declare Adafruit_NeoPixel class to avoid including full header
class Adafruit_NeoPixel;

// Global button object (defined in hardware_init.cpp)
// Accessible from main.cpp for interrupt handling
extern QwiicButton button;

// Global GPS object (defined in hardware_init.cpp)
// Accessible from time_manager.cpp for time synchronization
extern SFE_UBLOX_GNSS gps;

// Global RGB LED object (defined in hardware_init.cpp)
// Accessible from main.cpp for dual-channel indication (GPS status + state machine)
extern Adafruit_NeoPixel rgbLED;

#endif // HARDWARE_INIT_H
