# CLAUDE.md - M3 Data Logger

This file provides guidance to Claude Code when working in this repository.

## Project Context

The M3 Data Logger is an IoT-based time-series data collection system built on SparkFun hardware. It enables button-triggered sensor recording sessions with QR code metadata tagging, designed to create datasets for replay, analysis, and machine learning applications.

**Technology Stack**: Arduino/C++ for embedded systems, SparkFun DataLogger IoT platform
**Primary Use Case**: Generate time-series sensor datasets with metadata framing for analysis and training

## Hardware Components

| Component | Part Number | I2C Address | Purpose |
|-----------|-------------|-------------|---------|
| SparkFun DataLogger IoT | DEV-22462 | N/A | Main controller board (ESP32-WROVER, no built-in IMU) |
| ISM330DHCX IMU Breakout | SEN-19764 | 0x6B | 6DoF inertial measurement unit (accel + gyro) |
| Qwiic Button - Red LED | BOB-15932 | 0x6F | Recording trigger and status indicator |
| Tiny Code Reader | SEN-23352 | 0x0C | QR code scanning for metadata capture |
| MAX17048 Fuel Gauge | Onboard | 0x36 | LiPo battery monitoring (built into DataLogger) |

**Onboard Features**:
- MicroSD card slot with level shifter (GPIO32 control required)
- LiPo battery connector and charging circuitry
- USB-C connector for programming and power
- Unknown device at 0x7E (detected during I2C scan, purpose TBD)

All sensors connect via Qwiic (I2C) for tool-free assembly.

**Critical Hardware Setup**:
- SD Card Level Shifter: GPIO32 MUST be set HIGH before calling SD_MMC.begin()
- I2C Bus: SDA on GPIO21, SCL on GPIO22 (default Wire pins)
- SD Card: Use 1-bit mode with SD_MMC library (not SPI)

## Implementation Status

**Current Phase**: M3L-58 Complete (Button Interrupt Handler)
**Last Updated**: 2025-11-12
**Firmware Build**: 7.0% RAM (22,780 bytes), 29.5% Flash (386,685 bytes), Stable heap at 302,732 bytes free

**Completed (NOW Phase)**:
- [x] M3L-57: State machine with 4 states (IDLE, AWAITING_QR, RECORDING, ERROR)
- [x] M3L-58: Button interrupt handler with hardware debouncing
- [x] Hardware initialization (SD card with GPIO32 level shifter, I2C, button LED)
- [x] Non-blocking LED patterns for state indication
- [x] Serial logging with timestamps and heap monitoring
- [x] Hardware verification (5 I2C devices detected and operational)

**In Progress**:
- [ ] M3L-60: QR code scanner integration (NEXT)
- [ ] M3L-61: IMU sensor data sampling
- [ ] M3L-62: SD card data recording

## Project Structure

```
m3_data_logger/
├── init_prompt.md          # Original project requirements
├── PRD.md                  # Product requirements document
├── CLAUDE.md               # This file
├── plans/                  # Linear task PRDs and design briefs
│   ├── prd-m3l-57-state-machine.md
│   └── design-m3l-57-state-machine.md
├── src/                    # Source code
│   ├── main.cpp           # Main application logic with state machine (IMPLEMENTED)
│   └── hardware_init.cpp  # Hardware initialization (IMPLEMENTED)
├── lib/                    # External libraries
│   └── SparkFun_Qwiic_Button_Arduino_Library/  # Button LED control
├── include/                # Header files
│   └── hardware_init.h    # Hardware setup declarations
├── test/                   # Unit tests
│   └── test_build_validation/  # Automated build checks
├── tools/                  # Development and testing tools
│   └── qr_generator/      # QR code generation scripts (TO BE IMPLEMENTED)
│       ├── generate_qr.py # CLI tool for metadata QR codes
│       └── README.md      # Usage instructions
├── data/                   # Sample datasets
│   └── test_qr_codes/     # Pre-generated test QR codes (TO BE CREATED)
├── docs/                   # Documentation
│   └── BACKUP_STOCK_FIRMWARE.md  # Stock firmware backup instructions
├── platformio.ini          # PlatformIO configuration
└── partitions_ota.csv     # ESP32 partition table (OTA support)
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

# Upload to device (use 115200 baud for reliability on ESP32)
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

**Expected Boot Sequence**:
```
ESP-ROM:esp32-20200527
...
M3 Data Logger Initializing...
[0.256s] INFO: SD Card Level Shifter enabled on GPIO32
[0.512s] INFO: SD Card mounted successfully | Type: SDHC | Size: 30436MB
[0.768s] INFO: I2C initialized | SDA: GPIO21, SCL: GPIO22
[1.024s] INFO: Qwiic button initialized
[1.280s] INFO: I2C Device Scan found 5 devices at addresses: 0x0C, 0x36, 0x6B, 0x6F, 0x7E
[1.536s] INFO: System initialized | State: IDLE | Free Heap: 302732 bytes
```

**Upload Troubleshooting**:
If upload fails with "Invalid head of packet" error:
1. Hold BOOT button on device, click Upload, release BOOT when upload starts
2. Or reduce upload_speed in platformio.ini: `upload_speed = 115200` (instead of 921600)
3. Or disconnect and reconnect USB cable, then retry

**Verify Hardware Operation**:
1. Connect via serial monitor at 115200 baud
2. Check for successful I2C scan (should find 5 devices)
3. Verify SD card mount (should show card size in MB)
4. Check free heap stability (should stay around 302KB)

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
        ┌─────────────────┐
        │      IDLE       │ ←─────┐
        └────────┬────────┘        │
                 │ Button Press    │ Session End
                 ▼                 │
        ┌─────────────────┐        │
        │  AWAITING_QR    │        │
        │  (30s timeout)  │        │
        └────┬───────┬────┘        │
             │       │ Timeout     │
    QR Scan  │       └──────┐      │
             ▼              ▼      │
        ┌─────────────┐  ┌────────┴─────┐
        │  RECORDING  │  │     ERROR    │
        │             │  │ (60s recover)│
        └─────────────┘  └──────────────┘
```

**State Descriptions**:
- **IDLE**: System ready, waiting for button press. LED OFF.
- **AWAITING_QR**: Waiting for QR code scan (30s timeout). LED SLOW BLINK (1Hz).
- **RECORDING**: Active sensor data recording. LED SOLID ON.
- **ERROR**: Error condition with automatic recovery. LED FAST BLINK (5Hz).

**Implemented in**: `src/main.cpp` (lines 30-35, 48-299)

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

### Build Validation Tests (Automated)
Located in `test/test_build_validation/test_main.cpp`, runs 8 automated checks:
1. Verify firmware builds without errors
2. Check RAM usage under 80% (currently 7.0%)
3. Check Flash usage under 80% (currently 29.5%)
4. Validate all required libraries present
5. Verify no compilation warnings
6. Check platformio.ini configuration
7. Validate serial baud rate (115200)
8. Check for memory leaks (static analysis)

Run with: `pio test`

### Hardware Integration Tests (Manual)
**Prerequisites**:
- Device connected via USB
- MicroSD card formatted as FAT32 with MBR (not GUID)
- All Qwiic sensors connected
- Serial monitor at 115200 baud

**M3L-57 State Machine Tests** (COMPLETED):
- [x] System boots and enters IDLE state
- [x] LED OFF in IDLE state
- [x] Serial logging shows timestamps and heap usage
- [x] I2C scan detects 5 devices (0x0C, 0x36, 0x6B, 0x6F, 0x7E)
- [x] SD card mounts successfully (shows size in MB)
- [x] Heap remains stable (302KB free, no leaks)
- [x] State machine transitions logged correctly
- [x] Non-blocking LED patterns work (no loop blocking)

**M3L-58 Button Tests** (COMPLETED):
- [x] Button press triggers state change IDLE -> AWAITING_QR
- [x] LED changes to slow blink (1Hz) in AWAITING_QR
- [x] 30-second timeout transitions to ERROR state
- [x] Button press during AWAITING_QR cancels and returns to IDLE
- [x] Debouncing prevents multiple rapid presses
- [x] Button LED blinks 100ms on every valid press
- [x] ISR execution time <10μs (flag-only pattern)
- [x] No false triggers in 100+ rapid press test
- [x] State-aware handling in all 4 states (IDLE, AWAITING_QR, RECORDING, ERROR)
- [x] No memory leaks after 1000+ button presses

**M3L-60 QR Scanner Tests** (PENDING):
- [ ] QR code scan starts recording
- [ ] Invalid QR code shows error and returns to AWAITING_QR
- [ ] Metadata extracted correctly from JSON

**M3L-61 IMU Tests** (PENDING):
- [ ] Sensor data appears in serial output
- [ ] Sampling rate is 100Hz
- [ ] No data loss during continuous recording

**M3L-62 SD Card Tests** (PENDING):
- [ ] Data written to microSD in correct CSV format
- [ ] Second button press stops recording cleanly
- [ ] File closed properly (no corruption)
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

**CRITICAL: SD Card Formatting Requirements**
The ESP32 SD_MMC library is very particular about card formatting:

1. **Must use FAT32 with MBR partition table** (not GUID/GPT, not exFAT)
2. **Mac Disk Utility setup**:
   - Select the SD card device (not volume)
   - Click "Erase"
   - Format: "MS-DOS (FAT)" (this is FAT32)
   - Scheme: "Master Boot Record"
   - Name: Any name (e.g., "M3_DATA")
   - Click "Erase"

3. **Common failure symptoms**:
   - Error: "Card Mount Failed" even though card is present
   - Error: "getPartition failed" - indicates GUID partition table
   - Card works in computer but not in ESP32

4. **Other troubleshooting**:
   - Use high-quality card (Class 10 or better)
   - Check card lock switch is in unlocked position
   - Verify card size is supported (32GB max for FAT32)
   - Ensure GPIO32 is HIGH before SD_MMC.begin() call

**Verified Working Configuration**:
- SanDisk 32GB SDHC card
- Formatted as MS-DOS (FAT) with Master Boot Record
- Mounts as Type: SDHC, Size: 30436MB

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

### NOW (Current Phase - Core Functionality)
**Completed**:
- [x] M3L-57: State machine architecture (IDLE, AWAITING_QR, RECORDING, ERROR)
- [x] M3L-58: Button interrupt handling with hardware debouncing
- [x] Hardware initialization (SD card, I2C, LED)
- [x] Non-blocking LED patterns for state indication
- [x] Serial logging with timestamps and heap monitoring
- [x] Build validation test suite

**In Progress**:
- [ ] M3L-60: QR code scanning and metadata parsing (NEXT UP)

**Remaining NOW Phase**:
- [ ] M3L-61: IMU data sampling at 100Hz
- [ ] M3L-62: MicroSD storage in CSV format
- [ ] Python CLI tool for generating metadata QR codes

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

### Hardware Initialization Order (CRITICAL)
**SD Card Level Shifter**: GPIO32 MUST be set HIGH before calling SD_MMC.begin()
```cpp
// CORRECT order (from hardware_init.cpp)
pinMode(SD_CARD_LEVEL_SHIFTER_PIN, OUTPUT);
digitalWrite(SD_CARD_LEVEL_SHIFTER_PIN, HIGH);  // Enable level shifter FIRST
delay(100);  // Allow level shifter to stabilize
if (!SD_MMC.begin("/sdcard", true)) {  // THEN initialize SD card
    Serial.println("SD Card mount failed");
}
```

If you call SD_MMC.begin() before setting GPIO32 HIGH, the SD card will not mount even if it's properly formatted.

### SD Card Formatting (CRITICAL)
**Must use FAT32 with MBR partition table**. Common mistake: Mac Disk Utility defaults to GUID partition table which ESP32 cannot read.

Error message: "getPartition failed" = wrong partition table (use MBR, not GUID)

See "SD Card Write Failures" section for detailed formatting instructions.

### State Machine LED Patterns
**LED initialization gotcha**: When entering a new state with blinking LED, you MUST reset timing variables:
```cpp
void transitionState(SystemState newState, const char* reason) {
    // ... state transition code ...

    // Reset LED timing to prevent flicker on state entry
    lastLEDToggle = millis();
    ledState = false;
    digitalWrite(LED_PIN, LOW);
}
```

Without this reset, the LED may start with incorrect timing or state.

### ESP32 ISR Constraints (CRITICAL)
The SparkFun DataLogger IoT runs on ESP32. Interrupt Service Routines have strict requirements:

**IRAM_ATTR Requirement**:
- ALL ISR functions MUST use `IRAM_ATTR` attribute
- Places code in IRAM (not flash) to prevent crashes during flash writes
- Syntax: `void IRAM_ATTR onButtonPressed() { ... }`
- Without it: ISR crashes when flash cache is disabled

**Volatile Keyword Requirement**:
- All ISR-modified variables MUST be declared `volatile`
- Prevents compiler optimization from breaking ISR-to-main-loop communication
- Example: `volatile bool buttonPressed = false;`

**ISR Performance Constraints**:
- ISR must complete in <10μs (no I2C, Serial, or LED operations)
- Pattern: ISR sets flag only, main loop processes flag
- NEVER perform I2C in ISR (takes 100-500μs, blocks other interrupts)
- Keep ISR minimal: single flag assignment only

**Correct ISR Pattern** (from M3L-58):
```cpp
volatile bool buttonPressed = false;

void IRAM_ATTR onButtonPressed() {
    buttonPressed = true;  // Flag only, no I2C/Serial/LED
}

void loop() {
    if (buttonPressed) {
        // Process in main loop, can use I2C here
        if (button.hasBeenClicked()) {
            buttonPressed = false;  // Clear after verification
            // ... handle button press ...
        }
    }
}
```

### Timing Issues
The SparkFun DataLogger IoT runs on ESP32, which has two cores. Be careful with:
- Blocking delays - use `millis()` for non-blocking timing (see LED pattern implementation)
- millis() rollover - use `uint32_t timeInState = millis() - stateEntryTime` which handles rollover correctly
- Software debouncing - use 50ms window check: `(millis() - lastTime) < DEBOUNCE_MS`

**Non-blocking LED pattern example** (from src/main.cpp):
```cpp
void updateLEDPattern() {
    uint32_t currentMillis = millis();

    if (currentState == AWAITING_QR) {
        // Slow blink at 1Hz (500ms on, 500ms off)
        if (currentMillis - lastLEDToggle >= 500) {
            ledState = !ledState;
            digitalWrite(LED_PIN, ledState ? HIGH : LOW);
            lastLEDToggle = currentMillis;
        }
    }
    // ... other states ...
}
```

### Memory Constraints
- ESP32 has limited heap - avoid large dynamic allocations
- Use circular buffers for sensor data to prevent fragmentation
- Monitor free heap with `ESP.getFreeHeap()` during development
- Current baseline: 302,732 bytes free after initialization
- Watch for heap fragmentation (multiple small allocs/frees)

### SD Card Performance
- Batch writes to reduce latency (buffer 10-20 samples)
- Close file periodically to prevent data loss on power failure
- Don't write during button interrupt - set flag and write in main loop
- Use 1-bit SD_MMC mode (more reliable than 4-bit mode on DataLogger IoT)

### JSON Parsing
- ArduinoJson requires proper memory allocation for JsonDocument
- Calculate required size with ArduinoJson Assistant tool
- Always check `deserializeJson()` return value for errors

### Upload Issues
ESP32 upload can be finicky. If you get "Invalid head of packet" error:
1. Reduce upload_speed to 115200 in platformio.ini
2. Hold BOOT button during upload start
3. Use high-quality USB cable (data-capable, not charge-only)

## Recent Learnings (Auto-Updated)

### 2025-11-12: M3L-57 State Machine Implementation Complete
**Discovered by**: senior-software-engineer
**Context**: Implemented and deployed core state machine architecture with hardware initialization

**Critical Hardware Discoveries**:

1. **SD Card Level Shifter Initialization** (HARDWARE BUG FOUND)
   - GPIO32 MUST be set HIGH before SD_MMC.begin()
   - Failure to do this prevents SD card mount even with correct formatting
   - Required 100ms delay after enabling shifter for stabilization
   - Code location: `src/hardware_init.cpp:init_sd_card()`

2. **SD Card Formatting Requirements** (DOCUMENTATION GAP)
   - ESP32 SD_MMC ONLY works with FAT32 + MBR partition table
   - exFAT not supported, GUID/GPT partition table not supported
   - Mac Disk Utility defaults to GUID - must explicitly select "Master Boot Record"
   - Error "getPartition failed" = wrong partition table type
   - Working config: "MS-DOS (FAT)" format + "Master Boot Record" scheme

3. **I2C Device Map Discovered**
   - 0x0C: Tiny Code Reader (QR scanner)
   - 0x36: MAX17048 (LiPo fuel gauge, onboard)
   - 0x6B: ISM330DHCX (IMU)
   - 0x6F: Qwiic Button
   - 0x7E: Unknown device (investigate in future)

**State Machine Implementation Patterns**:

1. **Non-blocking LED Control**
   ```cpp
   // Must reset timing on state entry to prevent flicker
   lastLEDToggle = millis();
   ledState = false;

   // Check timing in updateLEDPattern() called every loop
   if (currentMillis - lastLEDToggle >= blinkInterval) {
       ledState = !ledState;
       digitalWrite(LED_PIN, ledState);
       lastLEDToggle = currentMillis;
   }
   ```

2. **Timeout Handling**
   - Use `uint32_t timeInState = millis() - stateEntryTime`
   - This handles millis() rollover correctly (every 49 days)
   - Check timeout in state handler, not in timer interrupt

3. **State Transition Logging**
   - Format: `[uptime_ms] STATE_CHANGE: OLD → NEW (reason) | Free Heap: X bytes`
   - Always log reason for transition (debugging aid)
   - Include heap usage to detect memory leaks

**Memory Baseline Established**:
- Initial free heap: 302,732 bytes
- After 1000+ loop iterations: Still 302,732 bytes (no leaks detected)
- RAM usage: 7.0% (22,780 bytes)
- Flash usage: 29.5% (386,685 bytes)

**Upload Troubleshooting**:
- ESP32 upload can fail with "Invalid head of packet" error
- Solution: Reduce upload_speed from 921600 to 115200 in platformio.ini
- Or hold BOOT button during upload start
- Not a code issue - hardware timing sensitivity

**Testing Approach**:
1. Build validation (8 automated checks) - PASSED
2. Hardware initialization test - PASSED (5 I2C devices detected)
3. State machine operation - VERIFIED (IDLE state, LED OFF, stable heap)
4. Serial logging - WORKING (timestamps, heap monitoring)

**Integration Points for Next Tasks**:
- M3L-58 (Button): Will call `transitionState(AWAITING_QR, "button pressed")`
- M3L-60 (QR): Will call `transitionState(RECORDING, "QR code scanned")`
- M3L-61 (IMU): Start/stop sampling in RECORDING state entry/exit
- M3L-62 (SD): File operations in RECORDING state entry/exit

**Files Modified**:
- `src/main.cpp`: State machine logic (lines 30-344)
- `src/hardware_init.cpp`: SD card, I2C, button init
- `include/hardware_init.h`: Function declarations
- `platformio.ini`: Upload speed, partition table
- `test/test_build_validation/test_main.cpp`: Automated checks

---

### 2025-11-12: M3L-58 Button Interrupt Handler Implementation Complete
**Discovered by**: senior-software-engineer (with code-reviewer feedback)
**Context**: Implemented button interrupt handler with hardware debouncing and state-aware transitions

**Critical ESP32 ISR Discoveries**:

1. **IRAM_ATTR Requirement** (ESP32-SPECIFIC)
   - ISR functions MUST use `IRAM_ATTR` attribute to place code in IRAM
   - Without it: ISR crashes when flash cache is disabled during write operations
   - Syntax: `void IRAM_ATTR onButtonPressed() { ... }`
   - Code location: `src/main.cpp:52-55`

2. **ISR Performance Constraints** (CRITICAL)
   - ISR must complete in <10μs (measured: ~5μs for flag-only ISR)
   - NEVER perform I2C, Serial, or LED operations in ISR
   - Pattern: ISR sets volatile flag only, main loop processes flag
   - Rationale: I2C operations take 100-500μs, block other interrupts

3. **Volatile Keyword Required** (COMPILER CONSTRAINT)
   - All ISR-modified variables MUST be declared `volatile`
   - Without volatile: compiler may optimize away flag checks in main loop
   - Applied to: `volatile bool buttonPressed`, `volatile uint32_t lastButtonPressTime`
   - Code location: `src/main.cpp:42-43`

**Software Debouncing Implementation**:

1. **Dual Debouncing Strategy** (HARDWARE + SOFTWARE)
   - Hardware debounce: Qwiic Button's 50ms internal debounce (I2C register level)
   - Software debounce: Additional 50ms time window check (mechanical bounce level)
   - Why both: ISR can fire multiple times during mechanical bounce before I2C register updates

2. **Timing Implementation**
   ```cpp
   // Check debounce BEFORE processing button press
   if (currentTime - lastButtonPressTime < BUTTON_DEBOUNCE_MS) {
       buttonPressed = false;  // Reject bounce, clear flag
       return;  // Don't process this event
   }
   ```

3. **Debounce Tuning**
   - 50ms threshold: Industry standard for tactile buttons
   - Too low (<20ms): False triggers from mechanical bounce
   - Too high (>100ms): User perceives lag, may double-press

**Button Interrupt Setup Pattern**:

1. **GPIO Selection Constraints**
   - GPIO 33 chosen: interrupt-capable, no boot conflicts
   - Avoid GPIO 0, 2, 5, 12, 15 (boot/strapping pins)
   - Avoid GPIO 21, 22 (I2C bus)
   - Avoid GPIO 32 (SD card level shifter control)

2. **Initialization Order** (CRITICAL)
   ```cpp
   // 1. Enable interrupt on Qwiic Button I2C register
   button.enablePressedInterrupt();

   // 2. Configure ESP32 GPIO pin for interrupt
   pinMode(BUTTON_INTERRUPT_PIN, INPUT_PULLUP);

   // 3. Attach ISR with FALLING edge trigger
   attachInterrupt(digitalPinToInterrupt(BUTTON_INTERRUPT_PIN),
                   onButtonPressed, FALLING);

   // 4. Clear any pending events
   button.clearEventBits();
   ```

3. **Interrupt Pin Behavior**
   - Button's interrupt pin goes LOW on press (active-low)
   - Stays LOW until `button.clearEventBits()` called via I2C
   - FALLING edge trigger: ISR fires on HIGH-to-LOW transition

**Flag Clearing Race Condition** (BUG FOUND AND FIXED):

Initial implementation had incorrect flag clearing order:
```cpp
// WRONG: Clear flag before I2C verification
buttonPressed = false;  // Race: new interrupt could set flag here
if (button.hasBeenClicked()) {  // I2C check
    // Process button...
}
```

**Correct pattern** (prevents race condition):
```cpp
// RIGHT: Clear flag AFTER I2C verification
if (button.hasBeenClicked()) {  // I2C check first
    buttonPressed = false;  // Then clear flag (no race window)
    lastButtonPressTime = currentTime;
    // Process button...
    button.clearEventBits();  // Reset hardware interrupt pin
}
```

**State-Aware Button Handling**:

Each state has different button press behavior:
- **IDLE**: Button press → AWAITING_QR (start session)
- **AWAITING_QR**: Button press → IDLE (cancel QR scan)
- **RECORDING**: Button press → IDLE (stop recording, close file)
- **ERROR**: Button press → IDLE (manual error recovery)

**Visual Feedback Pattern**:
```cpp
// 100ms LED blink for immediate user confirmation
button.LEDon(255);  // Full brightness
delay(100);  // Blocking acceptable for UX feedback
button.LEDoff();
```

**Code Review Lessons** (4 CRITICAL ISSUES FOUND):

1. **Missing AWAITING_QR Cancel Logic**
   - Initial code only handled IDLE → AWAITING_QR
   - Forgot AWAITING_QR → IDLE (cancel scan) transition
   - Fixed by adding button handler in AWAITING_QR state

2. **No Software Debounce Timing**
   - First implementation only set `lastButtonPressTime`, never checked it
   - Added debounce window check: `currentTime - lastButtonPressTime < 50ms`

3. **Flag Clearing Race Condition**
   - Cleared `buttonPressed` before I2C verification
   - New interrupt could set flag between clear and check
   - Fixed by clearing flag inside `if (button.hasBeenClicked())` block

4. **Test Build Linking Error**
   - Test tried to access `volatile bool buttonPressed` directly
   - Linker can't resolve ISR volatile flags from test environment
   - Solution: Test ISR via hardware integration, not unit tests

**Testing Strategy**:

1. **Build Validation Tests** (10 Automated Checks)
   - Verify ISR has IRAM_ATTR attribute
   - Check volatile keywords present
   - Verify GPIO pin definitions valid
   - Confirm button library linked
   - Test firmware builds without errors
   - Code location: `test/test_build_validation/test_button.cpp`

2. **Hardware Integration Tests** (Manual, 10 Test Cases)
   - Single press: IDLE → AWAITING_QR transition
   - Double press: IDLE → AWAITING_QR → IDLE cancel
   - Rapid presses: Debouncing prevents false triggers
   - LED feedback: Button LED blinks on every valid press
   - Timeout: AWAITING_QR → ERROR after 30 seconds
   - Test procedure: `test/hardware_tests/test_m3l58_button.md`

3. **Test Build Limitation Discovered**
   - Cannot unit test ISR volatile flags due to linker constraints
   - PlatformIO test environment can't resolve ISR symbols from main.cpp
   - Workaround: Verify ISR behavior via hardware integration tests only

**Hardware Pin Assignments** (Documented):
| Signal | GPIO | Purpose | Notes |
|--------|------|---------|-------|
| Button Interrupt | GPIO 33 | Button press detection | FALLING edge trigger |
| Button I2C | GPIO 21/22 | LED control, event readback | Shared Qwiic bus |
| SD Card Control | GPIO 32 | Level shifter enable | Must be HIGH for SD |

**Memory Impact**:
- Added 4 bytes: `volatile bool buttonPressed` + `volatile uint32_t lastButtonPressTime`
- Added ~200 bytes Flash: ISR function + button handling logic
- No heap allocations (all stack or static)
- No memory leaks detected after 1000+ button presses

**Integration Points for Next Tasks**:
- M3L-60 (QR Scanner): Activated when entering AWAITING_QR state
- M3L-61 (IMU): Start sampling on RECORDING entry (triggered by QR scan)
- M3L-62 (SD Card): Create file on RECORDING entry, close on button press exit

**Files Modified**:
- `include/hardware_init.h`: Button pin definitions, init function declaration
- `src/hardware_init.cpp`: `init_button_interrupt()` implementation
- `src/main.cpp`: ISR, debounce logic, state-aware button handlers (lines 42-43, 52-55, 145-246)
- `test/test_build_validation/test_button.cpp`: 10 automated build checks
- `test/hardware_tests/test_m3l58_button.md`: Manual test procedure
