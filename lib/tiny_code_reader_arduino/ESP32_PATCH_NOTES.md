# ESP32 I2C Chunking Patch

## Problem

The original Tiny Code Reader library was designed for Arduino Uno, which has a 32-byte Wire buffer limitation. The library attempted to work around this by reading the 256-byte data structure in 64-byte chunks.

However, on ESP32, each `Wire.requestFrom()` call starts a NEW I2C transaction that resets the device's read pointer to the beginning. This caused the same 64 bytes to be read 4 times instead of sequential 256-byte data, resulting in corrupted QR code content.

## Evidence

Serial output showing corruption:
```
Raw JSON (110 bytes): {"test_id":"HM4TS6AZ","description":"walking_outdoor","labels"n␀{"test_id":"HM4TS6AZ","description":"walking_o
Warning: Non-printable character at position 63 (byte value: 0)
```

Analysis:
- Byte 63: null byte (MSB of duplicated length field from 2nd chunk)
- Bytes 0-61: correct JSON data
- Bytes 62-63: duplicate of length field (bytes 0-1)
- Bytes 64+: duplicate of JSON start

## Solution

Modified `tiny_code_reader.h` to use platform-specific chunk sizes:
- ESP32/ESP8266: 256 bytes (full read in single transaction)
- Other platforms: 32 bytes (Arduino Uno compatibility)

## Changes

File: `tiny_code_reader.h` (lines 52-56)
```cpp
#if defined(ESP32) || defined(ESP8266)
    const int maxBytesPerChunk = 256;  // ESP32 supports full read
#else
    const int maxBytesPerChunk = 32;   // Arduino Uno buffer limit
#endif
```

## Testing

After applying this patch:
1. Clean build: `pio run --target clean`
2. Upload: `pio run --target upload`
3. Monitor serial output at 115200 baud
4. Scan QR code and verify complete JSON without null bytes or duplication

Expected output:
```
Raw JSON (110 bytes): {"test_id":"HM4TS6AZ","description":"walking_outdoor","labels":["walking","outdoor"]}
```

## Date

2025-11-15

## Contact

M3 Data Logger Project
