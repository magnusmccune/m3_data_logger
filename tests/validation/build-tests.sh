#!/bin/bash

###############################################################################
# Build Validation Tests for M3 Data Logger
#
# This script validates that the firmware builds successfully and meets
# resource constraints for the ESP32 platform.
#
# Usage: ./build-tests.sh
#
# Requirements:
#   - PlatformIO CLI installed
#   - Project platformio.ini configured
#
# Exit Codes:
#   0 - All tests passed
#   1 - Build failed
#   2 - Resource constraints violated
###############################################################################

set -e  # Exit on first error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Resource thresholds
MAX_RAM_PERCENT=50
MAX_FLASH_PERCENT=80

# Test results
TESTS_PASSED=0
TESTS_FAILED=0

###############################################################################
# Helper Functions
###############################################################################

print_header() {
    echo -e "\n${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║           M3 Data Logger - Build Validation Tests         ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}\n"
}

print_test() {
    echo -e "${YELLOW}[TEST]${NC} $1"
}

print_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((TESTS_PASSED++))
}

print_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((TESTS_FAILED++))
}

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_summary() {
    echo -e "\n${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║                      Test Summary                          ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
    echo -e "Total Tests: $((TESTS_PASSED + TESTS_FAILED))"
    echo -e "${GREEN}Passed: ${TESTS_PASSED}${NC}"
    echo -e "${RED}Failed: ${TESTS_FAILED}${NC}"

    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "\n${GREEN}✓ All build validation tests passed${NC}\n"
        return 0
    else
        echo -e "\n${RED}✗ Some tests failed${NC}\n"
        return 1
    fi
}

###############################################################################
# Test Functions
###############################################################################

test_pio_installed() {
    print_test "Checking PlatformIO installation..."

    if command -v pio &> /dev/null; then
        local version=$(pio --version 2>&1 | head -n1)
        print_pass "PlatformIO installed: $version"
        return 0
    else
        print_fail "PlatformIO not found - install from https://platformio.org/"
        return 1
    fi
}

test_project_structure() {
    print_test "Validating project structure..."

    local missing_files=()

    # Check required files
    [ ! -f "platformio.ini" ] && missing_files+=("platformio.ini")
    [ ! -d "src" ] && missing_files+=("src/")
    [ ! -f "src/main.cpp" ] && missing_files+=("src/main.cpp")
    [ ! -d "include" ] && missing_files+=("include/")

    if [ ${#missing_files[@]} -eq 0 ]; then
        print_pass "All required project files present"
        return 0
    else
        print_fail "Missing files: ${missing_files[*]}"
        return 1
    fi
}

test_clean_build() {
    print_test "Performing clean build..."

    # Clean previous builds
    print_info "Cleaning build directory..."
    pio run --target clean > /dev/null 2>&1 || true

    # Build firmware
    print_info "Building firmware (this may take a minute)..."
    if pio run > /tmp/pio_build_output.txt 2>&1; then
        print_pass "Firmware built successfully"
        return 0
    else
        print_fail "Build failed - check /tmp/pio_build_output.txt"
        echo "Last 20 lines of build output:"
        tail -n 20 /tmp/pio_build_output.txt
        return 1
    fi
}

test_ram_usage() {
    print_test "Checking RAM usage..."

    # Extract RAM usage from build output
    local ram_line=$(grep "RAM:" /tmp/pio_build_output.txt | tail -n1)

    if [ -z "$ram_line" ]; then
        print_fail "Could not extract RAM usage from build output"
        return 1
    fi

    # Parse percentage (e.g., "RAM:   [=         ]   7.0%")
    local ram_percent=$(echo "$ram_line" | grep -oE '[0-9]+\.[0-9]+%' | sed 's/%//')
    local ram_used=$(echo "$ram_line" | grep -oE 'used [0-9]+ bytes' | awk '{print $2}')
    local ram_total=$(echo "$ram_line" | grep -oE 'from [0-9]+ bytes' | awk '{print $2}')

    print_info "RAM Usage: ${ram_percent}% (${ram_used} / ${ram_total} bytes)"

    # Check threshold
    if (( $(echo "$ram_percent < $MAX_RAM_PERCENT" | bc -l) )); then
        print_pass "RAM usage within threshold (< ${MAX_RAM_PERCENT}%)"
        return 0
    else
        print_fail "RAM usage exceeds threshold: ${ram_percent}% >= ${MAX_RAM_PERCENT}%"
        return 1
    fi
}

test_flash_usage() {
    print_test "Checking Flash usage..."

    # Extract Flash usage from build output
    local flash_line=$(grep "Flash:" /tmp/pio_build_output.txt | tail -n1)

    if [ -z "$flash_line" ]; then
        print_fail "Could not extract Flash usage from build output"
        return 1
    fi

    # Parse percentage
    local flash_percent=$(echo "$flash_line" | grep -oE '[0-9]+\.[0-9]+%' | sed 's/%//')
    local flash_used=$(echo "$flash_line" | grep -oE 'used [0-9]+ bytes' | awk '{print $2}')
    local flash_total=$(echo "$flash_line" | grep -oE 'from [0-9]+ bytes' | awk '{print $2}')

    print_info "Flash Usage: ${flash_percent}% (${flash_used} / ${flash_total} bytes)"

    # Check threshold
    if (( $(echo "$flash_percent < $MAX_FLASH_PERCENT" | bc -l) )); then
        print_pass "Flash usage within threshold (< ${MAX_FLASH_PERCENT}%)"
        return 0
    else
        print_fail "Flash usage exceeds threshold: ${flash_percent}% >= ${MAX_FLASH_PERCENT}%"
        return 1
    fi
}

test_build_warnings() {
    print_test "Checking for build warnings..."

    # Count warnings in build output
    local warning_count=$(grep -i "warning:" /tmp/pio_build_output.txt | wc -l | tr -d ' ')

    print_info "Build warnings found: $warning_count"

    if [ "$warning_count" -eq 0 ]; then
        print_pass "No build warnings"
        return 0
    else
        print_info "Warnings present (not a failure, but review recommended):"
        grep -i "warning:" /tmp/pio_build_output.txt | head -n 5
        if [ "$warning_count" -gt 5 ]; then
            print_info "... and $((warning_count - 5)) more warnings"
        fi
        print_pass "Build completed with warnings (non-critical)"
        return 0
    fi
}

test_firmware_exists() {
    print_test "Verifying firmware binary exists..."

    local firmware_path=".pio/build/sparkfun_datalogger_iot/firmware.bin"

    if [ -f "$firmware_path" ]; then
        local size=$(ls -lh "$firmware_path" | awk '{print $5}')
        print_pass "Firmware binary exists: $firmware_path ($size)"
        return 0
    else
        print_fail "Firmware binary not found at $firmware_path"
        return 1
    fi
}

test_dependency_resolution() {
    print_test "Checking library dependencies..."

    # Check if dependencies are resolved in build output
    if grep -q "Dependency Graph" /tmp/pio_build_output.txt; then
        print_info "Library dependencies:"
        grep -A 10 "Dependency Graph" /tmp/pio_build_output.txt | grep "|--" | while read line; do
            print_info "  $line"
        done
        print_pass "Dependencies resolved successfully"
        return 0
    else
        print_fail "Could not verify dependency resolution"
        return 1
    fi
}

###############################################################################
# Static Analysis (Optional - requires tools)
###############################################################################

test_static_analysis() {
    print_test "Running static analysis (if available)..."

    # Check for cppcheck
    if command -v cppcheck &> /dev/null; then
        print_info "Running cppcheck on src/ ..."
        if cppcheck --enable=warning,style --suppress=missingIncludeSystem \
            --quiet src/ 2>&1 | tee /tmp/cppcheck_output.txt | grep -q "error:"; then
            print_fail "cppcheck found errors"
            cat /tmp/cppcheck_output.txt
            return 1
        else
            print_pass "cppcheck analysis passed"
            return 0
        fi
    else
        print_info "cppcheck not installed - skipping static analysis"
        print_info "Install with: brew install cppcheck (macOS) or apt-get install cppcheck (Linux)"
        return 0  # Not a failure, just skipped
    fi
}

###############################################################################
# Main Test Execution
###############################################################################

main() {
    print_header

    # Change to project directory if not already there
    if [ ! -f "platformio.ini" ]; then
        echo -e "${RED}Error: Must run from project root directory${NC}"
        echo "Current directory: $(pwd)"
        exit 1
    fi

    # Run tests in sequence
    test_pio_installed || exit 1
    test_project_structure || exit 1
    test_clean_build || exit 1
    test_firmware_exists || exit 1
    test_ram_usage || exit 2
    test_flash_usage || exit 2
    test_build_warnings || true  # Warnings don't cause failure
    test_dependency_resolution || true
    test_static_analysis || true  # Optional, doesn't cause failure

    # Print summary
    print_summary
    exit $?
}

# Run main function
main
