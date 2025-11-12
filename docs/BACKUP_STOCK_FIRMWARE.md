# Stock Firmware Backup Procedure

⚠️ **CRITICAL: DO THIS BEFORE FIRST CUSTOM FIRMWARE FLASH**

Once you flash custom firmware to the SparkFun DataLogger IoT, you cannot return to stock firmware through normal means. The stock firmware's OTA update and self-recovery features will be permanently lost.

However, you can always restore from a backup using esptool.py.

## Why Backup?

**Stock Firmware Features You'll Lose**:
- Automatic sensor detection (50+ Qwiic sensors)
- Serial menu system for configuration
- OTA firmware updates from SparkFun
- Factory reset capability
- Web interface for log access
- IoT cloud integrations

**What You Keep**:
- ESP32 bootloader (ROM-based, cannot be erased)
- Ability to flash ANY firmware via USB serial
- All hardware functionality

## Prerequisites

```bash
# Install PlatformIO (if not already installed)
pip install platformio

# Install esptool.py via PlatformIO
pio pkg install -t tool-esptoolpy
```

## Backup Command

### macOS/Linux

```bash
# Find your device port (usually /dev/cu.usbserial-* on macOS)
ls /dev/cu.usb*

/dev/cu.usbserial-10

# Read entire 4MB flash
~/.platformio/packages/tool-esptoolpy/esptool.py \
    --port /dev/cu.usbserial-10 \
    --baud 921600 \
    read_flash 0x0 0x400000 \
    stock_firmware_backup_$(date +%Y%m%d).bin
```

### Windows

```powershell
# Find your COM port in Device Manager (e.g., COM3)

# Read entire 4MB flash
%USERPROFILE%\.platformio\packages\tool-esptoolpy\esptool.py ^
    --port COM3 ^
    --baud 921600 ^
    read_flash 0x0 0x400000 ^
    stock_firmware_backup_%DATE:~-4,4%%DATE:~-10,2%%DATE:~-7,2%.bin
```

## Verify Backup

```bash
# Check file size (should be exactly 4,194,304 bytes = 4MB)
ls -lh stock_firmware_backup_*.bin

# Optional: Read bootloader signature (should show "ESP32")
head -c 100 stock_firmware_backup_*.bin | hexdump -C
```

## Store Backup Safely

```bash
# Move to safe location
mkdir -p ~/backups/m3_data_logger
cp stock_firmware_backup_*.bin ~/backups/m3_data_logger/

# Optional: Create compressed archive
tar -czf stock_firmware_backup_$(date +%Y%m%d).tar.gz \
    stock_firmware_backup_*.bin
```

## Restore Stock Firmware

If you need to return to stock firmware:

### From Your Backup

```bash
# Flash your backup
~/.platformio/packages/tool-esptoolpy/esptool.py \
    --port /dev/cu.usbserial-XXXXXXXX \
    --baud 921600 \
    write_flash 0x0 \
    stock_firmware_backup_20231112.bin
```

### From SparkFun Release

Alternatively, download official firmware from SparkFun:

1. Visit: https://github.com/sparkfun/SparkFun_DataLogger/releases
2. Download latest `.bin` file
3. Flash with esptool.py:

```bash
~/.platformio/packages/tool-esptoolpy/esptool.py \
    --port /dev/cu.usbserial-XXXXXXXX \
    --baud 921600 \
    write_flash 0x0 \
    SparkFun_DataLoggerIoT_01_00_14.bin
```

## Troubleshooting

### "Serial port not found"

1. Check USB cable (must support data, not just power)
2. Install CP210x USB-to-Serial drivers from Silicon Labs
3. Check System Preferences → Security & Privacy for permissions

### "Timed out waiting for packet header"

1. Hold BOOT button on DataLogger IoT
2. Press RESET button briefly
3. Release RESET (keep BOOT held)
4. Run esptool command
5. Release BOOT when "Connecting..." appears

### "Failed to connect"

Try slower baud rate:
```bash
# Use 115200 instead of 921600
--baud 115200
```

## Recovery Without Backup

If you didn't backup and need stock firmware:

1. Download from SparkFun GitHub releases (see above)
2. Flash using esptool.py
3. Stock firmware will restore default settings

**Note**: Your old configuration/settings will be lost, but hardware will work.

## ESP32 Bootloader Protection

**Good news**: The ESP32 bootloader is in ROM and cannot be erased.

**This means**:
- You can ALWAYS enter serial flash mode (BOOT + RESET buttons)
- You can ALWAYS flash new firmware via USB
- It's nearly impossible to permanently brick the device

**Only way to brick** (extremely unlikely):
- Physical damage to ESP32 chip
- Incorrect voltage on critical pins (hardware issue)
- Damaged USB-to-Serial converter

## Pre-Flash Checklist

Before flashing custom firmware for the first time:

- [ ] Stock firmware backup created
- [ ] Backup file size verified (4,194,304 bytes)
- [ ] Backup stored in safe location
- [ ] Backup compressed/archived
- [ ] Tested entering bootloader mode (BOOT + RESET)
- [ ] esptool.py working with device
- [ ] USB cable verified (data transfer capable)
- [ ] Documented stock firmware version
- [ ] Tested stock firmware features you want to preserve

## Additional Resources

- **SparkFun DataLogger GitHub**: https://github.com/sparkfun/SparkFun_DataLogger
- **esptool.py Documentation**: https://docs.espressif.com/projects/esptool/
- **ESP32 Recovery Guide**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/

---

**Last Updated**: 2025-11-12
**Firmware Version**: M3 Data Logger v0.1.0
**Hardware**: SparkFun DataLogger IoT (DEV-22462)
