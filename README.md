# M3 Data Logger

IoT time-series data logger for SparkFun hardware. Records IMU sensor data at 100Hz with QR code metadata tagging for ML datasets.

## Hardware Components

| Component | Part Number | I2C Addr | Purpose |
|-----------|-------------|----------|---------|
| SparkFun DataLogger IoT | DEV-22462 | - | Main controller (ESP32, microSD, Qwiic) |
| ISM330DHCX IMU Breakout | SEN-19764 | 0x6B | 6DoF inertial measurement (100Hz) |
| SAM-M8Q GPS Module | GPS-15210 | 0x42 | Time sync and location |
| Qwiic Button - Red LED | BOB-15932 | 0x6F | Recording trigger |
| Tiny Code Reader | SEN-23352 | 0x0C | QR code metadata scanner |
| MAX17048 Fuel Gauge | (onboard) | 0x36 | LiPo battery monitoring |

**All sensors connect via Qwiic (I2C)** - no soldering required!

## Quick Start

### Prerequisites

1. **Install PlatformIO**
   - VSCode: Install the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode)
   - CLI: `pip install platformio` or `brew install platformio`

2. **Hardware Setup**
   - Connect all Qwiic sensors to the DataLogger IoT board using Qwiic cables
   - Insert a microSD card (formatted as FAT32 with MBR partition table - see CLAUDE.md)
   - Connect USB-C cable to computer

### Build and Upload

```bash
# Build the project
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

Expected serial output shows:
- 6 I2C devices detected: 0x0C, 0x36, 0x42, 0x6B, 0x6F, 0x7E
- SD card mounted with size
- Free heap ~302KB
- IMU initialization with test readings
- GPS status (acquiring/locked)

**Troubleshooting**: If "Invalid head of packet" error, hold BOOT button during upload or reduce upload_speed to 115200 in platformio.ini.

## Project Structure

```
m3_data_logger/
├── platformio.ini       # PlatformIO configuration
├── src/
│   └── main.cpp        # Main application
├── include/            # Header files
├── lib/                # Custom libraries
├── test/               # Unit tests
├── tools/              # Development tools
│   └── qr_generator/  # QR code generation scripts
├── data/               # Sample datasets
│   └── test_qr_codes/ # Pre-generated test QR codes
└── README.md           # This file
```

## Workflow Overview

1. **Press Button** → RGB LED blinks (awaiting QR scan, 30s timeout)
2. **Scan QR Code** → Captures metadata (test_id, description, labels)
3. **Recording Starts** → LED solid, IMU data logged to microSD at 100Hz with GPS timestamps
4. **Press Button** → Recording stops, LED returns to breathing pattern

**RGB LED Status** (dual-channel indication):
- Color: Green=GPS locked, Yellow=GPS acquiring, Blue=millis fallback, Red=error
- Pattern: Breathing=IDLE, Blinking=AWAITING_QR, Solid=RECORDING

**Power Management** (M3L-83):
- IDLE: Device enters deep sleep after 5 seconds to save battery
- Wake: Press button to wake and start QR scan (instant response)
- Battery Life: ~21+ days on 2000mAh LiPo (95% idle, 5% recording)
- Battery Status: Logged every 30 seconds, included in session metadata

**Data Formats**:
- CSV: `/data/session_YYYYMMDD_HHMMSS.csv` with GPS timestamps (Unix epoch ms), lat/lon, accel_xyz, gyro_xyz
- Metadata: `/data/metadata.json` with session info, test_id mapping, time source (GPS or millis), battery data

## QR Code Generation

Generate test QR codes using the CLI tool or REST API:

**CLI Tool**:
```bash
cd tools/qr_generator
python3 -m venv venv && source venv/bin/activate
pip install -r requirements.txt
python generate_qr.py --description "walking_outdoor" --labels "walking,outdoor"
```

**REST API** (Docker):
```bash
cd tools/qr_generator
docker-compose up
# In another terminal:
curl -X POST http://localhost:8000/generate \
  -H "Content-Type: application/json" \
  -d '{"description": "walking_outdoor", "labels": ["walking", "outdoor"]}' \
  --output qr_code.png
```

See `tools/qr_generator/README.md` for full API documentation and Postman guide.

## Documentation

- **[CLAUDE.md](CLAUDE.md)**: Complete developer documentation (hardware, architecture, gotchas, patterns)
- **[PRD.md](PRD.md)**: Product requirements and roadmap
- **[tools/qr_generator/README.md](tools/qr_generator/README.md)**: QR Generator API documentation
- **[docs/](docs/)**: Testing guides and procedures

## Development Status

**Current Phase**: NEXT - Enhanced Features

**Completed**:
- [x] Core data logging: State machine, button handler, QR scanner, IMU (100Hz), SD storage
- [x] Session management with metadata.json (M3L-64)
- [x] QR Generator CLI + REST API + Docker (M3L-66/67)
- [x] ESP32 I2C fix for Tiny Code Reader (M3L-66)
- [x] GPS time synchronization (M3L-77 Epic)
  - [x] time_manager module with GPS/millis fallback (M3L-78)
  - [x] GPS integration (SAM-M8Q at I2C 0x42) (M3L-79)
  - [x] Dual-channel RGB LED status indication (M3L-80)
  - [x] CSV timestamps with Unix epoch ms (M3L-81)
- [x] GPS location logging (lat/lon in CSV with 1Hz caching) (M3L-82)

**Next Up**:
- [ ] Battery optimization (deep sleep between recordings) (M3L-83)
- [ ] MQTT transmission (WiFi data upload)
- [ ] NTP time sync for indoor use

**Build Stats**: 8.0% RAM (26KB), 38.0% Flash (498KB), 302KB free heap

See Linear project: [M3-Data-Logger](https://linear.app/m3labs/project/m3-data-logger-d5a8ffada01d)

## License

This project is for personal/research use.

## Support

For issues or questions, see:
- Hardware datasheets: [SparkFun product pages](https://www.sparkfun.com)
- PlatformIO docs: https://docs.platformio.org
- Linear issues: https://linear.app/m3labs
