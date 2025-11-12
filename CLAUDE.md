# CLAUDE.md - M3 Data Logger

This file provides guidance to Claude Code when working in this repository.

## Project Context

The M3 Data Logger is an IoT-based time-series data collection system built on SparkFun hardware. It enables button-triggered sensor recording sessions with QR code metadata tagging, designed to create datasets for replay, analysis, and machine learning applications.

**Technology Stack**: Arduino/C++ for embedded systems, SparkFun DataLogger IoT platform
**Primary Use Case**: Generate time-series sensor datasets with metadata framing for analysis and training

## Hardware Components

| Component | Part Number | Purpose |
|-----------|-------------|---------|
| SparkFun DataLogger IoT | DEV-22462 | Main controller board (no built-in IMU) |
| ISM330DHCX IMU Breakout | SEN-19764 | 6DoF inertial measurement unit (accel + gyro) |
| Qwiic Button - Red LED | BOB-15932 | Recording trigger and status indicator |
| Tiny Code Reader | SEN-23352 | QR code scanning for metadata capture |

All sensors connect via Qwiic (I2C) for tool-free assembly.

## Project Structure

```
m3_data_logger/
├── init_prompt.md          # Original project requirements
├── PRD.md                  # Product requirements document
├── CLAUDE.md               # This file
├── src/                    # Source code (to be created)
│   ├── main.cpp           # Main application logic
│   ├── sensor_manager.cpp # Sensor data collection
│   ├── qr_handler.cpp     # QR code processing
│   └── storage.cpp        # MicroSD data writing
├── lib/                    # External libraries
├── include/                # Header files
├── test/                   # Unit tests
├── tools/                  # Development and testing tools
│   └── qr_generator/      # QR code generation scripts
│       ├── generate_qr.py # CLI tool for metadata QR codes
│       └── README.md      # Usage instructions
├── data/                   # Sample datasets
│   └── test_qr_codes/     # Pre-generated test QR codes
└── platformio.ini          # PlatformIO configuration
```

## Development Workflow

### Setup
1. Install PlatformIO IDE or CLI
2. Clone repository and open in PlatformIO
3. Connect DataLogger IoT via USB
4. Run `pio run` to build firmware

### Build and Deploy
```bash
# Build firmware
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

### Testing
```bash
# Run unit tests (host machine)
pio test

# Run tests on device
pio test --environment logger_iot
```

## Key Concepts

### Framing and Epochs
A "frame" or "epoch" represents a single recording session bounded by:
- **Start Event**: Button press + QR code scan with metadata
- **Recording Period**: Continuous sensor sampling at defined rate
- **End Event**: Second button press to terminate session

Each frame includes:
- Unique session ID
- Start/end timestamps
- QR metadata (test name, labels)
- Sensor readings (timestamped IMU data)

### QR Code Metadata Format
QR codes encode JSON objects with session metadata:
```json
{
  "test": "walking_indoor_flat",
  "labels": ["walking", "indoor", "flat_surface"]
}
```

### Sensor Data Format
Data stored to microSD as CSV with columns:
```
session_id, timestamp_ms, accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z
```

### LED States
- **Off**: Idle, not recording
- **Blinking**: Awaiting QR code scan
- **Solid**: Currently recording data
- **Off**: Recording ended

## Architecture

### System States
```
IDLE -> AWAITING_QR -> RECORDING -> IDLE
  ^                                   |
  +-----------------------------------+
```

### Data Flow
1. Button interrupt triggers state transition
2. QR reader activated, waits for scan (timeout 30s)
3. Metadata parsed, session initialized
4. Sensor data sampled at 100Hz, buffered
5. Buffer written to microSD periodically
6. Button press ends session, closes file

### Module Responsibilities
- **Main Loop**: State machine, button handling, LED control
- **Sensor Manager**: IMU initialization, sampling, data buffering
- **QR Handler**: Code reader activation, JSON parsing, validation
- **Storage Manager**: File creation, CSV writing, SD card management

## Coding Conventions

### Style
- **Naming**: snake_case for functions/variables, PascalCase for classes
- **Indentation**: 4 spaces, no tabs
- **Line Length**: 100 characters max
- **Comments**: Doxygen-style for functions, inline for complex logic

### Example
```cpp
/**
 * @brief Initialize the IMU sensor
 * @return true if successful, false otherwise
 */
bool init_imu_sensor() {
    if (!imu.begin()) {
        Serial.println("IMU initialization failed");
        return false;
    }
    imu.setAccelRange(ISM330DHCX_ACCEL_RANGE_4_G);
    imu.setGyroRange(ISM330DHCX_GYRO_RANGE_500_DPS);
    return true;
}
```

### Error Handling
- Always check return values from sensor operations
- Log errors to Serial for debugging
- Implement graceful degradation (continue recording if non-critical sensor fails)

## Testing Strategy

### Unit Tests
- Mock hardware interfaces for host-side testing
- Test JSON parsing logic independently
- Validate state machine transitions

### Integration Tests
- Test on actual hardware with sensors connected
- Verify QR code scanning with known test codes
- Confirm data written to SD card is valid CSV

### Manual Testing Checklist
- [ ] Button press triggers state change
- [ ] LED blinks when awaiting QR scan
- [ ] QR code scan starts recording
- [ ] Sensor data appears in serial output
- [ ] Data written to microSD in correct format
- [ ] Second button press stops recording
- [ ] LED turns off after recording ends

## Common Tasks

### Add a New Sensor
1. Include sensor library in `platformio.ini`
2. Initialize sensor in `sensor_manager.cpp`
3. Add data columns to CSV format
4. Update sampling loop to read new sensor

### Change Sampling Rate
Modify `SAMPLE_RATE_HZ` constant in `sensor_manager.h`:
```cpp
constexpr uint16_t SAMPLE_RATE_HZ = 100; // 100 samples per second
```

### Modify QR Metadata Schema
Update JSON parsing in `qr_handler.cpp`:
```cpp
JsonDocument doc;
deserializeJson(doc, qr_data);
const char* test = doc["test"];
JsonArray labels = doc["labels"];
```

### View Recorded Data
1. Remove microSD from logger
2. Insert into computer
3. Open CSV files with any spreadsheet tool or Python:
```python
import pandas as pd
df = pd.read_csv("session_001.csv")
print(df.head())
```

### Generate QR Codes for Testing
Use the Python CLI tool to create metadata QR codes:
```bash
# Navigate to tools directory
cd tools/qr_generator

# Generate a QR code for a test session
python generate_qr.py --test "walking_outdoor" --labels "walking,outdoor,rough_terrain"

# Generate with output to file
python generate_qr.py --test "sitting_still" --labels "sitting,stationary" --output sitting_test.png

# Display QR in terminal (for quick testing)
python generate_qr.py --test "running_indoor" --labels "running,indoor" --display
```

The tool will:
- Validate JSON structure
- Generate QR code image (default: PNG format)
- Verify code is readable before saving
- Print JSON content for verification

## Troubleshooting

### IMU Not Detected
- Check Qwiic cable connections
- Verify I2C address (default 0x6B)
- Run I2C scanner sketch to detect devices
- Ensure proper power supply (USB or battery)

### QR Code Not Scanning
- Increase timeout in `QR_SCAN_TIMEOUT_MS`
- Verify QR code contrast and size (min 2cm x 2cm)
- Check Tiny Code Reader orientation
- Test with sample QR codes from `data/test_qr_codes/`

### SD Card Write Failures
- Format card as FAT32
- Use high-quality card (Class 10 or better)
- Check card lock switch is in unlocked position
- Verify card size is supported (32GB max for FAT32)

### Button Not Responding
- Check button wiring/Qwiic connection
- Adjust debounce delay in code (default 50ms)
- Verify button LED lights up when pressed
- Test button in isolation with simple sketch

### Serial Monitor Shows Gibberish
- Set baud rate to 115200
- Check USB cable quality
- Try different USB port
- Reset board while monitor is open

## Development Phasing

### NOW (Current Phase)
- Basic firmware structure with state machine
- Button interrupt handling
- QR code scanning and metadata parsing
- IMU data sampling at 100Hz
- MicroSD storage in CSV format
- LED status indication using button LED
- Python CLI tool for generating metadata QR codes

### NEXT (Planned Features)
- MQTT transmission of sensor data
- Additional Qwiic sensors (GPS, RFID, ENVIRONMENTAL/GAS)
- QR-based configuration (WiFi SSID, MQTT broker)
- AdaFruit NeoPixel Stick for enhanced status display
- Battery operation and power management

### LATER (Future Enhancements)
- Python tool extension to generate configuration QR codes (WiFi, MQTT settings)
- Web application for QR code generation (both metadata and config types)
- Web-based data visualization dashboard
- Cloud storage integration
- Multi-device synchronization
- Real-time data streaming to analysis tools
- Machine learning model deployment on device

## Gotchas & Debugging

### Timing Issues
The SparkFun DataLogger IoT runs on ESP32, which has two cores. Be careful with:
- ISR (interrupt service routine) functions - keep them short
- I2C operations in interrupts - use flags and handle in main loop
- Blocking delays - use `millis()` for non-blocking timing

### Memory Constraints
- ESP32 has limited heap - avoid large dynamic allocations
- Use circular buffers for sensor data to prevent fragmentation
- Monitor free heap with `ESP.getFreeHeap()` during development

### SD Card Performance
- Batch writes to reduce latency (buffer 10-20 samples)
- Close file periodically to prevent data loss on power failure
- Don't write during button interrupt - set flag and write in main loop

### JSON Parsing
- ArduinoJson requires proper memory allocation for JsonDocument
- Calculate required size with ArduinoJson Assistant tool
- Always check `deserializeJson()` return value for errors

## Recent Learnings (Auto-Updated)

### 2025-11-12: Project Initialization
**Discovered by**: documentation-specialist
**Context**: Initial CLAUDE.md creation based on init_prompt.md

**Finding**:
- Project is in early planning phase with hardware defined but no code yet
- Key design pattern: framing/epoching data with button + QR metadata
- Hardware uses Qwiic ecosystem for easy sensor expansion
- Future MQTT capability indicates need for WiFi configuration strategy

**Architecture Decision**:
State machine approach is most appropriate for this application:
1. Simplifies button-triggered workflow
2. Makes LED state indication straightforward
3. Enables clean timeout handling for QR scanning
4. Facilitates future feature additions (MQTT, config modes)

### 2025-11-12: State Machine Architecture PRD (M3L-57)
**Agent**: product-manager

**Business Objective**:
Implement foundation state management system to enable coordinated recording workflows across hardware components (button, QR scanner, IMU, SD card). This is the critical path feature - all user-facing functionality depends on reliable state transitions.

**User Segments**:
- Data collection researchers operating physical device in field
- Jobs-to-be-done: Start recording with QR metadata, handle timeouts gracefully, stop recording cleanly, troubleshoot device state

**Success Metrics**:
- Leading indicators: State transition <100ms, zero stuck states in 100-cycle stress test, stable heap across 1000 transitions
- Lagging indicators: Downstream sensor integration (M3L-58, 60, 61) requires no state machine changes, 100% manual test pass rate

**Key Design Decisions**:
1. ISR-safe architecture: Interrupts set flags only, main loop handles state transitions
2. Non-blocking timeout: AWAITING_QR uses millis() for 30s QR scan window
3. Error recovery: ERROR state with 60s auto-recovery + manual serial reset
4. Memory safety: No dynamic allocation, continuous heap monitoring
5. Observability: All transitions logged with timestamps and heap usage

**PRD Location**: `plans/prd-m3l-57-state-machine.md` (Linear: M3L-57)

### 2025-11-12: State Machine UX Design (M3L-57)
**Agent**: ux-designer

**Design Pattern**: Hardware interaction state machine with LED-based user feedback

**User Problem**:
Users need unambiguous feedback about device state when operating a single-button IoT device. Without visual differentiation, users cannot tell if the device is idle, waiting for input, recording, or has encountered an error.

**Design Approach**:
Each state has a unique LED pattern that is instantly recognizable and cannot be confused with other states. Patterns are designed for peripheral vision recognition and follow embedded systems best practices (non-blocking timing, ISR safety).

**LED Pattern Vocabulary**:
- **IDLE**: OFF - device ready
- **AWAITING_QR**: SLOW BLINK (1Hz) - waiting for QR scan
- **RECORDING**: SOLID ON - actively recording
- **SESSION_ENDING**: DOUBLE BLINK → OFF - saving data
- **TIMEOUT_ERROR**: TRIPLE BLINK → OFF - QR scan timeout
- **QR_PARSE_FAIL**: TRIPLE BLINK → SLOW BLINK - invalid QR, retry
- **SD_CARD_FULL**: RAPID BLINK (5Hz, 3s) → OFF - fatal error
- **SENSOR_INIT_FAIL**: CONTINUOUS RAPID BLINK - fatal hardware error

**Edge Cases Handled**:
- Button press during AWAITING_QR: Cancel and return to IDLE
- Multiple rapid presses: Debounced (50ms ISR + 100ms software lockout)
- QR scan after timeout: Ignored (reader deactivated on timeout entry)
- SD card removed during recording: Graceful failure with periodic buffer flush
- Power loss during recording: Minimize data loss via 2-second flush interval

**Accessibility Considerations**:
- Serial output provides structured debug logging: `[MMM:SS.mmm] LEVEL: Context - Message`
- All state transitions logged with timestamps
- LED patterns distinguishable even with peripheral vision
- Non-blocking timing prevents race conditions
- ISR-safe design (flags only, no I2C in interrupts)

**Component Reuse**: Qwiic Button LED (existing hardware)

**Design Brief Location**: `plans/design-m3l-57-state-machine.md` (Linear: M3L-57)

### 2025-11-12: State Machine Implementation (M3L-57)
**Agent**: senior-software-engineer

**What was built**:
Implemented core state machine architecture in `src/main.cpp` with four system states (IDLE, AWAITING_QR, RECORDING, ERROR), state transition management, non-blocking LED patterns, and timeout handling. This establishes the foundation for all sensor integration work.

**Key Decisions**:
1. **Non-blocking LED patterns**: Used `millis()` and `lastLEDToggle` for blink timing to avoid blocking main loop
2. **State entry/exit actions**: Centralized in `transitionState()` for consistency and debugging
3. **Timeout implementation**: `stateEntryTime` tracked per-state, checked in state handlers
4. **Memory efficiency**: No dynamic allocation, all timing variables are uint32_t
5. **Observability**: Serial logging format `[uptime_ms] STATE_CHANGE: OLD → NEW (reason) | Free Heap: X bytes`
6. **ISR preparation**: Added TODOs for future button ISR integration (M3L-58)

**Implementation Patterns**:
```cpp
// State transition with logging and actions
void transitionState(SystemState newState, const char* reason);

// Non-blocking LED pattern update (called every loop iteration)
void updateLEDPattern();

// State handlers (called every loop iteration)
void handleIdleState();
void handleAwaitingQRState();  // Checks 30s timeout
void handleRecordingState();
void handleErrorState();        // Checks 60s auto-recovery
```

**LED Pattern Timing**:
- IDLE: OFF (no timing needed)
- AWAITING_QR: Slow blink at 1Hz (500ms on, 500ms off)
- RECORDING: Solid ON (no timing needed)
- ERROR: Fast blink at 5Hz (100ms on, 100ms off)

**Gotchas**:
1. **LED state initialization**: Must set `lastLEDToggle = millis()` and `ledState = false` when entering new state to prevent initial flicker
2. **Timeout arithmetic**: Use `uint32_t timeInState = millis() - stateEntryTime` to handle millis() rollover correctly
3. **State transition idempotency**: Always check `if (oldState == newState) return;` to prevent redundant transitions
4. **Heartbeat integration**: Enhanced heartbeat now shows state name and time-in-state for debugging

**Testing Challenges**:
- Serial upload to device failed with "Invalid head of packet" error (hardware timing issue, not code)
- Code compiles cleanly: RAM 7.0% (22780 bytes), Flash 29.5% (386685 bytes)
- Manual testing requires: Hold boot button, reset device, or adjust upload_speed in platformio.ini

**Code Locations**:
- Main implementation: `src/main.cpp:48-299` (state machine functions)
- State enum: `src/main.cpp:30-35`
- Timing constants: `src/main.cpp:38-48`
- Loop integration: `src/main.cpp:301-344`

**Next Steps** (Integration Points):
- M3L-58: Button interrupt handler will call `transitionState(AWAITING_QR, "button pressed")`
- M3L-60: QR scanner will call `transitionState(RECORDING, "QR code scanned")` on success
- M3L-61: IMU sampling will start/stop based on RECORDING state entry/exit actions
- M3L-62: SD card file operations will occur in RECORDING state entry/exit actions
