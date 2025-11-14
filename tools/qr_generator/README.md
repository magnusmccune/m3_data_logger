# QR Code Generator for M3 Data Logger

Generates QR codes with test metadata for scanning by the M3 Data Logger.

## Installation

```bash
pip install qrcode[pil]
```

## Usage

### Generate QR with Random Test ID

```bash
python generate_qr.py --random --description "walking_outdoor" --labels walking outdoor
```

### Generate QR with Specific Test ID

```bash
python generate_qr.py --test-id A3F9K2M7 --description "running_indoor" --labels running indoor
```

### Save QR to File

```bash
python generate_qr.py --random --description "test1" --labels demo --output test1_qr.png
```

### Batch Generate QR Codes

```bash
# Generate 5 test QR codes
for i in {1..5}; do
  python generate_qr.py --random --description "test$i" --labels demo --output "test${i}_qr.png" --no-show
done
```

## QR Format

The generated QR codes contain JSON metadata:

```json
{
  "test_id": "A3F9K2M7",
  "description": "walking_outdoor",
  "labels": ["walking", "outdoor"]
}
```

### Field Requirements

- **test_id**: Exactly 8 alphanumeric characters (e.g., `A3F9K2M7`)
  - Auto-generated using shortUUID style (excludes ambiguous chars: 0, O, 1, I, l)
- **description**: 1-64 characters, human-readable test description
- **labels**: 1-10 labels, each 1-32 characters

## Example Output

```
Generated random test_id: A3F9K2M7

QR Code (scan with M3 Data Logger):
█████████████████████████████████
█████████████████████████████████
████ ▄▄▄▄▄ █▀█ █▄▄▀▄█ ▄▄▄▄▄ ████
████ █   █ █▀▀▀█ ▀ ▀█ █   █ ████
████ █▄▄▄█ █▀ █▀▀█ ██ █▄▄▄█ ████
████▄▄▄▄▄▄▄█▄▀ ▀▄█ █▄▄▄▄▄▄▄████
████ ▄ ▄  ▄▀▄█▀ ▄█ █ ▄▀▀▀▄▀████
████▄ ▄▀ ▀▄▀█  █▀▀▀▀▄█▀█▄▀█████
████ █▀▄█▄▄█▀█ ▀▄█▀▀  ▄▀▀▀ ████
████▄███▄█▄█▀▀▀▀ █  ▄▄▄  ▀▀████
████ ▄▄▄▄▄ ███ ▄ ▀▀█ ▄ ▀  █████
████ █   █ █  ▀██▀▀▀▄▄▄█▄▀█████
████ █▄▄▄█ █▀▀▀ █▀▀▀ ▀█▀▄▀▀████
████▄▄▄▄▄▄▄█▄▄█▄█▄█▄▄▄██▄█▄████
█████████████████████████████████
█████████████████████████████████

Metadata JSON (77 bytes):
{
  "test_id": "A3F9K2M7",
  "description": "walking_outdoor",
  "labels": [
    "walking",
    "outdoor"
  ]
}

✓ QR code saved to: test1_qr.png
```

## Notes

- QR codes are optimized for size (compact JSON, error correction level L)
- Test IDs exclude ambiguous characters for easier manual entry if needed
- Generated QR codes can be printed or displayed on a phone screen for scanning
