# M3 Data Logger - Testing Documentation

This directory contains all testing artifacts for the M3 Data Logger firmware.

## Testing Strategy

The M3 Data Logger uses a multi-layered testing approach appropriate for embedded firmware:

```
┌─────────────────────────────────────────┐
│   Integration Tests (Hardware-based)    │  ← End-to-end workflows
├─────────────────────────────────────────┤
│   Manual Tests (Hardware-based)         │  ← Serial output, LED patterns
├─────────────────────────────────────────┤
│   Build Validation (Automated)          │  ← Compilation, resource usage
└─────────────────────────────────────────┘
```

## Directory Structure

```
tests/
├── README.md                           # This file
├── manual/                             # Manual test plans
│   └── M3L-57-state-machine-test-plan.md
├── validation/                         # Automated build tests
│   └── build-tests.sh
├── integration/                        # Integration test specs
│   └── state-machine-integration.md
└── reports/                            # Test execution reports
    └── M3L-57-test-report-2025-11-12.md
```

## Quick Start

### Running Build Validation Tests

```bash
# From project root
./tests/validation/build-tests.sh
```

**What it tests**:
- Firmware compilation
- RAM usage (< 50% threshold)
- Flash usage (< 80% threshold)
- Build warnings
- Library dependencies

**Requirements**: PlatformIO CLI installed

---

### Running Manual Tests

```bash
# Upload firmware to device
pio run --target upload

# Open serial monitor
pio device monitor --baud 115200

# Follow test plan
open tests/manual/M3L-57-state-machine-test-plan.md
```

**What it tests**:
- Serial output validation
- LED patterns (IDLE, AWAITING_QR, RECORDING, ERROR)
- State machine timeouts (30s for QR, 60s for ERROR)
- Memory stability over time
- Boot behavior with/without SD card

**Requirements**:
- SparkFun DataLogger IoT device
- USB cable
- MicroSD card (optional for some tests)

---

### Running Integration Tests (Future)

Integration tests are documented but cannot be executed until sensor integration is complete.

**Prerequisites**:
- M3L-58: Qwiic Button integration
- M3L-60: QR scanner integration
- M3L-61: IMU integration

**Test execution**:
```bash
# Prepare test QR codes
cd tools/qr_generator
python generate_qr.py --test "test_session_1" --labels "testing,integration"

# Upload firmware
pio run --target upload

# Open serial monitor
pio device monitor --baud 115200

# Follow integration test plan
open tests/integration/state-machine-integration.md
```

**What it tests**:
- Button-triggered state transitions
- QR code scanning workflows
- IMU data recording sessions
- Error handling and recovery
- Multi-session recording

---

## Test Coverage

### M3L-57: State Machine Architecture

| Component | Build Tests | Manual Tests | Integration Tests |
|-----------|-------------|--------------|-------------------|
| State Machine Framework | ✓ PASS | READY | PLANNED |
| LED Patterns | ✓ PASS | READY | PLANNED |
| Timeout Logic | ✓ PASS | READY | PLANNED |
| State Transitions | ✓ PASS | PARTIAL | PLANNED |
| Memory Management | ✓ PASS | READY | PLANNED |

**Status**: Build validation complete, manual/integration tests pending hardware

### Future Test Coverage (M3L-58, M3L-60, M3L-61)

Tests will be added as sensors are integrated:
- Button interrupt handling (M3L-58)
- QR metadata parsing (M3L-60)
- IMU data acquisition (M3L-61)
- SD card file I/O (M3L-62)

---

## Test Results Summary

### Latest Build Validation (2025-11-12)

| Metric | Value | Threshold | Status |
|--------|-------|-----------|--------|
| RAM Usage | 7.0% (22,780 bytes) | < 50% | ✓ PASS |
| Flash Usage | 29.5% (386,685 bytes) | < 80% | ✓ PASS |
| Build Warnings | 0 | 0 | ✓ PASS |
| Dependencies | 4 resolved | All | ✓ PASS |
| Total Tests | 8/8 passed | - | ✓ PASS |

**Detailed Report**: `tests/reports/M3L-57-test-report-2025-11-12.md`

---

## Testing Workflow

### For Developers

1. **Before Committing Code**:
   ```bash
   # Run build validation
   ./tests/validation/build-tests.sh
   ```

2. **After Adding Features**:
   - Update relevant test plans (manual or integration)
   - Add new test cases as needed
   - Execute tests and document results

3. **Before Pull Requests**:
   - All build tests must pass
   - Manual tests executed if hardware changes
   - Integration tests executed if sensor behavior changes
   - Test report updated

### For Testers

1. **Initial Setup**:
   - Assemble hardware (DataLogger IoT + sensors)
   - Install PlatformIO CLI
   - Generate test QR codes

2. **Test Execution**:
   - Follow manual test plan step-by-step
   - Record results in test plan document
   - Save serial output logs
   - Archive CSV data files for validation

3. **Reporting**:
   - Generate test report (use template in `reports/`)
   - Document issues found
   - Attach serial logs and screenshots

---

## Test Data

### Test QR Codes

Generate test QR codes using the generator tool:

```bash
cd tools/qr_generator

# Basic test sessions
python generate_qr.py --test "walking_test" --labels "walking,outdoor"
python generate_qr.py --test "sitting_test" --labels "sitting,indoor"

# Edge cases
python generate_qr.py --test "empty_labels" --labels ""
python generate_qr.py --test "special_chars" --labels "test:1,value-2,item_3"
```

**Location**: `data/test_qr_codes/`

### CSV Validation

After recording sessions, validate CSV files:

```python
# Example validation script
import pandas as pd

def validate_csv(filepath):
    df = pd.read_csv(filepath)

    # Check columns
    expected = ['session_id', 'timestamp_ms', 'accel_x', 'accel_y', 'accel_z', 'gyro_x', 'gyro_y', 'gyro_z']
    assert list(df.columns) == expected

    # Check sample rate
    intervals = df['timestamp_ms'].diff().dropna()
    avg_interval = intervals.mean()
    print(f"Average sample interval: {avg_interval:.2f} ms")
    print(f"Sample rate: {1000/avg_interval:.1f} Hz")

    return True

# Usage
validate_csv('/path/to/session_001.csv')
```

---

## Continuous Integration (Future)

### Planned CI/CD Integration

```yaml
# Future .github/workflows/build-test.yml
name: Build Tests
on: [push, pull_request]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Install PlatformIO
        run: pip install platformio
      - name: Run Build Tests
        run: ./tests/validation/build-tests.sh
```

**Status**: Planned for future implementation

---

## Known Limitations

### Current Testing Gaps

1. **No Automated Hardware Tests**: Button presses, QR scanning, and IMU movement cannot be automated without additional test infrastructure.

2. **No Unit Tests**: The firmware uses Arduino framework which makes traditional unit testing challenging. Focus is on integration and manual testing.

3. **No Performance Benchmarks**: Sample rate accuracy, SD write speed, and power consumption are not yet measured.

### Mitigation Strategies

- **Manual Testing**: Comprehensive manual test plans provide repeatability
- **Integration Tests**: End-to-end testing validates complete workflows
- **Build Validation**: Automated checks catch compilation and resource issues early

---

## Contributing

### Adding New Tests

1. **Manual Tests**: Add to `tests/manual/` with clear step-by-step instructions
2. **Integration Tests**: Document in `tests/integration/` with expected serial output
3. **Build Tests**: Extend `tests/validation/build-tests.sh` with new checks

### Test Report Format

Use this template for test reports:

```markdown
# Test Execution Report: [Issue ID] - [Feature Name]

**Test Date**: YYYY-MM-DD
**Firmware Version**: X.Y.Z
**Tested By**: [Name]

## Executive Summary
[Brief overview of testing and results]

## Test Results
[Detailed results with pass/fail status]

## Issues Found
[List of bugs or problems discovered]

## Recommendations
[Suggestions for improvements]
```

Save reports to `tests/reports/[ISSUE-ID]-test-report-[DATE].md`

---

## References

- **Main Firmware**: `src/main.cpp`
- **Hardware Init**: `src/hardware_init.cpp`
- **Project Docs**: `CLAUDE.md`, `PRD.md`
- **QR Generator**: `tools/qr_generator/`

---

## Contact

For questions about testing strategy or test execution:
- Review existing test plans in this directory
- Check project documentation in `CLAUDE.md`
- Consult Linear issue M3L-57 for state machine context
