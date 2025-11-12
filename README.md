# M3 Data Logger

IoT-based time-series data collection system for generating sensor datasets with metadata framing.

## Hardware Components

| Component | Part Number | Purpose |
|-----------|-------------|---------|
| SparkFun DataLogger IoT | DEV-22462 | Main controller board (ESP32, microSD, Qwiic) |
| ISM330DHCX IMU Breakout | SEN-19764 | 6DoF inertial measurement (accel + gyro) |
| Qwiic Button - Red LED | BOB-15932 | Recording trigger and status indicator |
| Tiny Code Reader | SEN-23352 | QR code scanning for metadata capture |

**All sensors connect via Qwiic (I2C)** - no soldering required!

## Quick Start

### Prerequisites

1. **Install PlatformIO**
   - VSCode: Install the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode)
   - CLI: `pip install platformio` or `brew install platformio`

2. **Hardware Setup**
   - Connect all Qwiic sensors to the DataLogger IoT board using Qwiic cables
   - Insert a microSD card (formatted as FAT32)
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

Expected serial output:
```
==================================
  M3 Data Logger - Initializing
==================================

Board: SparkFun DataLogger IoT (DEV-22462)
MCU: ESP32
CPU Frequency: 240 MHz
Flash Size: 4 MB
Free Heap: 298516 bytes

✓ Serial initialized (115200 baud)
✓ Board initialization complete

Ready for sensor integration...
==================================
```

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

1. **Press Button** → LED blinks (awaiting QR scan)
2. **Scan QR Code** → Captures metadata (test name, labels)
3. **Recording Starts** → LED solid, IMU data logged to microSD
4. **Press Button** → Recording stops, LED off

See [CLAUDE.md](CLAUDE.md) for detailed developer documentation.
See [PRD.md](PRD.md) for product requirements and roadmap.

## Development Status

**Current Phase**: NOW (MVP)
- [x] Project initialization
- [ ] State machine implementation
- [ ] Sensor integration
- [ ] Data logging to microSD
- [ ] QR code scanning

See Linear project: [M3-Data-Logger](https://linear.app/m3labs/project/m3-data-logger-d5a8ffada01d)

## License

This project is for personal/research use.

## Support

For issues or questions, see:
- Hardware datasheets: [SparkFun product pages](https://www.sparkfun.com)
- PlatformIO docs: https://docs.platformio.org
- Linear issues: https://linear.app/m3labs
