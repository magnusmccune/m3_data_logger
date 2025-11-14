# CLAUDE.md - M3 Data Logger

IoT time-series data logger with QR metadata tagging for ML datasets. ESP32-based with IMU sensor, GPS time sync, and SD storage.

## Hardware (I2C Qwiic)

| Component | I2C Addr | Purpose |
|-----------|----------|---------|
| SparkFun DataLogger IoT (DEV-22462) | - | ESP32-WROVER controller |
| ISM330DHCX IMU (SEN-19764) | 0x6B | 6DoF accel+gyro @ 100Hz |
| SAM-M8Q GPS (GPS-15210) | 0x42 | Time sync + location |
| Qwiic Button (BOB-15932) | 0x6F | Recording trigger |
| Tiny Code Reader (SEN-23352) | 0x0C | QR metadata scanner |
| MAX17048 Fuel Gauge | 0x36 | LiPo monitoring (onboard) |

## Project Status

**Phase**: NOW - Core Data Logging & GPS Time Sync
**Last Updated**: 2025-11-14
**Build**: 7.2% RAM, 30.5% Flash, 301KB free heap

**Active Work**:
- Core logging (M3L-61/63/64): IMU debugging - zero values in CSV despite valid init
- GPS time sync (M3L-77): time_manager module, RGB LED status, CSV timestamps, location logging

**Blocking Issues**: IMU zero values (see plans/debugging-imu-zeros.md)

## Quick Start

```bash
pio run --target upload && pio device monitor --baud 115200
pio test  # Build validation
```

**Upload issues**: Hold BOOT button if "Invalid head of packet" error occurs.

## Architecture

### State Machine
```
IDLE → AWAITING_QR (30s) → RECORDING → IDLE
         ↓ timeout           ↑ button
       ERROR ←───────────────┘
```

### RGB LED Status (GPIO26 NeoPixel)
Dual-channel indication: **Color** = GPS status, **Pattern** = state machine

| Color | GPS Status | Pattern | State | Example |
|-------|-----------|---------|-------|---------|
| Green | Locked | Breathing | IDLE | Green breathing |
| Yellow | Acquiring | Slow blink | AWAITING_QR | Yellow slow blink |
| Blue | No GPS/millis | Solid | RECORDING | Blue solid |
| Red | Error | Fast blink | ERROR | Red fast blink (overrides all) |

### QR Metadata Format
```json
{"test_id": "A3F9K2M7", "description": "walking_outdoor", "labels": ["walking", "outdoor"]}
```
Generate with: `tools/qr_generator/generate_qr.py --description "name" --labels "tag1,tag2"`

### Data Storage

**CSV** (`/data/session_YYYYMMDD_HHMMSS.csv`):
```csv
test_id,timestamp_ms,lat,lon,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z
A3F9K2M7,1731604252123,37.774900,-122.419400,0.012,-0.003,0.981,0.001,-0.002,0.000
```
- Timestamps: Unix epoch ms (GPS locked) or millis() fallback
- Location: Decimal degrees (6 decimals = ~0.1m accuracy)
- Units: Accel in g, gyro in deg/s

**Metadata** (`/data/metadata.json`):
```json
{
  "sessions": [{
    "session_id": "session_20251114_143052",
    "test_id": "A3F9K2M7",
    "description": "walking_outdoor",
    "labels": ["walking", "outdoor"],
    "start_time": "2025-11-14T14:30:52Z",
    "sample_count": 1500,
    "time_source": "gps",
    "gps_locked": true
  }]
}
```

## Critical Gotchas

### 1. SD Card Formatting
**ESP32 SD_MMC requires FAT32 with MBR partition table** (not GUID/GPT, not exFAT).

Mac Disk Utility: Format "MS-DOS (FAT)", Scheme "Master Boot Record"

### 2. Hardware Initialization Order
```cpp
// GPIO32 HIGH before SD_MMC.begin() - enables level shifter
pinMode(SD_CARD_LEVEL_SHIFTER_PIN, OUTPUT);
digitalWrite(SD_CARD_LEVEL_SHIFTER_PIN, HIGH);
delay(100);
SD_MMC.begin("/sdcard", true);
```

**GPS Cold Start**: SAM-M8Q takes 30+ seconds for initial satellite lock. Indoor testing will use millis() fallback.

### 3. ESP32 ISR Constraints
```cpp
volatile bool flag = false;
void IRAM_ATTR myISR() { flag = true; }  // Flag only, NO I2C/Serial (<10μs)
void loop() { if (flag) { /* I2C safe here */ flag = false; } }
```
**Why**: ISR crashes without IRAM_ATTR. I2C takes 100-500μs, blocks interrupts.

### 4. Non-Blocking Timing
```cpp
uint32_t elapsed = millis() - startTime;  // Rollover-safe
if (elapsed >= TIMEOUT_MS) { /* timeout */ }
```

### 5. IMU Timing
**DO NOT use checkStatus() in tight loops** - causes infinite hangs.
```cpp
uint32_t start = millis();
while (!imu.checkStatus()) {
    if (millis() - start > 100) break;  // Timeout required
    delay(1);
}
```

### 6. Path Sanitization
**NEVER sanitize full paths** - sanitizeFilename() converts "/" to "_".
```cpp
String filename = "session.csv";
sanitizeFilename(filename);  // OK
String path = "/data/" + filename;  // Safe: /data/session.csv
```

### 7. QR Reader LED Bug
**DO NOT write to register 0x01** - breaks I2C (error 263). Accept 5mW power cost.

### 8. Button Debouncing
```cpp
if (currentTime - lastPress < 50) { buttonPressed = false; return; }
```

## Common Tasks

### Generate QR Test Codes
```bash
cd tools/qr_generator
python3 -m venv venv && source venv/bin/activate  # First time only
pip install -r requirements.txt  # First time only
python generate_qr.py --description "walking_outdoor" --labels "walking,outdoor"
```
QR codes saved to `tools/qr_generator/qr_codes/` with test_id in filename.

## Active Debugging

### M3L-61/63/64: IMU Zero Values
**Status**: Implementation complete, debugging CSV zero values despite valid init readings.

**Current Issue**: IMU returns zeros in CSV files while init shows correct values.
**Root Cause**: Investigating data flow from circular buffer to storage.
**Next**: Debug logging trace from readSensor() → buffer → writeSample().

**Details**: See `plans/debugging-imu-zeros.md`

**Architecture**:
- sensor_manager: IMU driver with 10-sample circular buffer
- storage_manager: CSV writer with metadata.json session tracking
- QR format: Changed to test_id (8-char unique) + description + labels

## File Structure

**Core**: src/{main, hardware_init, sensor_manager, storage_manager, time_manager}.cpp
**Headers**: include/{hardware_init, sensor_manager, storage_manager, time_manager}.h
**Config**: platformio.ini
**Tools**: tools/qr_generator/generate_qr.py (Python venv)
**Docs**: plans/{prd-data-logging-core, debugging-imu-zeros, manual-testing}.md
**Archive**: plans/archive/completed/ (M3L-57/58/60 PRDs)

## Testing

**Build**: `pio test` (8 checks: build, RAM/Flash <80%, warnings, leaks)
**Hardware**: Serial monitor at 115200 baud - verify 6 I2C devices (0x0C, 0x36, 0x42, 0x6B, 0x6F, 0x7E), SD mount, 302KB heap, GPS lock.

## Coding Conventions

- **Style**: snake_case (functions/vars), PascalCase (classes), 4 spaces, 100 char lines
- **Error Handling**: Check returns, log to Serial, graceful degradation
- **Comments**: Doxygen for functions, inline for complex logic

## Phasing

**NOW**: Core logging (IMU, SD, sessions), GPS time sync (time_manager, RGB LED, timestamps, location)
**NEXT**: NTP sync (WiFi QR config), MQTT transmission, RFID sensors, battery optimization
**LATER**: Web dashboard, cloud storage, ML deployment
