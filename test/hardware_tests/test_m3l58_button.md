# M3L-58 Button Interrupt Handler - Hardware Test Procedure

**Feature**: Button interrupt handler with debouncing
**Hardware Required**: SparkFun DataLogger IoT with Qwiic Button (BOB-15932)
**Test Engineer**: _____________
**Date**: _____________
**Build Version**: _____________

## Purpose

This document provides step-by-step manual test procedures to validate the button interrupt handler implementation (M3L-58). These tests verify:
- Button press detection and response time (<100ms NFR)
- Hardware debouncing (50ms in ISR)
- State machine integration (all state transitions)
- Visual feedback (LED patterns)
- Reliability across 100+ button presses

## Prerequisites

### Hardware Setup
- [ ] SparkFun DataLogger IoT connected via USB
- [ ] Qwiic Button (Red LED) connected via Qwiic cable
- [ ] MicroSD card inserted (FAT32, MBR partition table)
- [ ] LiPo battery connected OR USB power supply
- [ ] All Qwiic sensors connected (IMU at 0x6B, Button at 0x6F, QR reader at 0x0C)

### Software Setup
- [ ] Firmware uploaded successfully (pio run --target upload)
- [ ] Serial monitor open at 115200 baud (pio device monitor --baud 115200)
- [ ] Serial monitor shows successful boot sequence (SD card mounted, I2C devices detected)

### Expected Boot Output
```
M3 Data Logger Initializing...
[0.256s] INFO: SD Card Level Shifter enabled on GPIO32
[0.512s] INFO: SD Card mounted successfully | Type: SDHC | Size: 30436MB
[0.768s] INFO: I2C initialized | SDA: GPIO21, SCL: GPIO22
[1.024s] INFO: Qwiic button initialized
[1.280s] INFO: I2C Device Scan found 5 devices at addresses: 0x0C, 0x36, 0x6B, 0x6F, 0x7E
[1.536s] INFO: System initialized | State: IDLE | Free Heap: 302732 bytes
```

### Initial State Verification
- [ ] Device in IDLE state (serial log confirms)
- [ ] Status LED is OFF
- [ ] Button LED is OFF
- [ ] No error messages in serial log

---

## Test Cases

### TC1: Basic Button Press Detection (IDLE → AWAITING_QR)

**Objective**: Verify button press triggers state transition from IDLE to AWAITING_QR

**Preconditions**:
- Device in IDLE state
- Status LED OFF
- Serial monitor open

**Test Steps**:
1. Press Qwiic button once (firm press, ~1 second hold)
2. Observe button LED behavior
3. Observe status LED behavior
4. Check serial monitor output

**Expected Results**:
- [ ] Button LED lights up for ~100ms (visual confirmation)
- [ ] Status LED begins slow blink pattern (1Hz, 500ms on/off)
- [ ] Serial log shows state transition:
  ```
  [XXXX ms] STATE_CHANGE: IDLE → AWAITING_QR (button pressed) | Free Heap: XXXXXX bytes
  → Entered AWAITING_QR state: Activate QR scanner (30s timeout)
  ```
- [ ] State transition occurs within 100ms of button press (check timestamps)

**Pass/Fail**: _______
**Notes**: _______________________________________________________

---

### TC2: QR Scan Timeout (AWAITING_QR → IDLE)

**Objective**: Verify 30-second timeout in AWAITING_QR state returns to IDLE

**Preconditions**:
- Device in AWAITING_QR state (from TC1)
- Status LED slow blinking

**Test Steps**:
1. Wait 30 seconds without pressing button or scanning QR code
2. Observe LED behavior
3. Check serial monitor output

**Expected Results**:
- [ ] Status LED stops blinking after exactly 30 seconds
- [ ] Status LED turns OFF (enters IDLE state)
- [ ] Serial log shows timeout transition:
  ```
  [XXXX ms] STATE_CHANGE: AWAITING_QR → IDLE (QR scan timeout (30s)) | Free Heap: XXXXXX bytes
  → Entered IDLE state: Waiting for button press
  ```
- [ ] Heap usage remains stable (no memory leak)

**Pass/Fail**: _______
**Notes**: _______________________________________________________

---

### TC3: Button Press During AWAITING_QR (Cancel Operation)

**Objective**: Verify button press during AWAITING_QR cancels QR scan and returns to IDLE

**Preconditions**:
- Device in AWAITING_QR state
- Status LED slow blinking

**Test Steps**:
1. Press button once within 30-second timeout window
2. Observe button LED behavior
3. Observe status LED behavior
4. Check serial monitor output

**Expected Results**:
- [ ] Button LED lights up for ~100ms (visual confirmation)
- [ ] Status LED stops blinking immediately
- [ ] Status LED turns OFF (enters IDLE state)
- [ ] Serial log shows cancellation (implementation may vary):
  ```
  [XXXX ms] Button press detected during AWAITING_QR
  [XXXX ms] STATE_CHANGE: AWAITING_QR → IDLE (user canceled) | Free Heap: XXXXXX bytes
  ```
- [ ] Response time <100ms from button press

**Pass/Fail**: _______
**Notes**: _______________________________________________________

---

### TC4: Stop Recording (RECORDING → IDLE)

**Objective**: Verify button press stops active recording session

**Preconditions**:
- Device must be in RECORDING state
- Since QR scanner is not yet implemented (M3L-60), we simulate RECORDING state:
  - Option A: Manually trigger via serial command (if implemented)
  - Option B: Skip this test until M3L-60 is complete
  - Option C: Modify firmware temporarily to transition on second button press

**Test Steps**:
1. Get device into RECORDING state (status LED solid ON)
2. Wait 5 seconds to verify recording is active
3. Press button once
4. Observe LED behavior
5. Check serial monitor output

**Expected Results**:
- [ ] Button LED lights up for ~100ms (visual confirmation)
- [ ] Status LED turns OFF immediately (enters IDLE state)
- [ ] Serial log shows recording stop:
  ```
  [XXXX ms] STATE_CHANGE: RECORDING → IDLE (recording stopped via button) | Free Heap: XXXXXX bytes
  → Entered IDLE state: Waiting for button press
  ```
- [ ] Response time <100ms from button press

**Pass/Fail**: _______
**Notes**: _______________________________________________________
**Status**: ⚠️ SKIP if RECORDING state not accessible yet

---

### TC5: Manual Recovery from ERROR State

**Objective**: Verify button press in ERROR state attempts manual recovery

**Preconditions**:
- Device must be in ERROR state
- To trigger ERROR state:
  - Option A: Remove SD card and try to transition to RECORDING
  - Option B: Disconnect I2C bus temporarily
  - Option C: Manually trigger via serial command (if implemented)

**Test Steps**:
1. Get device into ERROR state (status LED fast blinking 5Hz)
2. Press button once
3. Observe LED behavior
4. Check serial monitor output

**Expected Results**:
- [ ] Button LED lights up for ~100ms (visual confirmation)
- [ ] If recovery successful: Status LED turns OFF (enters IDLE)
- [ ] If recovery failed: Status LED continues fast blinking
- [ ] Serial log shows recovery attempt:
  ```
  [XXXX ms] Button press detected (manual recovery attempt)
  [XXXX ms] STATE_CHANGE: ERROR → IDLE (manual recovery via button) | Free Heap: XXXXXX bytes
  ```
  OR
  ```
  [XXXX ms] Button press detected (manual recovery attempt)
  [XXXX ms] ERROR: Manual recovery failed - power cycle required
  ```
- [ ] Response time <100ms from button press

**Pass/Fail**: _______
**Notes**: _______________________________________________________
**Status**: ⚠️ SKIP if ERROR state not easily triggered

---

### TC6: Debounce Validation - Rapid Button Presses

**Objective**: Verify hardware and software debouncing prevents false triggers

**Preconditions**:
- Device in IDLE state
- Status LED OFF
- Serial monitor open with DEBUG logging enabled (if available)

**Test Steps**:
1. Press button rapidly 10 times in quick succession (~2-3 seconds total)
2. Count number of state transitions
3. Check serial monitor for debounce messages (if DEBUG enabled)

**Expected Results**:
- [ ] Only ONE state transition occurs (IDLE → AWAITING_QR)
- [ ] Subsequent button presses ignored due to:
  - ISR debounce (50ms threshold)
  - State lock (100ms since state change)
- [ ] Serial log shows single transition:
  ```
  [XXXX ms] STATE_CHANGE: IDLE → AWAITING_QR (button pressed) | Free Heap: XXXXXX bytes
  ```
- [ ] If DEBUG enabled, may see debounce messages:
  ```
  [XXXX ms] DEBUG: Button - ISR debounce (Xms since last press)
  [XXXX ms] DEBUG: Button - State lock (Xms since state change)
  ```
- [ ] No crashes, no stuck states

**Pass/Fail**: _______
**Notes**: _______________________________________________________

---

### TC7: Button Response Time Measurement

**Objective**: Measure time from button press to LED feedback (NFR1: <100ms)

**Preconditions**:
- Device in IDLE state
- Serial monitor open with timestamps enabled
- High-speed camera or stopwatch ready (optional)

**Test Steps**:
1. Note current timestamp in serial monitor
2. Press button while watching serial monitor
3. Record timestamp of state transition message
4. Calculate elapsed time

**Expected Results**:
- [ ] Time from button press to state transition log: <100ms
- [ ] Time from button press to LED change: <100ms (should be <20ms typical)
- [ ] Measured response time: _______ ms

**Pass/Fail**: _______
**Notes**: _______________________________________________________

---

### TC8: Button Held Down (No Continuous Triggering)

**Objective**: Verify holding button does not cause continuous state transitions

**Preconditions**:
- Device in IDLE state
- Status LED OFF

**Test Steps**:
1. Press and hold button for 10 seconds (do not release)
2. Observe LED behavior during hold
3. Release button after 10 seconds
4. Observe LED behavior after release
5. Check serial monitor for state transitions

**Expected Results**:
- [ ] Single state transition on button press (IDLE → AWAITING_QR)
- [ ] Status LED begins slow blink immediately, continues throughout hold
- [ ] No additional state transitions during 10-second hold
- [ ] No state transition on button release
- [ ] Serial log shows only one transition:
  ```
  [XXXX ms] STATE_CHANGE: IDLE → AWAITING_QR (button pressed) | Free Heap: XXXXXX bytes
  ```
- [ ] Device remains responsive after button release

**Pass/Fail**: _______
**Notes**: _______________________________________________________

---

### TC9: 100 Sequential Button Presses (Stress Test)

**Objective**: Verify button handling reliability over many presses (AC4 requirement)

**Preconditions**:
- Device in IDLE state
- Serial monitor open
- Note initial free heap value

**Test Steps**:
1. Perform 100 button presses with the following pattern:
   - Press button (IDLE → AWAITING_QR)
   - Wait 1 second
   - Press button again (AWAITING_QR → IDLE if cancel supported, or wait for timeout)
   - Wait 1 second
   - Repeat 100 times
2. Monitor serial output during test
3. Check final heap value

**Expected Results**:
- [ ] All 100 button presses detected successfully
- [ ] Each press triggers expected state transition
- [ ] No crashes or system hangs
- [ ] No false triggers or double-presses
- [ ] Heap memory remains stable (±5% of initial value):
  - Initial heap: _______ bytes
  - Final heap: _______ bytes
  - Difference: _______ bytes (should be <5% of initial)
- [ ] Serial log shows consistent behavior throughout test
- [ ] Device remains responsive after test completes

**Pass/Fail**: _______
**Notes**: _______________________________________________________
**Duration**: _______ minutes

---

### TC10: Visual Feedback Validation

**Objective**: Verify button LED provides clear visual confirmation

**Preconditions**:
- Device in IDLE state
- Room with moderate lighting (not too bright/dark)

**Test Steps**:
1. Press button in IDLE state
2. Observe button LED during press
3. Repeat in AWAITING_QR state
4. Repeat in RECORDING state (if accessible)
5. Repeat in ERROR state (if accessible)

**Expected Results**:
- [ ] Button LED lights at full brightness (255) during confirmation blink
- [ ] LED pulse duration is approximately 100ms (not too fast to see, not too slow)
- [ ] LED is clearly visible in normal room lighting
- [ ] LED turns off completely after confirmation blink
- [ ] LED behavior is consistent across all state transitions

**Pass/Fail**: _______
**Notes**: _______________________________________________________

---

## Test Data Recording Table

| Test Case | Timestamp | Result | Response Time (ms) | Notes |
|-----------|-----------|--------|-------------------|-------|
| TC1       |           | P/F    |                   |       |
| TC2       |           | P/F    |                   |       |
| TC3       |           | P/F    |                   |       |
| TC4       |           | P/F    |                   |       |
| TC5       |           | P/F    |                   |       |
| TC6       |           | P/F    |                   |       |
| TC7       |           | P/F    |                   |       |
| TC8       |           | P/F    |                   |       |
| TC9       |           | P/F    |                   |       |
| TC10      |           | P/F    |                   |       |

---

## Troubleshooting Guide

### Issue: Button press not detected at all

**Symptoms**: No LED change, no serial log messages

**Possible Causes**:
1. Qwiic Button not connected properly
2. Button not detected at I2C address 0x6F
3. Interrupt pin not configured correctly
4. ISR not attached

**Debug Steps**:
1. Check serial boot log for "Qwiic button initialized" message
2. Verify I2C scan shows device at 0x6F
3. Check Qwiic cable connection (try different cable)
4. Verify GPIO33 is not used by another peripheral
5. Re-upload firmware and check for compilation errors

---

### Issue: Button triggers multiple times from single press

**Symptoms**: State transitions rapidly (IDLE → AWAITING_QR → IDLE)

**Possible Causes**:
1. Debounce threshold too low
2. State lock not working correctly
3. Hardware issue with button (excessive bounce)

**Debug Steps**:
1. Enable DEBUG logging (add -DDEBUG_BUTTON to build_flags)
2. Check serial log for debounce messages
3. Verify BUTTON_DEBOUNCE_MS is set to 50ms
4. Try increasing debounce threshold to 75ms
5. Test with different button (may be defective)

---

### Issue: Slow response time (>100ms)

**Symptoms**: Noticeable delay between button press and LED change

**Possible Causes**:
1. Main loop blocked by other operations
2. I2C bus contention
3. Serial logging too verbose

**Debug Steps**:
1. Measure main loop iteration time (add timing logs)
2. Disable verbose serial logging temporarily
3. Check for blocking delays in loop()
4. Verify no other tasks blocking for >10ms

---

### Issue: Button works initially, then stops responding

**Symptoms**: First few presses work, then button becomes unresponsive

**Possible Causes**:
1. ISR flag not being cleared
2. Interrupt disabled accidentally
3. I2C bus lockup

**Debug Steps**:
1. Check if buttonPressed flag is stuck TRUE
2. Verify button.clearEventBits() is called after each press
3. Check for I2C errors in serial log
4. Power cycle device to reset I2C bus
5. Verify heap is not depleted (memory leak)

---

### Issue: Button LED not lighting up

**Symptoms**: State transitions work, but button LED never lights

**Possible Causes**:
1. LED control commands not reaching button
2. I2C communication issue
3. Button LED disabled or burned out

**Debug Steps**:
1. Verify I2C communication works (check other button functions)
2. Test LED manually via serial command: `button.LEDon(255);`
3. Check button library version (may be incompatible)
4. Try different Qwiic Button (LED may be defective)

---

## Acceptance Criteria Checklist

Based on M3L-58 acceptance criteria:

- [ ] **AC1**: Button press detected within 100ms (TC7 measures this)
- [ ] **AC2**: No false triggers from mechanical bounce (TC6 validates debouncing)
- [ ] **AC3**: ISR and state handling properly separated (code review + no crashes)
- [ ] **AC4**: Reliable across 100+ presses (TC9 stress test)

Additional validation:

- [ ] All test cases passed (TC1-TC10)
- [ ] No memory leaks detected (heap stable in TC9)
- [ ] Visual feedback clear and consistent (TC10)
- [ ] Device remains responsive after all tests
- [ ] No crashes or system hangs observed

---

## Test Summary

**Date**: _____________
**Test Engineer**: _____________
**Build Version**: _____________
**Hardware Serial Number**: _____________

**Overall Result**: PASS / FAIL / CONDITIONAL PASS

**Tests Passed**: _____ / 10
**Tests Failed**: _____
**Tests Skipped**: _____

**Critical Issues Found**:
1. _________________________________________________________________
2. _________________________________________________________________
3. _________________________________________________________________

**Non-Critical Issues Found**:
1. _________________________________________________________________
2. _________________________________________________________________
3. _________________________________________________________________

**Recommendations**:
_________________________________________________________________
_________________________________________________________________
_________________________________________________________________

**Approval**:
- [ ] Approved for integration with M3L-60 (QR Scanner)
- [ ] Approved for integration with M3L-61 (IMU Sampling)
- [ ] Requires fixes before approval

**Signatures**:

Test Engineer: _________________________ Date: _________
Tech Lead: _____________________________ Date: _________
