# Postman Testing Guide - QR Generator API

This guide shows you how to test the QR Generator API using Postman.

## Quick Start

### Option 1: Run with Docker (Recommended)

```bash
cd tools/qr_generator
docker-compose up
```

API will be available at: `http://localhost:8000`

### Option 2: Run with Python (Without Docker)

```bash
cd tools/qr_generator
source venv/bin/activate
pip install -r requirements.txt
uvicorn api:app --reload
```

API will be available at: `http://localhost:8000`

---

## Postman Collection Setup

### Import Collection (One-Click Setup)

1. Download this collection: [QR_Generator_API.postman_collection.json](./QR_Generator_API.postman_collection.json)
2. Open Postman → Click **Import** → Select the file
3. Collection appears in left sidebar with 3 pre-configured requests

### Manual Setup (Alternative)

If you prefer to set it up manually:

#### 1. Create New Collection

- Click **New** → **Collection**
- Name: `QR Generator API`
- Base URL: `http://localhost:8000`

#### 2. Add Environment Variables

- Click **Environments** → **+** (Create new)
- Name: `Local Development`
- Add variables:
  - `base_url` = `http://localhost:8000`
  - `port` = `8000`

---

## API Endpoints

### 1. Health Check

**Request:**
- Method: `GET`
- URL: `{{base_url}}/health`

**Expected Response:**
```json
{
  "status": "healthy"
}
```

**Status Code:** `200 OK`

---

### 2. Service Info

**Request:**
- Method: `GET`
- URL: `{{base_url}}/`

**Expected Response:**
```json
{
  "service": "QR Code Generator API",
  "version": "1.0.0",
  "endpoints": {
    "generate": "POST /generate",
    "health": "GET /health"
  }
}
```

**Status Code:** `200 OK`

---

### 3. Generate QR Code (Auto-Generated Test ID)

**Request:**
- Method: `POST`
- URL: `{{base_url}}/generate`
- Headers:
  - `Content-Type`: `application/json`
- Body (raw JSON):
```json
{
  "description": "walking_outdoor",
  "labels": ["walking", "outdoor", "test"]
}
```

**Expected Response:**
- Content-Type: `image/png`
- Headers:
  - `X-Test-ID`: `<8-char auto-generated ID>` (e.g., `A3F9K2M7`)
- Body: PNG image bytes (binary data)

**Status Code:** `200 OK`

**Postman Tips:**
- Click **Send and Download** to save the QR code image
- Check the **Headers** tab in response to see `X-Test-ID`
- Use **Visualize** tab to preview the image (Postman may display it)

---

### 4. Generate QR Code (Custom Test ID)

**Request:**
- Method: `POST`
- URL: `{{base_url}}/generate`
- Headers:
  - `Content-Type`: `application/json`
- Body (raw JSON):
```json
{
  "description": "running_indoor",
  "labels": ["running", "indoor"],
  "test_id": "CUSTOM99"
}
```

**Expected Response:**
- Content-Type: `image/png`
- Headers:
  - `X-Test-ID`: `CUSTOM99` (your custom ID)
- Body: PNG image bytes

**Status Code:** `200 OK`

---

## Error Cases to Test

### Invalid Test ID (Too Short)

**Request:**
```json
{
  "description": "test",
  "labels": ["demo"],
  "test_id": "SHORT"
}
```

**Expected Response:**
```json
{
  "detail": "test_id must be exactly 8 characters (got 5)"
}
```

**Status Code:** `400 Bad Request`

---

### Invalid Test ID (Special Characters)

**Request:**
```json
{
  "description": "test",
  "labels": ["demo"],
  "test_id": "TEST@123"
}
```

**Expected Response:**
```json
{
  "detail": "test_id must contain only alphanumeric characters (A-Z, 0-9)"
}
```

**Status Code:** `400 Bad Request`

---

### Missing Description

**Request:**
```json
{
  "labels": ["demo"]
}
```

**Expected Response:**
```json
{
  "detail": [
    {
      "type": "missing",
      "loc": ["body", "description"],
      "msg": "Field required"
    }
  ]
}
```

**Status Code:** `422 Unprocessable Entity`

---

### Description Too Long

**Request:**
```json
{
  "description": "This is a very long description that exceeds the maximum allowed length of 64 characters for QR code metadata",
  "labels": ["demo"]
}
```

**Expected Response:**
```json
{
  "detail": "description must be 64 characters or less (got 120)"
}
```

**Status Code:** `400 Bad Request`

---

### Too Many Labels

**Request:**
```json
{
  "description": "test",
  "labels": ["label1", "label2", "label3", "label4", "label5", "label6", "label7", "label8", "label9", "label10", "label11"]
}
```

**Expected Response:**
```json
{
  "detail": "labels must contain 10 items or less (got 11)"
}
```

**Status Code:** `400 Bad Request`

---

## Advanced Testing

### Test Workflow: Generate → Scan

1. **Generate QR Code** in Postman
2. **Save the response** image (Send and Download)
3. **Scan with phone** or QR scanner app
4. **Verify JSON content** matches request:
   ```json
   {
     "test_id": "A3F9K2M7",
     "description": "walking_outdoor",
     "labels": ["walking", "outdoor"]
   }
   ```

---

### Batch Testing with Collection Runner

1. Create a CSV file with test data:
   ```csv
   description,labels
   walking_outdoor,"walking,outdoor"
   running_indoor,"running,indoor"
   sitting_still,"sitting,stationary"
   ```

2. In Postman:
   - Select Collection → Click **Run**
   - Upload CSV
   - Map columns to request body
   - Run all requests

---

## Troubleshooting

### Docker Container Not Running

**Symptom:** Connection refused errors

**Solution:**
```bash
# Check if container is running
docker ps

# If not, start it
cd tools/qr_generator
docker-compose up -d

# Check logs
docker-compose logs -f
```

---

### Port 8000 Already in Use

**Symptom:** `Address already in use` error

**Solution:**
```bash
# Find process using port 8000
lsof -i :8000

# Kill the process (replace PID)
kill -9 <PID>

# Or change the port in docker-compose.yml
ports:
  - "8001:8000"  # Use port 8001 instead
```

Then update Postman environment: `base_url` = `http://localhost:8001`

---

### API Returns 500 Internal Server Error

**Symptom:** Unexpected server errors

**Check Logs:**
```bash
# Docker
docker-compose logs -f

# Python (direct)
# Error will be in terminal where uvicorn is running
```

**Common Causes:**
- Missing dependencies (run `pip install -r requirements.txt`)
- Python import errors
- QR code generation library issues

---

## Interactive API Documentation

FastAPI provides built-in interactive docs:

1. **Swagger UI:** `http://localhost:8000/docs`
   - Try out endpoints directly in browser
   - See request/response schemas
   - Download generated images

2. **ReDoc:** `http://localhost:8000/redoc`
   - Alternative documentation UI
   - Better for reading API specs

These are excellent alternatives to Postman for quick testing!

---

## Next Steps

After testing with Postman:
1. Export your Postman collection (share with team)
2. Write automated tests (see `tests/` directory)
3. Deploy to production (see deployment guide)

---

## Need Help?

- API Documentation: `http://localhost:8000/docs`
- README: `./README.md`
- Issues: Create a Linear ticket with project "M3-Data-Logger"
