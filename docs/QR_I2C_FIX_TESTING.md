# QR Code I2C Chunking Fix - Testing Instructions

## Overview

This document provides testing instructions to verify the ESP32 I2C chunking fix for the Tiny Code Reader library has been successfully applied and is working correctly.

## Problem Summary

The original Tiny Code Reader library chunked 256-byte reads into 64-byte segments for Arduino Uno compatibility. On ESP32, each `Wire.requestFrom()` call starts a new I2C transaction that resets the device's read pointer, causing data corruption:

**Before Fix:**
```
Raw JSON (110 bytes): {"test_id":"HM4TS6AZ","description":"walking_outdoor","labels"n␀{"test_id":"HM4TS6AZ","description":"walking_o
Warning: Non-printable character at position 63 (byte value: 0)
```

**Expected After Fix:**
```
Raw JSON (110 bytes): {"test_id":"HM4TS6AZ","description":"walking_outdoor","labels":["walking","outdoor"]}
```

## Prerequisites

1. ESP32 DataLogger IoT with firmware uploaded (already completed)
2. Tiny Code Reader connected via I2C (address 0x0C)
3. QR code generated with `tools/qr_generator/generate_qr.py`
4. Serial monitor access at 115200 baud

## Testing Procedure

### Step 1: Open Serial Monitor

```bash
pio device monitor --baud 115200
```

Or use your preferred serial monitor tool at 115200 baud.

### Step 2: Generate Test QR Code

```bash
cd tools/qr_generator
python3 -m venv venv && source venv/bin/activate && pip install -r requirements.txt  # First time only
python generate_qr.py --description "walking_outdoor" --labels "walking,outdoor"
```

This will create a QR code in `qr_codes/` directory with JSON format:
```json
{"test_id":"HM4TS6AZ","description":"walking_outdoor","labels":["walking","outdoor"]}
```

### Step 3: Reset Device and Monitor Boot

Watch for I2C initialization messages:
```
[I2C] Scanning for devices...
[I2C] Device found at 0x0C  <-- Tiny Code Reader
[I2C] Device found at 0x36  <-- Fuel Gauge
[I2C] Device found at 0x42  <-- GPS
[I2C] Device found at 0x6B  <-- IMU
[I2C] Device found at 0x6F  <-- Button
```

### Step 4: Trigger QR Scan

1. Press the Qwiic Button to enter AWAITING_QR state
2. Hold the generated QR code in front of the Tiny Code Reader
3. Wait for the scan (typically 1-5 seconds)

### Step 5: Verify Serial Output

Look for QR scan output in serial monitor. You should see:

**SUCCESS INDICATORS:**
```
[QR] Code detected!
Raw JSON (110 bytes): {"test_id":"HM4TS6AZ","description":"walking_outdoor","labels":["walking","outdoor"]}
[QR] Successfully parsed metadata
[QR] Test ID: HM4TS6AZ
[QR] Description: walking_outdoor
[QR] Labels: walking, outdoor
[STATE] AWAITING_QR -> RECORDING
```

**FAILURE INDICATORS (if fix didn't work):**
```
Warning: Non-printable character at position 63 (byte value: 0)
[QR] Failed to parse JSON
[QR] JSON parsing failed: InvalidInput
```

### Step 6: Test Multiple QR Codes

Generate and test several different QR codes to ensure consistency:

```bash
# Test with different content lengths
python generate_qr.py --description "short" --labels "test"
python generate_qr.py --description "very_long_description_for_testing_buffer_limits" --labels "long,test,multiple,labels,here"
python generate_qr.py --description "special_chars_!@#$%" --labels "special"
```

For each QR code:
1. Reset to IDLE state (press button if in RECORDING)
2. Press button to enter AWAITING_QR
3. Scan QR code
4. Verify complete JSON is received without null bytes or duplication

### Step 7: Extended Reliability Test

Perform 10 consecutive scans:
1. Generate a single test QR code
2. Scan it 10 times (reset to IDLE between scans)
3. Verify all 10 scans parse successfully
4. Expected success rate: 100% (10/10)

## Success Criteria

- [ ] All 6 I2C devices detected during boot
- [ ] QR scan returns complete JSON string with no null bytes
- [ ] JSON parsing succeeds without errors
- [ ] Test ID, description, and labels correctly extracted
- [ ] State machine transitions to RECORDING after successful scan
- [ ] 100% success rate over 10 consecutive scans
- [ ] No compiler errors (warnings about ambiguous function calls are acceptable)

## Troubleshooting

### QR Code Not Detected

**Symptom:** No output after scanning QR code

**Solutions:**
1. Ensure QR code is well-lit and in focus (5-15cm distance)
2. Verify Tiny Code Reader is at I2C address 0x0C during boot scan
3. Check Wire connections (Qwiic cables should be firmly seated)
4. Try regenerating QR code with larger size (default is 10 modules per box)

### Partial JSON Data

**Symptom:** JSON is truncated but no null bytes

**Solutions:**
1. Verify content length in serial output matches actual JSON length
2. Check for I2C timeout errors in serial output
3. Ensure adequate power supply (5V/1A minimum)

### I2C Communication Failure

**Symptom:** "Only X bytes available on I2C, but we need Y"

**Solutions:**
1. Reduce I2C bus speed (currently 400kHz, try 100kHz)
2. Check for I2C bus contention (6 devices on single bus)
3. Add external I2C pull-up resistors if using long cables (4.7kΩ typical)

### Still Seeing Null Bytes

**Symptom:** Corruption still occurs after applying patch

**Verification:**
1. Confirm patched library is in use:
   ```bash
   grep -r "ESP32 supports full read" lib/tiny_code_reader_arduino/
   ```
   Should return a match in `tiny_code_reader.h`

2. Verify platformio.ini is using local library:
   ```bash
   grep "tiny_code_reader" platformio.ini
   ```
   Should show commented line: `; https://github.com/usefulsensors/tiny_code_reader_arduino`

3. Rebuild from scratch:
   ```bash
   pio run --target clean
   pio run --target upload
   ```

## Technical Details

### Library Changes

**File:** `lib/tiny_code_reader_arduino/tiny_code_reader.h` (lines 52-56)

**Before:**
```cpp
const int maxBytesPerChunk = 64;
```

**After:**
```cpp
#if defined(ESP32) || defined(ESP8266)
    const int maxBytesPerChunk = 256;  // ESP32 supports full read
#else
    const int maxBytesPerChunk = 32;   // Arduino Uno buffer limit
#endif
```

### Build Configuration

**File:** `platformio.ini` (line 24)

**Changed from:**
```ini
lib_deps =
    https://github.com/usefulsensors/tiny_code_reader_arduino
```

**Changed to:**
```ini
lib_deps =
    ; https://github.com/usefulsensors/tiny_code_reader_arduino  ; Disabled: using patched version in /lib
```

### Expected Build Output

```
Dependency Graph
|-- tiny_code_reader_arduino @ 0.0.0+20251112201336-esp32-patch
```

Note the version suffix `-esp32-patch` indicating patched library is in use.

## References

- Original Issue: QR data corruption with null bytes at position 63
- Root Cause: I2C read pointer reset on each `Wire.requestFrom()` call
- Fix: Platform-specific chunk sizes (256 bytes for ESP32, 32 bytes for Arduino Uno)
- Documentation: `lib/tiny_code_reader_arduino/ESP32_PATCH_NOTES.md`

## Contact

For issues with this fix, create a new Linear issue in the M3 Data Logger project.
