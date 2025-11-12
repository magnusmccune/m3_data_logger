# State Machine Integration Tests

**Status**: PLANNED - Awaiting Sensor Integration
**Dependencies**: M3L-58 (Button), M3L-60 (QR Scanner), M3L-61 (IMU)

## Overview

This document defines integration tests for the M3 Data Logger state machine. These tests verify that the state machine correctly integrates with hardware sensors and external events (button presses, QR scans, IMU data).

**Current State**: State machine framework implemented, but hardware sensors not yet integrated.

**When to Implement**: After completing sensor integration tasks:
- M3L-58: Qwiic Button interrupt handling
- M3L-60: Tiny Code Reader QR scanning
- M3L-61: ISM330DHCX IMU data acquisition

---

## Test Environment Setup

### Hardware Requirements
- SparkFun DataLogger IoT (DEV-22462)
- ISM330DHCX IMU breakout connected via Qwiic
- Qwiic Button (Red LED) connected via Qwiic
- Tiny Code Reader connected via Qwiic
- MicroSD card (FAT32 formatted, 4GB+ recommended)
- USB cable for serial monitoring and power

### Software Requirements
- PlatformIO CLI
- Serial monitor (115200 baud)
- QR code generator (from `tools/qr_generator/`)
- Test QR codes in `data/test_qr_codes/`

### Test Data Preparation
Generate test QR codes before testing:
```bash
cd tools/qr_generator

# Generate basic test QR codes
python generate_qr.py --test "test_session_1" --labels "testing,basic" --output ../../data/test_qr_codes/test1.png
python generate_qr.py --test "test_session_2" --labels "testing,integration" --output ../../data/test_qr_codes/test2.png
python generate_qr.py --test "invalid_format" --labels "" --output ../../data/test_qr_codes/invalid.png

# Generate QR with special characters (edge case)
python generate_qr.py --test "test_special_chars" --labels "testing,special:chars,with-dashes" --output ../../data/test_qr_codes/special.png
```

---

## Integration Test Suite

### IT-1: Button-Triggered State Transitions (M3L-58 Required)

**Objective**: Verify button press correctly transitions state machine

**Dependencies**: M3L-58 (Qwiic Button initialization and interrupt handling)

**Preconditions**:
- Device in IDLE state
- Button interrupt handler configured
- Serial monitor open at 115200 baud

**Test Steps**:
1. Verify device is in IDLE state (LED OFF)
2. Press Qwiic button once
3. Observe state transition to AWAITING_QR
4. Observe LED begins slow blink (1 Hz)
5. Wait 30 seconds (timeout)
6. Observe automatic return to IDLE
7. Verify LED turns OFF

**Expected Serial Output**:
```
♥ Heartbeat: ... | State: IDLE | ...
[timestamp] STATE_CHANGE: IDLE → AWAITING_QR (button pressed) | Free Heap: ...
→ Entered AWAITING_QR state: Activate QR scanner (30s timeout)
... (30 seconds later) ...
[timestamp] STATE_CHANGE: AWAITING_QR → IDLE (QR scan timeout (30s)) | Free Heap: ...
→ Entered IDLE state: Waiting for button press
```

**Expected Behavior**:
- IDLE → AWAITING_QR transition occurs within 100ms of button press
- LED blinks consistently at 1 Hz
- Timeout occurs at 30s ± 0.5s
- Return to IDLE is automatic
- No memory leaks (heap stable)

**Verification Checklist**:
- [ ] Button press detected via interrupt
- [ ] State transition logged correctly
- [ ] LED pattern changes (OFF → slow blink)
- [ ] 30-second timeout accurate
- [ ] Automatic return to IDLE
- [ ] LED returns to OFF state
- [ ] No spurious button presses (debouncing works)

---

### IT-2: QR Code Scan Triggers Recording (M3L-60 Required)

**Objective**: Verify QR scan transitions from AWAITING_QR to RECORDING

**Dependencies**:
- M3L-58 (Button to enter AWAITING_QR)
- M3L-60 (QR scanner integration)

**Preconditions**:
- Device in IDLE state
- Test QR code available (test1.png displayed on phone/monitor)

**Test Steps**:
1. Press button to enter AWAITING_QR state
2. Present test QR code to scanner within 30 seconds
3. Observe state transition to RECORDING
4. Verify LED turns solid ON
5. Press button again to stop recording
6. Verify return to IDLE state

**Expected Serial Output**:
```
[timestamp] STATE_CHANGE: IDLE → AWAITING_QR (button pressed) | Free Heap: ...
→ Entered AWAITING_QR state: Activate QR scanner (30s timeout)
[QR scanner debug output showing scan attempt]
[timestamp] QR Code Scanned: {"test":"test_session_1","labels":["testing","basic"]}
[timestamp] STATE_CHANGE: AWAITING_QR → RECORDING (QR code scanned) | Free Heap: ...
→ Entered RECORDING state: Begin IMU data logging
... (variable duration) ...
[timestamp] STATE_CHANGE: RECORDING → IDLE (recording stopped) | Free Heap: ...
→ Entered IDLE state: Waiting for button press
```

**Expected Behavior**:
- QR scan detected within 2 seconds of presentation
- JSON metadata parsed correctly
- State transition: AWAITING_QR → RECORDING
- LED changes: slow blink → solid ON
- Recording starts immediately
- Button press stops recording
- Clean return to IDLE

**Verification Checklist**:
- [ ] QR code detected within scan window
- [ ] JSON metadata extracted correctly
- [ ] Metadata fields: "test" and "labels" present
- [ ] State transition to RECORDING
- [ ] LED solid ON during recording
- [ ] Button press stops recording
- [ ] Return to IDLE successful

---

### IT-3: Complete Recording Session Workflow (M3L-58, M3L-60, M3L-61 Required)

**Objective**: Verify full end-to-end recording session

**Dependencies**:
- M3L-58 (Button)
- M3L-60 (QR scanner)
- M3L-61 (IMU data sampling)
- M3L-62 (SD card file writing - implied)

**Preconditions**:
- Device in IDLE with SD card inserted
- IMU initialized and ready
- Test QR code prepared

**Test Steps**:
1. Press button (IDLE → AWAITING_QR)
2. Scan QR code (AWAITING_QR → RECORDING)
3. Move device to generate IMU data (shake, rotate, tilt)
4. Let recording run for 10 seconds
5. Press button to stop (RECORDING → IDLE)
6. Remove SD card and verify data file exists
7. Open CSV file and verify contents

**Expected Serial Output**:
```
[timestamp] STATE_CHANGE: IDLE → AWAITING_QR (button pressed)
[timestamp] QR Code Scanned: {"test":"test_session_1","labels":["testing","basic"]}
[timestamp] STATE_CHANGE: AWAITING_QR → RECORDING (QR code scanned)
[timestamp] Created data file: /sdcard/session_001.csv
[timestamp] IMU sampling started at 100 Hz
... (periodic buffer writes) ...
[timestamp] Buffer written: 100 samples
[timestamp] Buffer written: 100 samples
[timestamp] STATE_CHANGE: RECORDING → IDLE (recording stopped)
[timestamp] Data file closed: 1000 samples written
```

**Expected File Output** (`session_001.csv`):
```csv
session_id,timestamp_ms,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z
session_001,0,0.05,0.02,9.81,0.01,0.00,-0.02
session_001,10,0.06,0.03,9.80,0.02,0.01,-0.01
... (1000 samples for 10s at 100Hz) ...
```

**Verification Checklist**:
- [ ] Complete state sequence: IDLE → AWAITING_QR → RECORDING → IDLE
- [ ] CSV file created on SD card
- [ ] File named correctly (session_XXX.csv)
- [ ] CSV header row present
- [ ] Exactly 8 columns
- [ ] Sample rate ~100 Hz (verify timestamp intervals)
- [ ] Accelerometer values reasonable (gravity ~9.81 m/s² on Z-axis when flat)
- [ ] Gyroscope values change with motion
- [ ] No missing samples (consecutive timestamps)
- [ ] File closed properly (no corruption)

**Post-Test Analysis**:
```bash
# Count lines (should be ~1001 for 10s: 1 header + 1000 samples)
wc -l session_001.csv

# Verify CSV structure
head -n 5 session_001.csv

# Check sample rate
python3 <<EOF
import pandas as pd
df = pd.read_csv('session_001.csv')
df['timestamp_ms'] = pd.to_numeric(df['timestamp_ms'])
avg_interval = df['timestamp_ms'].diff().mean()
print(f"Average sample interval: {avg_interval:.2f} ms")
print(f"Estimated sample rate: {1000/avg_interval:.1f} Hz")
EOF
```

---

### IT-4: QR Scan Timeout Behavior (M3L-58, M3L-60 Required)

**Objective**: Verify timeout when no QR code is scanned

**Test Steps**:
1. Press button to enter AWAITING_QR
2. Do NOT present QR code
3. Wait 30 seconds
4. Verify automatic return to IDLE
5. Verify no data file created

**Expected Behavior**:
- LED blinks for exactly 30 seconds
- State returns to IDLE automatically
- No file created on SD card
- QR scanner deactivated

**Verification Checklist**:
- [ ] 30-second timeout accurate (±0.5s)
- [ ] Return to IDLE automatic
- [ ] LED stops blinking and turns OFF
- [ ] No CSV file created
- [ ] Memory stable (no leaks)

---

### IT-5: Invalid QR Code Handling (M3L-60 Required)

**Objective**: Verify graceful handling of invalid QR data

**Test Data**: Create invalid QR codes for testing
```bash
# Invalid JSON format
echo "this is not json" | qrencode -o invalid_json.png

# Missing required fields
echo '{"wrong_field":"value"}' | qrencode -o missing_fields.png

# Empty labels array
echo '{"test":"empty_labels","labels":[]}' | qrencode -o empty_labels.png
```

**Test Steps**:
1. Press button (IDLE → AWAITING_QR)
2. Present invalid QR code to scanner
3. Observe error handling
4. Verify state behavior

**Expected Serial Output** (for each invalid case):
```
[timestamp] QR Code Scanned: [raw data]
[timestamp] ERROR: Invalid QR metadata - [specific error]
[timestamp] STATE_CHANGE: AWAITING_QR → ERROR (invalid QR metadata)
→ Entered ERROR state: 60s auto-recovery timer started
... (60 seconds later) ...
[timestamp] STATE_CHANGE: ERROR → IDLE (auto-recovery timeout (60s))
```

**Expected Behavior**:
- Invalid QR triggers ERROR state
- LED blinks rapidly (5 Hz)
- Error message logged with details
- Auto-recovery after 60 seconds
- No data file created

**Verification Checklist**:
- [ ] Invalid JSON detected
- [ ] Missing fields detected
- [ ] Empty labels handled
- [ ] Transition to ERROR state
- [ ] Fast LED blink (5 Hz)
- [ ] Error message descriptive
- [ ] 60s auto-recovery works
- [ ] Return to IDLE successful

---

### IT-6: Button Press During Recording (M3L-58, M3L-60, M3L-61 Required)

**Objective**: Verify button correctly stops recording session

**Test Steps**:
1. Start recording session (IDLE → AWAITING_QR → RECORDING)
2. Let recording run for 5 seconds
3. Press button to stop
4. Verify immediate stop
5. Check data file integrity

**Expected Behavior**:
- Button press detected immediately
- Recording stops within 100ms
- LED turns OFF
- Data file closed properly
- All buffered data written to SD card
- Return to IDLE state

**Verification Checklist**:
- [ ] Button press stops recording immediately
- [ ] State transition: RECORDING → IDLE
- [ ] LED turns OFF
- [ ] Data file closed (not corrupted)
- [ ] CSV file complete and parseable
- [ ] Sample count matches duration (~500 samples for 5s)

---

### IT-7: Multiple Recording Sessions (M3L-58, M3L-60, M3L-61 Required)

**Objective**: Verify device can handle multiple consecutive sessions

**Test Steps**:
1. Perform 5 complete recording sessions back-to-back
2. Each session: different QR code, 10-second recording
3. Verify all files created correctly
4. Check session numbering

**Expected Files**:
```
/sdcard/session_001.csv
/sdcard/session_002.csv
/sdcard/session_003.csv
/sdcard/session_004.csv
/sdcard/session_005.csv
```

**Verification Checklist**:
- [ ] All 5 sessions complete successfully
- [ ] Files numbered sequentially
- [ ] Each file has correct metadata in filename or header
- [ ] No file corruption
- [ ] Heap memory stable across sessions
- [ ] No performance degradation
- [ ] SD card space sufficient

---

### IT-8: Error Recovery - Button Press (M3L-58 Required)

**Objective**: Verify button can manually recover from ERROR state

**Test Setup**: Force ERROR state (e.g., invalid QR code)

**Test Steps**:
1. Enter ERROR state (LED fast blink)
2. Wait 5 seconds
3. Press button
4. Verify immediate return to IDLE

**Expected Behavior**:
- Button press clears ERROR state
- Immediate transition to IDLE (no 60s wait)
- LED turns OFF
- Device ready for new session

**Verification Checklist**:
- [ ] Button press detected in ERROR state
- [ ] Immediate recovery (no timeout wait)
- [ ] LED stops blinking and turns OFF
- [ ] State is IDLE
- [ ] Device functional for next session

---

### IT-9: Concurrent LED and State Updates (All Hardware Required)

**Objective**: Verify LED patterns remain accurate during state transitions

**Test Steps**:
1. Rapidly cycle through states: IDLE → AWAITING_QR → IDLE → AWAITING_QR → RECORDING → IDLE
2. Observe LED patterns during each transition
3. Verify no LED glitches or incorrect patterns

**Expected Behavior**:
- LED immediately reflects current state
- No flickering during transitions
- Blink patterns consistent
- No LED stuck in wrong state

**Verification Checklist**:
- [ ] IDLE: LED OFF
- [ ] AWAITING_QR: Slow blink (1 Hz)
- [ ] RECORDING: Solid ON
- [ ] ERROR: Fast blink (5 Hz)
- [ ] Transitions are immediate (< 100ms)
- [ ] No visual glitches

---

### IT-10: Long Recording Session (M3L-58, M3L-60, M3L-61 Required)

**Objective**: Verify device handles extended recording periods

**Test Steps**:
1. Start recording session
2. Let recording run for 5 minutes
3. Monitor serial output for issues
4. Stop recording
5. Verify data file integrity

**Expected Results**:
- 5 minutes at 100 Hz = 30,000 samples
- File size ~2-3 MB (depending on float precision)
- Heap memory stable throughout
- No buffer overflows or dropped samples
- Consistent sample rate

**Verification Checklist**:
- [ ] Recording runs for full 5 minutes
- [ ] No crashes or resets
- [ ] Heap memory stable
- [ ] ~30,000 samples in CSV file
- [ ] No missing timestamps
- [ ] Sample rate consistent (100 Hz ±5%)
- [ ] File closes properly

---

## Integration Test Execution Guide

### Running Integration Tests

1. **Setup Hardware**:
   - Connect all sensors via Qwiic
   - Insert formatted SD card
   - Connect USB for power and serial monitoring

2. **Prepare Test Environment**:
   ```bash
   cd /path/to/m3_data_logger

   # Generate test QR codes
   cd tools/qr_generator
   python generate_qr.py --test "integration_test_1" --labels "testing,integration"
   cd ../..

   # Upload firmware
   pio run --target upload

   # Open serial monitor
   pio device monitor --baud 115200
   ```

3. **Execute Tests Sequentially**:
   - Follow test steps for each IT-1 through IT-10
   - Record results in test report
   - Save serial output logs
   - Archive CSV files for verification

4. **Post-Test Analysis**:
   ```bash
   # Analyze CSV files
   python tools/analyze_session_data.py /path/to/sdcard/session_*.csv

   # Check for data quality issues
   python tools/validate_csv.py /path/to/sdcard/session_001.csv
   ```

### Automation Possibilities

While full automation is challenging for hardware tests, some aspects can be semi-automated:

**Automated Build and Flash**:
```bash
#!/bin/bash
# tests/integration/auto-flash.sh
pio run --target upload && pio device monitor --baud 115200 > /tmp/serial_log.txt
```

**Automated Serial Log Analysis**:
```python
# tests/integration/analyze_serial.py
import re

def verify_state_transitions(log_file):
    """Parse serial log and verify state transition sequence"""
    with open(log_file, 'r') as f:
        content = f.read()

    # Find all state transitions
    transitions = re.findall(r'STATE_CHANGE: (\w+) → (\w+) \((.*?)\)', content)

    # Verify expected sequence
    expected = [('IDLE', 'AWAITING_QR'), ('AWAITING_QR', 'RECORDING'), ('RECORDING', 'IDLE')]
    # ... validation logic ...
```

**CSV Validation Script**:
```python
# tests/integration/validate_csv.py
import pandas as pd
import sys

def validate_session_csv(filepath):
    """Validate CSV structure and sample rate"""
    df = pd.read_csv(filepath)

    # Check columns
    expected_cols = ['session_id', 'timestamp_ms', 'accel_x', 'accel_y', 'accel_z', 'gyro_x', 'gyro_y', 'gyro_z']
    assert list(df.columns) == expected_cols, "Invalid CSV columns"

    # Check sample rate
    intervals = df['timestamp_ms'].diff().dropna()
    avg_interval = intervals.mean()
    assert 9 <= avg_interval <= 11, f"Sample rate out of range: {avg_interval} ms"

    print(f"✓ CSV validation passed: {len(df)} samples, {avg_interval:.2f} ms interval")

if __name__ == '__main__':
    validate_session_csv(sys.argv[1])
```

---

## Known Limitations and Future Work

### Current Limitations
- No automated test execution (requires manual button presses and QR presentations)
- No hardware-in-the-loop (HIL) test framework
- Serial output parsing is manual
- Cannot simulate sensor failures programmatically

### Future Improvements
- **HIL Testing**: Investigate PlatformIO remote testing or custom test harness
- **Button Emulation**: GPIO control from test PC to simulate button presses
- **QR Code Display**: Automated QR code display on connected screen
- **Serial Log Parser**: Automated validation of state transitions
- **Continuous Integration**: GitHub Actions workflow for build validation (hardware tests remain manual)

### Recommended Tools for Future Automation
- **Pytest** with serial port fixtures for log parsing
- **Robot Framework** for hardware test automation
- **PySerial** for automated serial communication
- **GPIO Control** via USB relay or test board for button simulation

---

## Test Completion Criteria

Integration tests are considered complete when:
- [ ] All 10 integration tests (IT-1 through IT-10) executed
- [ ] 90% of tests passed (9/10 minimum)
- [ ] All critical issues documented and triaged
- [ ] CSV data validated for correctness
- [ ] Serial logs archived
- [ ] Test report generated and reviewed
- [ ] Known issues added to project backlog

---

## References

- Main firmware: `src/main.cpp`
- Hardware initialization: `src/hardware_init.cpp`
- State machine documentation: `CLAUDE.md`
- QR code generator: `tools/qr_generator/generate_qr.py`
- Manual test plan: `tests/manual/M3L-57-state-machine-test-plan.md`
