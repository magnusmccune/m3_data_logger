# Manual Test Plan: M3L-57 - State Machine Architecture

**Test Date**: _____________
**Tested By**: _____________
**Firmware Version**: 0.2.0-dev
**Device**: SparkFun DataLogger IoT (DEV-22462)

## Overview

This manual test plan validates the state machine implementation in `src/main.cpp`. The state machine manages four states (IDLE, AWAITING_QR, RECORDING, ERROR) with LED patterns and automatic timeouts.

**Test Environment**:
- Hardware: ESP32-based DataLogger IoT connected via USB
- Serial Monitor: 115200 baud
- LED: Status LED on GPIO25

**Test Constraints**:
- Button interrupt handler not yet implemented (M3L-58)
- QR scanner not yet integrated (M3L-60)
- IMU not yet integrated (M3L-61)
- Tests focus on state machine logic, LED patterns, and timeout behavior

---

## Test Section 1: Build and Deployment Validation

### TC-1.1: Firmware Build Success
**Objective**: Verify firmware compiles without errors

**Steps**:
1. Run `pio run` from project root
2. Check for compilation errors
3. Record RAM and Flash usage

**Expected Results**:
- [ ] Build completes successfully
- [ ] RAM usage < 50% (target: < 163,840 bytes)
- [ ] Flash usage < 80% (target: < 1,048,576 bytes)
- [ ] No warnings or errors

**Actual Results**:
```
RAM:   _____ % (used _______ bytes from 327680 bytes)
Flash: _____ % (used _______ bytes from 1310720 bytes)
Build Status: _______________
Warnings/Errors: _______________
```

---

### TC-1.2: Firmware Upload and Boot
**Objective**: Verify firmware uploads and boots successfully

**Steps**:
1. Connect device via USB
2. Run `pio run --target upload`
3. Open serial monitor: `pio device monitor --baud 115200`
4. Observe boot messages

**Expected Results**:
- [ ] Firmware uploads without errors
- [ ] Device boots and prints initialization banner
- [ ] Serial output shows firmware version and build date
- [ ] Hardware information displayed
- [ ] SD card initialization completes (or shows warning if no card)
- [ ] I2C initialization completes
- [ ] Device enters IDLE state

**Actual Results**:
```
Upload Status: _______________
Boot Messages: _______________
Initial State: _______________
```

---

## Test Section 2: Serial Output Validation

### TC-2.1: Initialization Messages
**Objective**: Verify startup serial output is correct and complete

**Steps**:
1. Reset device (press reset button or reconnect USB)
2. Capture serial output from boot

**Expected Results**:
- [ ] Banner displays "M3 Data Logger - Initializing"
- [ ] Firmware version shows "0.2.0-dev"
- [ ] Build date and time shown
- [ ] Hardware info section displays:
  - [ ] Board name: SparkFun DataLogger IoT
  - [ ] MCU: ESP32-WROOM-32E
  - [ ] CPU frequency (240 MHz typical)
  - [ ] Flash size (4 MB)
  - [ ] Free heap memory
- [ ] SD card initialization messages appear
- [ ] I2C initialization messages appear
- [ ] Final message: "Current State: IDLE"

**Actual Results**:
```
[Paste relevant serial output here]
```

---

### TC-2.2: Heartbeat Messages
**Objective**: Verify periodic heartbeat shows correct state information

**Steps**:
1. Let device run in IDLE state
2. Observe serial output for 30 seconds
3. Count heartbeat messages

**Expected Results**:
- [ ] Heartbeat appears every 5 seconds
- [ ] Heartbeat format: "♥ Heartbeat: [uptime]s uptime | Free Heap: [bytes] bytes | State: IDLE | Time in state: [seconds]s"
- [ ] Uptime increments correctly
- [ ] Free heap remains stable (no continuous decrease)
- [ ] State shows "IDLE"
- [ ] Time in state increments

**Actual Results**:
```
Number of heartbeats in 30s: _____
Sample heartbeat message:
_______________________________________________
Free heap trend: _______________
```

---

### TC-2.3: Heap Memory Stability
**Objective**: Verify no memory leaks during extended operation

**Steps**:
1. Note initial free heap from first heartbeat
2. Let device run for 10 minutes
3. Record heap values every minute from heartbeat messages

**Expected Results**:
- [ ] Free heap remains stable (variation < 1000 bytes)
- [ ] No continuous downward trend
- [ ] No out-of-memory errors
- [ ] Device does not reboot unexpectedly

**Actual Results**:
```
Time (min) | Free Heap (bytes)
-----------|------------------
0          | _____________
1          | _____________
2          | _____________
3          | _____________
4          | _____________
5          | _____________
6          | _____________
7          | _____________
8          | _____________
9          | _____________
10         | _____________

Heap variation: _____ bytes
Stability: PASS / FAIL
Unexpected reboots: YES / NO
```

---

## Test Section 3: LED Pattern Validation

### TC-3.1: IDLE State LED (OFF)
**Objective**: Verify LED is OFF in IDLE state

**Prerequisites**: Device must have status LED visible (GPIO25)

**Steps**:
1. Device boots into IDLE state
2. Wait for initialization blink sequence to complete (3 blinks)
3. Observe LED status

**Expected Results**:
- [ ] LED is completely OFF after initialization
- [ ] LED remains OFF during IDLE state
- [ ] No flickering or dimming

**Actual Results**:
```
LED Status: _______________
Notes: _______________
```

---

### TC-3.2: Initialization Blink Sequence
**Objective**: Verify startup LED blink pattern

**Steps**:
1. Reset device
2. Count LED blinks after initialization completes
3. Measure blink timing

**Expected Results**:
- [ ] LED blinks exactly 3 times
- [ ] Each blink: 200ms ON, 200ms OFF
- [ ] Sequence completes in ~1.2 seconds
- [ ] LED turns OFF after sequence

**Actual Results**:
```
Number of blinks: _____
Timing observed: _______________
Final state: _______________
```

---

## Test Section 4: State Machine Timeout Logic

**IMPORTANT**: These tests require temporary code modifications to trigger state transitions, as button/QR handlers are not yet implemented.

### TC-4.1: AWAITING_QR Timeout (30 seconds)
**Objective**: Verify automatic return to IDLE after QR scan timeout

**Test Setup** - Add to `setup()` function after initialization:
```cpp
// TEMPORARY: Force AWAITING_QR state for testing
delay(2000);  // Wait for serial output
transitionState(SystemState::AWAITING_QR, "MANUAL TEST");
```

**Steps**:
1. Upload modified firmware
2. Open serial monitor
3. Observe LED pattern (should be slow blink)
4. Wait 30 seconds
5. Observe state transition

**Expected Results**:
- [ ] Device enters AWAITING_QR state on boot
- [ ] LED blinks slowly (~1Hz: 500ms on, 500ms off)
- [ ] Serial shows: "→ Entered AWAITING_QR state: Activate QR scanner (30s timeout)"
- [ ] After exactly 30 seconds:
  - [ ] State transition message appears: "STATE_CHANGE: AWAITING_QR → IDLE (QR scan timeout (30s))"
  - [ ] LED stops blinking and turns OFF
  - [ ] Device returns to IDLE state
- [ ] Time in state shown in transition message is ~30s

**Actual Results**:
```
State transition time: _____ seconds
LED behavior: _______________
Serial messages:
_______________________________________________
```

---

### TC-4.2: ERROR State Auto-Recovery (60 seconds)
**Objective**: Verify automatic recovery from ERROR state

**Test Setup** - Add to `setup()` function:
```cpp
// TEMPORARY: Force ERROR state for testing
delay(2000);
transitionState(SystemState::ERROR, "MANUAL TEST");
```

**Steps**:
1. Upload modified firmware
2. Open serial monitor
3. Observe LED pattern (should be fast blink)
4. Wait 60 seconds
5. Observe state transition

**Expected Results**:
- [ ] Device enters ERROR state on boot
- [ ] LED blinks rapidly (~5Hz: 100ms on, 100ms off)
- [ ] Serial shows: "→ Entered ERROR state: 60s auto-recovery timer started"
- [ ] After exactly 60 seconds:
  - [ ] State transition message: "STATE_CHANGE: ERROR → IDLE (auto-recovery timeout (60s))"
  - [ ] LED stops blinking and turns OFF
  - [ ] Device returns to IDLE state
- [ ] Time in state shown in transition message is ~60s

**Actual Results**:
```
State transition time: _____ seconds
LED behavior: _______________
Serial messages:
_______________________________________________
```

---

### TC-4.3: Multiple Timeout Cycles
**Objective**: Verify timeout behavior is repeatable

**Test Setup** - Add to `loop()` function:
```cpp
// TEMPORARY: Cycle through states for testing
static unsigned long lastCycle = 0;
static int cycleCount = 0;

if (currentState == SystemState::IDLE && millis() - lastCycle > 35000) {
    // Only run 3 cycles
    if (cycleCount < 3) {
        transitionState(SystemState::AWAITING_QR, "TEST CYCLE");
        lastCycle = millis();
        cycleCount++;
    }
}
```

**Steps**:
1. Upload modified firmware
2. Observe device for 2 minutes
3. Count state transitions
4. Monitor heap usage

**Expected Results**:
- [ ] Device cycles: IDLE → AWAITING_QR → IDLE (3 times)
- [ ] Each AWAITING_QR period lasts 30 seconds
- [ ] Transitions are clean (no errors)
- [ ] Heap memory remains stable across cycles
- [ ] LED patterns correct for each state

**Actual Results**:
```
Cycles completed: _____
Heap stability: PASS / FAIL
Transition timing accuracy: _______________
Issues observed: _______________
```

---

## Test Section 5: State Transition Validation

### TC-5.1: State Transition Logging
**Objective**: Verify state transitions are logged with correct information

**Test Setup**: Use TC-4.1 or TC-4.2 setup to trigger transitions

**Steps**:
1. Trigger a state transition
2. Examine serial output

**Expected Results**:
- [ ] Transition log includes:
  - [ ] Timestamp in milliseconds
  - [ ] "STATE_CHANGE:" prefix
  - [ ] Old state name
  - [ ] "→" separator
  - [ ] New state name
  - [ ] Reason in parentheses
  - [ ] Free heap value
- [ ] Format: `[12345 ms] STATE_CHANGE: OLD → NEW (reason) | Free Heap: 12345 bytes`
- [ ] Entry message for new state appears after transition

**Actual Results**:
```
Sample transition log:
_______________________________________________
Format correct: YES / NO
Issues: _______________
```

---

### TC-5.2: Prevent Redundant Transitions
**Objective**: Verify transition to same state is ignored

**Test Setup** - Add to `loop()`:
```cpp
// TEMPORARY: Attempt redundant transition
static bool tested = false;
if (!tested && millis() > 10000) {
    Serial.println("Attempting transition to current state...");
    transitionState(SystemState::IDLE, "REDUNDANT TEST");
    tested = true;
}
```

**Steps**:
1. Upload firmware
2. Wait 10 seconds
3. Check serial output

**Expected Results**:
- [ ] "Attempting transition to current state..." message appears
- [ ] No STATE_CHANGE message appears
- [ ] State remains IDLE
- [ ] LED remains OFF

**Actual Results**:
```
Redundant transition handled: YES / NO
Serial output:
_______________________________________________
```

---

## Test Section 6: Edge Cases and Error Conditions

### TC-6.1: Boot Without SD Card
**Objective**: Verify graceful handling of missing SD card

**Steps**:
1. Remove SD card from device
2. Upload firmware and boot
3. Observe serial output and LED

**Expected Results**:
- [ ] SD card error messages appear
- [ ] Device transitions to ERROR state
- [ ] LED blinks rapidly (ERROR pattern)
- [ ] Device continues to run (no crash)
- [ ] Heartbeat messages continue
- [ ] After 60s, device recovers to IDLE

**Actual Results**:
```
Error handling: PASS / FAIL
Device stability: _______________
Notes: _______________
```

---

### TC-6.2: Extended Operation (Endurance Test)
**Objective**: Verify device stability over extended period

**Steps**:
1. Upload unmodified firmware
2. Let device run for 4 hours
3. Monitor via serial (can leave unattended)
4. Check for crashes or anomalies

**Expected Results**:
- [ ] Device runs continuously without reboot
- [ ] Heartbeat appears consistently every 5 seconds
- [ ] Heap memory remains stable
- [ ] No error messages in serial output
- [ ] LED remains in correct state (OFF for IDLE)

**Actual Results**:
```
Runtime: _____ hours
Unexpected reboots: _____
Final free heap: _____ bytes
Issues: _______________
```

---

## Test Section 7: LED Timing Accuracy

### TC-7.1: Slow Blink Timing (AWAITING_QR)
**Objective**: Verify LED blinks at correct 1Hz frequency

**Test Setup**: Use TC-4.1 setup to enter AWAITING_QR state

**Steps**:
1. Enter AWAITING_QR state
2. Count LED blinks for 30 seconds
3. Measure individual blink duration

**Expected Results**:
- [ ] Approximately 30 blinks in 30 seconds (1 Hz)
- [ ] Each blink: ~500ms ON, ~500ms OFF
- [ ] Consistent timing (no drift)

**Actual Results**:
```
Blinks counted: _____ in 30 seconds
Measured frequency: _____ Hz
Timing consistency: GOOD / FAIR / POOR
```

---

### TC-7.2: Fast Blink Timing (ERROR)
**Objective**: Verify LED blinks at correct 5Hz frequency

**Test Setup**: Use TC-4.2 setup to enter ERROR state

**Steps**:
1. Enter ERROR state
2. Count LED blinks for 10 seconds
3. Observe blink pattern

**Expected Results**:
- [ ] Approximately 50 blinks in 10 seconds (5 Hz)
- [ ] Each blink: ~100ms ON, ~100ms OFF
- [ ] Rapid, consistent blinking

**Actual Results**:
```
Blinks counted: _____ in 10 seconds
Measured frequency: _____ Hz
Visual assessment: _______________
```

---

## Test Summary

### Overall Results

| Test Section | Total Tests | Passed | Failed | Skipped |
|--------------|-------------|--------|--------|---------|
| Build/Deploy | 2 | ___ | ___ | ___ |
| Serial Output | 3 | ___ | ___ | ___ |
| LED Patterns | 2 | ___ | ___ | ___ |
| Timeouts | 3 | ___ | ___ | ___ |
| Transitions | 2 | ___ | ___ | ___ |
| Edge Cases | 2 | ___ | ___ | ___ |
| LED Timing | 2 | ___ | ___ | ___ |
| **TOTAL** | **16** | ___ | ___ | ___ |

### Critical Issues Found
```
1. _______________________________________________
2. _______________________________________________
3. _______________________________________________
```

### Non-Critical Issues / Observations
```
1. _______________________________________________
2. _______________________________________________
3. _______________________________________________
```

### Recommendations
```
1. _______________________________________________
2. _______________________________________________
3. _______________________________________________
```

### Test Completion
- [ ] All critical tests passed
- [ ] Known issues documented
- [ ] Test report generated
- [ ] Firmware version verified
- [ ] Ready for sensor integration (M3L-58, M3L-60, M3L-61)

**Tester Signature**: _______________ **Date**: _______________
