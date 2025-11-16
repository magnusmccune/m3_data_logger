# QR Code Generator for M3 Data Logger

Generates QR codes for the M3 Data Logger:
- **Metadata QR codes**: Test metadata (test ID, description, labels)
- **Configuration QR codes**: Device configuration (WiFi + MQTT broker settings)

Available as both a **CLI tool** and a **REST API service**.

## Installation

### CLI Tool

```bash
cd tools/qr_generator
pip install -r requirements.txt
```

### REST API (Docker)

```bash
cd tools/qr_generator
docker-compose up
```

Or build manually:

```bash
docker build -t qr-generator-api .
docker run -p 8000:8000 qr-generator-api
```

## Usage

### CLI Tool

The CLI tool supports two modes: `metadata` and `config`.

#### Metadata Mode

Generate QR codes with test metadata.

**Auto-generated test ID (default):**
```bash
python generate_qr.py --mode metadata --description "walking_outdoor" --labels walking outdoor
```

The test ID is automatically generated using a ShortUUID format (8 alphanumeric characters, excluding ambiguous characters).

**Specific test ID:**
```bash
python generate_qr.py --mode metadata --test-id A3F9K2M7 --description "running_indoor" --labels running indoor
```

**Save to file:**
```bash
python generate_qr.py --mode metadata --description "test1" --labels demo --output test1_qr.png
```

**Batch generate:**
```bash
# Generate 5 test QR codes
for i in {1..5}; do
  python generate_qr.py --mode metadata --description "test$i" --labels demo --output "test${i}_qr.png" --no-show
done
```

#### Config Mode

Generate QR codes with device configuration (WiFi credentials + MQTT broker settings).

**Basic config QR:**
```bash
python generate_qr.py --mode config \
  --wifi-ssid "MyNetwork" \
  --wifi-password "SecurePassword123" \
  --mqtt-host "mqtt.example.com" \
  --device-id "m3logger_001"
```

**Full config with MQTT credentials:**
```bash
python generate_qr.py --mode config \
  --wifi-ssid "MyNetwork" \
  --wifi-password "SecurePassword123" \
  --mqtt-host "mqtt.example.com" \
  --mqtt-port 1883 \
  --mqtt-username "device123" \
  --mqtt-password "secret" \
  --device-id "m3logger_001" \
  --output config_m3logger_001.png
```

**Using IP address:**
```bash
python generate_qr.py --mode config \
  --wifi-ssid "TestNet" \
  --wifi-password "password123" \
  --mqtt-host "192.168.1.100" \
  --device-id "m3logger_test" \
  --output config_qr.png
```

### REST API

The API service provides HTTP endpoints for programmatic QR code generation.

#### Start the API Server

Using Docker Compose (recommended):

```bash
cd tools/qr_generator
docker-compose up
```

Using Docker directly:

```bash
docker build -t qr-generator-api .
docker run -p 8000:8000 qr-generator-api
```

Or run locally with Python:

```bash
pip install -r requirements.txt
python api.py
# Or: uvicorn api:app --host 0.0.0.0 --port 8000
```

#### API Endpoints

##### GET /

Get API information and available endpoints.

```bash
curl http://localhost:8000/
```

##### GET /health

Health check endpoint for monitoring.

```bash
curl http://localhost:8000/health
```

Response:
```json
{"status": "healthy"}
```

##### POST /generate

Generate QR code image from test metadata. Returns PNG image directly.

**Request Body:**
```json
{
  "description": "walking_outdoor",
  "labels": ["walking", "outdoor"],
  "test_id": "A3F9K2M7"  // optional, auto-generated if not provided
}
```

**Response:**
- Content-Type: `image/png`
- Header: `X-Test-ID: <generated_or_provided_id>`
- Body: PNG image bytes

**Examples:**

Generate with auto-generated test ID and save to file:

```bash
curl -X POST http://localhost:8000/generate \
  -H "Content-Type: application/json" \
  -d '{"description": "walking_outdoor", "labels": ["walking", "outdoor"]}' \
  --output qr_code.png
```

Generate with specific test ID:

```bash
curl -X POST http://localhost:8000/generate \
  -H "Content-Type: application/json" \
  -d '{"description": "running_indoor", "labels": ["running", "indoor"], "test_id": "CUSTOM99"}' \
  --output qr_custom.png
```

Retrieve test ID from response headers:

```bash
curl -X POST http://localhost:8000/generate \
  -H "Content-Type: application/json" \
  -d '{"description": "test", "labels": ["demo"]}' \
  -I -o /dev/null -s -w "Test ID: %header{X-Test-ID}\n"
```

Python client example:

```python
import requests

response = requests.post(
    "http://localhost:8000/generate",
    json={
        "description": "walking_outdoor",
        "labels": ["walking", "outdoor"]
    }
)

# Get test ID from header
test_id = response.headers.get("X-Test-ID")
print(f"Generated test ID: {test_id}")

# Save QR code image
with open(f"qr_{test_id}.png", "wb") as f:
    f.write(response.content)
```

##### POST /generate/config

Generate device configuration QR code. Returns PNG image directly.

**Request Body:**
```json
{
  "wifi_ssid": "MyNetwork",
  "wifi_password": "SecurePassword123",
  "mqtt_host": "mqtt.example.com",
  "mqtt_port": 1883,
  "mqtt_username": "device123",
  "mqtt_password": "secret",
  "device_id": "m3logger_001"
}
```

**Required Fields:**
- `wifi_ssid`: WiFi SSID (1-32 chars, alphanumeric + underscore/hyphen)
- `wifi_password`: WiFi password (min 8 chars for WPA2)
- `mqtt_host`: MQTT broker host (DNS name or IPv4 address)
- `device_id`: Device identifier

**Optional Fields:**
- `mqtt_port`: MQTT port (1-65535, default: 1883)
- `mqtt_username`: MQTT username (default: "")
- `mqtt_password`: MQTT password (default: "")

**Response:**
- Content-Type: `image/png`
- Header: `X-Device-ID: <device_id>`
- Body: PNG image bytes

**Examples:**

Basic config QR:

```bash
curl -X POST http://localhost:8000/generate/config \
  -H "Content-Type: application/json" \
  -d '{
    "wifi_ssid": "MyNetwork",
    "wifi_password": "SecurePassword123",
    "mqtt_host": "mqtt.example.com",
    "device_id": "m3logger_001"
  }' \
  --output config_qr.png
```

Full config with MQTT credentials:

```bash
curl -X POST http://localhost:8000/generate/config \
  -H "Content-Type: application/json" \
  -d '{
    "wifi_ssid": "MyNetwork",
    "wifi_password": "SecurePassword123",
    "mqtt_host": "mqtt.example.com",
    "mqtt_port": 1883,
    "mqtt_username": "device123",
    "mqtt_password": "secret",
    "device_id": "m3logger_001"
  }' \
  --output config_qr.png
```

Using IP address:

```bash
curl -X POST http://localhost:8000/generate/config \
  -H "Content-Type: application/json" \
  -d '{
    "wifi_ssid": "TestNet",
    "wifi_password": "password123",
    "mqtt_host": "192.168.1.100",
    "device_id": "m3logger_test"
  }' \
  --output config_test.png
```

Python client example:

```python
import requests

response = requests.post(
    "http://localhost:8000/generate/config",
    json={
        "wifi_ssid": "MyNetwork",
        "wifi_password": "SecurePassword123",
        "mqtt_host": "mqtt.example.com",
        "mqtt_port": 1883,
        "mqtt_username": "device123",
        "mqtt_password": "secret",
        "device_id": "m3logger_001"
    }
)

# Get device ID from header
device_id = response.headers.get("X-Device-ID")
print(f"Config for device: {device_id}")

# Save config QR code
with open(f"config_{device_id}.png", "wb") as f:
    f.write(response.content)
```

#### API Documentation

Interactive API docs available at:
- Swagger UI: http://localhost:8000/docs
- ReDoc: http://localhost:8000/redoc

## QR Formats

### Metadata QR Format

Test metadata QR codes contain JSON with test information:

```json
{
  "test_id": "A3F9K2M7",
  "description": "walking_outdoor",
  "labels": ["walking", "outdoor"]
}
```

**Field Requirements:**
- **test_id**: Exactly 8 alphanumeric characters (e.g., `A3F9K2M7`)
  - Auto-generated using shortUUID style (excludes ambiguous chars: 0, O, 1, I, l)
- **description**: 1-64 characters, human-readable test description
- **labels**: 1-10 labels, each 1-32 characters

### Configuration QR Format

Device configuration QR codes contain JSON with WiFi and MQTT settings:

```json
{
  "type": "device_config",
  "version": "1.0",
  "wifi": {
    "ssid": "MyNetwork",
    "password": "SecurePassword123"
  },
  "mqtt": {
    "host": "mqtt.example.com",
    "port": 1883,
    "username": "device123",
    "password": "secret",
    "device_id": "m3logger_001"
  }
}
```

**Field Requirements:**
- **type**: Always "device_config" (identifies QR as configuration)
- **version**: Config schema version (currently "1.0")
- **wifi.ssid**: WiFi SSID (1-32 chars, printable ASCII including spaces)
- **wifi.password**: WiFi password (8-16 chars, printable ASCII)
- **mqtt.host**: MQTT broker host (DNS name or IPv4 address, max 64 chars)
- **mqtt.port**: MQTT broker port (1-65535, default: 1883)
- **mqtt.username**: MQTT username (optional, max 16 chars)
- **mqtt.password**: MQTT password (optional, max 16 chars)
- **mqtt.device_id**: Device identifier (1-16 chars)

### Configuration Field Limits

The Tiny Code Reader has a 256-byte hardware limit. Config QR codes use a 220-byte safe limit to account for QR encoding overhead. Field lengths are optimized to maximize MQTT hostname space:

| Field | Max Length | Notes |
|-------|------------|-------|
| WiFi SSID | 32 chars | IEEE 802.11 maximum, printable ASCII (spaces allowed) |
| WiFi Password | 16 chars | WPA2 compliant, reduced from 64 for QR size |
| MQTT Host | 64 chars | Generous limit for long hostnames/IPs |
| MQTT Username | 16 chars | Optional field, can be empty |
| MQTT Password | 16 chars | Optional field, can be empty |
| Device ID | 16 chars | Unique device identifier |

**Example realistic config** (217 bytes, fits in 220-byte limit):
```json
{
  "type":"device_config",
  "version":"1.0",
  "wifi":{"ssid":"OfficeWiFi","password":"SecPass123"},
  "mqtt":{"host":"a3k9m2f7-ats.iot.us-west-2.amazonaws.com","port":1883,"username":"","password":"","device_id":"m3logger01"}
}
```

Note: While individual fields support the max lengths shown above, not all fields can be at maximum simultaneously due to the 220-byte total limit. The configuration is optimized for typical IoT use cases like AWS IoT endpoints.

## Example Output

```
Generated test_id: A3F9K2M7

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
- API service is stateless (no database) - QR codes are generated on-demand
- Future: S3 upload endpoint planned for NEXT phase (M3L-67)

## Architecture

```
tools/qr_generator/
├── generate_qr.py       # CLI tool (original implementation)
├── api.py               # FastAPI REST API service
├── requirements.txt     # Python dependencies (CLI + API)
├── Dockerfile           # Container image for API service
├── docker-compose.yml   # Local development orchestration
└── README.md            # This file
```

**Code Reuse:**
The API (`api.py`) imports validation and generation functions from the CLI tool (`generate_qr.py`) to maintain single source of truth for business logic.

**Docker Image:**
- Base: `python:3.11-slim` (lightweight)
- Dependencies: FastAPI, uvicorn, qrcode, pillow
- Port: 8000
- Health check: `/health` endpoint
- Size: ~200MB
