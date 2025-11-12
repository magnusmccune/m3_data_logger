# M3 Data Logger - Quick Testing Guide

**Last Updated**: 2025-11-12 | **Firmware**: v0.2.0-dev

## 30-Second Quick Start

```bash
# 1. Build and validate firmware
./tests/validation/build-tests.sh

# 2. Upload to device (if hardware available)
pio run --target upload

# 3. Monitor serial output
pio device monitor --baud 115200
```

---

## Test Artifacts at a Glance

| File | Purpose | When to Use |
|------|---------|-------------|
| `validation/build-tests.sh` | Automated build validation | Before every commit |
| `manual/M3L-57-state-machine-test-plan.md` | Hardware test checklist | After hardware changes |
| `integration/state-machine-integration.md` | Sensor integration tests | After M3L-58/60/61 |
| `reports/M3L-57-test-report-[DATE].md` | Test execution results | After completing tests |

---

## Test Status Dashboard

### M3L-57: State Machine Architecture

```
Build Validation:  ████████████████████ 100% COMPLETE (8/8 passed)
Manual Tests:      ░░░░░░░░░░░░░░░░░░░░   0% PENDING (hardware required)
Integration Tests: ░░░░░░░░░░░░░░░░░░░░   0% PLANNED (sensors required)
```

**Resource Usage**:
- RAM: 7.0% (22,780 / 327,680 bytes) - EXCELLENT
- Flash: 29.5% (386,685 / 1,310,720 bytes) - EXCELLENT

**Dependencies**: 4/4 resolved (ArduinoJson, ISM330DHCX, SD_MMC, Wire)

---

## Common Testing Tasks

### Task 1: Validate Build After Code Changes

```bash
cd /path/to/m3_data_logger
./tests/validation/build-tests.sh
```

**Expected**: All 8 tests pass in < 1 minute

### Task 2: Test Firmware on Device

```bash
# Upload
pio run --target upload

# Monitor (CTRL+C to exit)
pio device monitor --baud 115200
```

**Expected Serial Output**:
```
╔════════════════════════════════════════╗
║      M3 Data Logger - Initializing    ║
╚════════════════════════════════════════╝
Firmware Version: 0.2.0-dev
...
Current State: IDLE
```

### Task 3: Check LED Patterns (Manual)

| State | LED Pattern | How to Trigger |
|-------|-------------|----------------|
| IDLE | OFF | Device boot |
| AWAITING_QR | Slow blink (1 Hz) | Needs M3L-58 |
| RECORDING | Solid ON | Needs M3L-58, M3L-60 |
| ERROR | Fast blink (5 Hz) | Remove SD card and reboot |

### Task 4: Generate Test QR Codes

```bash
cd tools/qr_generator

# Basic test
python generate_qr.py --test "test_1" --labels "testing,basic"

# Save to file
python generate_qr.py --test "test_2" --labels "testing" --output test2.png
```

---

## Troubleshooting

### Build Test Fails

**Problem**: `./tests/validation/build-tests.sh` shows errors

**Solutions**:
1. Check PlatformIO installed: `pio --version`
2. Clean build: `pio run --target clean`
3. Check `platformio.ini` for syntax errors
4. Review error log: `/tmp/pio_build_output.txt`

### Device Not Detected

**Problem**: `pio run --target upload` can't find device

**Solutions**:
1. Check USB cable connected
2. List devices: `pio device list`
3. Check permissions (Linux): `sudo usermod -a -G dialout $USER`
4. Press BOOT button during upload (if auto-reset fails)

### Serial Monitor Shows Gibberish

**Problem**: Random characters instead of readable text

**Solutions**:
1. Set baud rate: `pio device monitor --baud 115200`
2. Reset device while monitor is open
3. Try different USB port
4. Check for multiple serial monitors open (close others)

### SD Card Not Detected

**Problem**: "SD card mount failed" in serial output

**Solutions**:
1. Ensure card is inserted (push until it clicks)
2. Format card as FAT32
3. Check card size (< 32GB for FAT32)
4. Check card lock switch (unlocked position)
5. Try different SD card

---

## Expected Test Timeline

### Phase 1: Build Validation (NOW)
- Duration: 5 minutes
- Tasks: Run `build-tests.sh`, verify compilation
- Status: COMPLETE

### Phase 2: Basic Hardware Tests (NOW - if device available)
- Duration: 30 minutes
- Tasks: Upload firmware, check serial output, verify LED
- Status: READY

### Phase 3: State Machine Manual Tests (NEXT - after M3L-58)
- Duration: 2 hours
- Tasks: Test timeouts, transitions, LED timing
- Status: PLANNED

### Phase 4: Integration Tests (LATER - after M3L-58/60/61)
- Duration: 4 hours
- Tasks: End-to-end workflows, multi-session testing
- Status: PLANNED

---

## Test Acceptance Criteria

### Build Validation (Automated)
- [ ] All 8 build tests pass
- [ ] RAM usage < 50%
- [ ] Flash usage < 80%
- [ ] Zero build warnings

### Manual Testing (Hardware)
- [ ] Device boots and initializes
- [ ] Serial output readable and correct
- [ ] LED shows correct patterns
- [ ] Heap memory stable for 10+ minutes
- [ ] Handles missing SD card gracefully

### Integration Testing (Full System)
- [ ] Button triggers state changes
- [ ] QR codes scan successfully
- [ ] IMU data recorded to SD card
- [ ] CSV files valid and parseable
- [ ] Multi-session recording works

---

## Quick Reference: Test Files

### Manual Test Plan
**File**: `tests/manual/M3L-57-state-machine-test-plan.md`
**Tests**: 16 test cases
**Duration**: ~2 hours with hardware

**Categories**:
1. Build and Deployment (2 tests)
2. Serial Output (3 tests)
3. LED Patterns (2 tests)
4. State Machine Timeouts (3 tests)
5. State Transitions (2 tests)
6. Edge Cases (2 tests)
7. LED Timing (2 tests)

### Integration Test Specs
**File**: `tests/integration/state-machine-integration.md`
**Tests**: 10 test scenarios
**Duration**: ~4 hours with hardware

**Scenarios**:
- IT-1: Button-Triggered Transitions
- IT-2: QR Code Scan Triggers Recording
- IT-3: Complete Recording Session
- IT-4: QR Scan Timeout
- IT-5: Invalid QR Code Handling
- IT-6: Button Press During Recording
- IT-7: Multiple Recording Sessions
- IT-8: Error Recovery
- IT-9: Concurrent LED Updates
- IT-10: Long Recording Session

### Build Validation
**File**: `tests/validation/build-tests.sh`
**Tests**: 8 automated checks
**Duration**: < 1 minute

**Checks**:
1. PlatformIO installation
2. Project structure
3. Clean build
4. Firmware binary exists
5. RAM usage
6. Flash usage
7. Build warnings
8. Dependencies

---

## Success Indicators

### Green Flags (All Good)
- Build tests: 8/8 passed
- Serial output: Clean initialization messages
- LED: Correct patterns for each state
- Heap: Stable over time (< 1000 byte variation)
- Firmware: No unexpected reboots

### Yellow Flags (Investigate)
- Build warnings present (even if build succeeds)
- LED timing slightly off (±10% acceptable)
- Heap decreases slowly (possible leak)
- Occasional serial glitches

### Red Flags (Action Required)
- Any build test fails
- Device crashes or reboots
- Heap continuously decreases
- LED patterns incorrect
- SD card errors persist with good card

---

## Next Steps

After completing current tests:

1. **Review Results**: Check `tests/reports/M3L-57-test-report-2025-11-12.md`
2. **Document Issues**: Create Linear issues for any bugs found
3. **Update Tests**: Add new test cases as sensors are integrated
4. **Plan Integration**: Prepare for M3L-58, M3L-60, M3L-61 testing

---

**For Detailed Information**: See `tests/README.md`
