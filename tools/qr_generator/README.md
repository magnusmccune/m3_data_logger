# QR Code Generator for M3 Data Logger

Generates QR codes with test metadata for scanning by the M3 Data Logger.

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

#### Generate QR with Auto-Generated Test ID (Default)

```bash
python generate_qr.py --description "walking_outdoor" --labels walking outdoor
```

The test ID is automatically generated using a ShortUUID format (8 alphanumeric characters, excluding ambiguous characters).

#### Generate QR with Specific Test ID (Override)

```bash
python generate_qr.py --test-id A3F9K2M7 --description "running_indoor" --labels running indoor
```

#### Save QR to File

```bash
python generate_qr.py --description "test1" --labels demo --output test1_qr.png
```

#### Batch Generate QR Codes

```bash
# Generate 5 test QR codes
for i in {1..5}; do
  python generate_qr.py --description "test$i" --labels demo --output "test${i}_qr.png" --no-show
done
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

Generate QR code image from metadata. Returns PNG image directly.

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

#### API Documentation

Interactive API docs available at:
- Swagger UI: http://localhost:8000/docs
- ReDoc: http://localhost:8000/redoc

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
