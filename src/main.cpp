/**
 * M3 Data Logger - Main Application
 *
 * Hardware: SparkFun DataLogger IoT (DEV-22462)
 * - ESP32 microcontroller
 * - MicroSD card slot
 * - Qwiic I2C connector for sensors
 *
 * Sensors (via Qwiic/I2C):
 * - ISM330DHCX 6DoF IMU
 * - Qwiic Button with LED
 * - Tiny Code Reader (QR scanner)
 */

#include <Arduino.h>

// Pin Definitions (based on ESP32 DataLogger IoT)
#define LED_BUILTIN 2  // On-board LED for status indication

void setup() {
    // Initialize serial communication for debugging
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        ; // Wait for serial port to connect (max 3 seconds)
    }

    Serial.println();
    Serial.println("==================================");
    Serial.println("  M3 Data Logger - Initializing");
    Serial.println("==================================");
    Serial.println();

    // Initialize built-in LED
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // Print board information
    Serial.println("Board: SparkFun DataLogger IoT (DEV-22462)");
    Serial.println("MCU: ESP32");
    Serial.print("CPU Frequency: ");
    Serial.print(getCpuFrequencyMhz());
    Serial.println(" MHz");
    Serial.print("Flash Size: ");
    Serial.print(ESP.getFlashChipSize() / (1024 * 1024));
    Serial.println(" MB");
    Serial.print("Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    Serial.println();

    // TODO: Initialize I2C bus (Wire library)
    // TODO: Initialize SD card
    // TODO: Initialize sensors (IMU, Button, QR Reader)
    // TODO: Set up state machine

    Serial.println("✓ Serial initialized (115200 baud)");
    Serial.println("✓ Board initialization complete");
    Serial.println();
    Serial.println("Ready for sensor integration...");
    Serial.println("==================================");

    // Blink LED to indicate successful initialization
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN, LOW);
        delay(200);
    }
}

void loop() {
    // Main loop - currently just heartbeat
    static unsigned long lastHeartbeat = 0;
    unsigned long now = millis();

    if (now - lastHeartbeat >= 5000) {  // Every 5 seconds
        lastHeartbeat = now;
        Serial.print("Heartbeat: ");
        Serial.print(now / 1000);
        Serial.println(" seconds uptime");

        // Toggle LED
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
}
