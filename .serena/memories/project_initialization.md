# M3 Data Logger - Project Initialization

**Date**: 2025-11-12  
**Linear Issue**: M3L-56  
**Phase**: NOW

## Project Setup Summary

Successfully initialized the M3 Data Logger PlatformIO project for ESP32-based IoT sensor data collection.

### Directory Structure Created

```
m3_data_logger/
├── platformio.ini          # PlatformIO configuration
├── .gitignore             # Git ignore patterns
├── README.md              # Project documentation
├── PRD.md                 # Product requirements
├── CLAUDE.md              # Developer guide
├── src/
│   └── main.cpp          # Main application entry point
├── include/              # Header files (empty for now)
├── lib/                  # Custom libraries (empty for now)
├── test/                 # Unit tests (empty for now)
├── tools/
│   └── qr_generator/     # QR code generation tools (to be implemented)
└── data/
    └── test_qr_codes/    # Pre-generated test QR codes (to be added)
```

### PlatformIO Configuration (platformio.ini)

**Board**: `esp32dev` (compatible with SparkFun DataLogger IoT DEV-22462)  
**Platform**: `espressif32`  
**Framework**: `arduino`  
**Monitor Speed**: 115200 baud

**Library Dependencies**:
1. `SparkFun 6DoF ISM330DHCX ^1.0.0` - 6-axis IMU sensor
2. `SparkFun Qwiic Button ^2.0.0` - Button with integrated LED
3. `ArduinoJson ^6.21.0` - JSON parsing for QR metadata

**Note**: Tiny Code Reader library not yet added (needs research for correct library name)

### Main Application (main.cpp)

**Current Features**:
- Serial initialization at 115200 baud
- Board information display (CPU freq, flash size, heap)
- LED blink pattern on startup (3 blinks = successful init)
- Heartbeat every 5 seconds with LED toggle
- Placeholder TODOs for next implementation steps

### Next Implementation Steps

Per CLAUDE.md and Linear backlog:
1. **M3L-57**: State machine architecture (IDLE → AWAITING_QR → RECORDING)
2. **M3L-59**: I2C bus initialization for Qwiic sensors
3. **M3L-58**: Button interrupt handler with debouncing
4. **M3L-60**: QR code reader integration and JSON parsing
5. **M3L-61**: IMU sensor data collection at 100Hz

### Important Design Decisions Made

**NONE** - Per user instruction, this was pure initialization with zero architecture decisions.

### Known Issues / TODOs

1. **Tiny Code Reader Library**: Need to identify correct library for platformio.ini
2. **SD Card Library**: May need `SD_MMC` instead of standard `SD` for ESP32
3. **I2C Pin Configuration**: Verify SDA/SCL pins for DataLogger IoT board
4. **Build Verification**: User should run `pio run` to confirm configuration
