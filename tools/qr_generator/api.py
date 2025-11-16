#!/usr/bin/env python3
"""
FastAPI REST API for M3 Data Logger QR Code Generator

Provides a REST API endpoint to generate QR codes with test metadata.
Part of M3L-66/M3L-67 expansion.

API Endpoints:
    POST /generate - Generate QR code image from metadata

Usage:
    uvicorn api:app --host 0.0.0.0 --port 8000

    Or with Docker:
    docker build -t qr-generator-api .
    docker run -p 8000:8000 qr-generator-api
"""

import io
import json
from typing import List, Optional

from fastapi import FastAPI, HTTPException, Response
from fastapi.responses import StreamingResponse
from pydantic import BaseModel, Field, field_validator
import qrcode

# Import validation and generation logic from existing CLI tool
from generate_qr import (
    generate_short_uuid,
    validate_test_id,
    validate_description,
    validate_labels,
    validate_wifi_ssid,
    validate_wifi_password,
    validate_mqtt_host,
    validate_mqtt_port
)


# Pydantic models for request/response validation
class QRGenerateRequest(BaseModel):
    """Request model for QR code generation."""

    description: str = Field(
        ...,
        min_length=1,
        max_length=64,
        description="Human-readable test description (1-64 characters)",
        examples=["walking_outdoor"]
    )

    labels: List[str] = Field(
        ...,
        min_length=1,
        max_length=10,
        description="List of test labels (1-10 labels, each 1-32 characters)",
        examples=[["walking", "outdoor"]]
    )

    test_id: Optional[str] = Field(
        None,
        description="Optional 8-character alphanumeric test ID (auto-generated if not provided)",
        examples=["A3F9K2M7"]
    )

    @field_validator('labels')
    @classmethod
    def validate_labels_list(cls, v):
        """Validate each label in the list."""
        valid, error = validate_labels(v)
        if not valid:
            raise ValueError(error)
        return v


class ConfigQRRequest(BaseModel):
    """Request model for device configuration QR code generation."""

    wifi_ssid: str = Field(
        ...,
        min_length=1,
        max_length=32,
        description="WiFi network SSID (1-32 characters, alphanumeric + underscore/hyphen)",
        examples=["MyNetwork"]
    )

    wifi_password: str = Field(
        ...,
        min_length=8,
        max_length=64,
        description="WiFi password (min 8 characters for WPA2)",
        examples=["SecurePassword123"]
    )

    mqtt_host: str = Field(
        ...,
        description="MQTT broker host (DNS name or IPv4 address)",
        examples=["mqtt.example.com", "192.168.1.100"]
    )

    mqtt_port: int = Field(
        default=1883,
        ge=1,
        le=65535,
        description="MQTT broker port (1-65535, default 1883)",
        examples=[1883]
    )

    mqtt_username: Optional[str] = Field(
        default="",
        description="MQTT username (optional)",
        examples=["device123"]
    )

    mqtt_password: Optional[str] = Field(
        default="",
        description="MQTT password (optional)",
        examples=["secret"]
    )

    device_id: str = Field(
        ...,
        description="Device identifier",
        examples=["m3logger_001"]
    )

    @field_validator('wifi_ssid')
    @classmethod
    def validate_ssid(cls, v):
        """Validate WiFi SSID format."""
        valid, error = validate_wifi_ssid(v)
        if not valid:
            raise ValueError(error)
        return v

    @field_validator('wifi_password')
    @classmethod
    def validate_password(cls, v):
        """Validate WiFi password strength."""
        valid, error = validate_wifi_password(v)
        if not valid:
            raise ValueError(error)
        return v

    @field_validator('mqtt_host')
    @classmethod
    def validate_host(cls, v):
        """Validate MQTT broker host format."""
        valid, error = validate_mqtt_host(v)
        if not valid:
            raise ValueError(error)
        return v


# Initialize FastAPI app
app = FastAPI(
    title="M3 Data Logger QR Code Generator API",
    description="REST API for generating QR codes with test metadata for M3 Data Logger",
    version="1.0.0"
)


@app.get("/")
async def root():
    """
    Root endpoint providing API information.

    Returns:
        JSON object with API metadata
    """
    return {
        "service": "M3 Data Logger QR Code Generator API",
        "version": "1.0.0",
        "endpoints": {
            "POST /generate": "Generate QR code from test metadata",
            "POST /generate/config": "Generate QR code from device configuration",
            "GET /health": "Health check endpoint"
        }
    }


@app.get("/health")
async def health_check():
    """
    Health check endpoint for monitoring.

    Returns:
        JSON object with service status
    """
    return {"status": "healthy"}


@app.post("/generate", response_class=StreamingResponse)
async def generate_qr(request: QRGenerateRequest):
    """
    Generate QR code image from test metadata.

    Accepts JSON payload with test description, labels, and optional test_id.
    Returns PNG image of the generated QR code.

    Args:
        request: QRGenerateRequest object with metadata

    Returns:
        StreamingResponse containing PNG image bytes

    Raises:
        HTTPException: If validation fails or QR generation errors occur

    Example:
        ```bash
        curl -X POST http://localhost:8000/generate \\
          -H "Content-Type: application/json" \\
          -d '{"description": "walking_outdoor", "labels": ["walking", "outdoor"]}' \\
          --output qr_code.png
        ```
    """
    try:
        # Generate or validate test_id
        if request.test_id:
            valid, error = validate_test_id(request.test_id)
            if not valid:
                raise HTTPException(status_code=400, detail=f"Invalid test_id: {error}")
            test_id = request.test_id
        else:
            test_id = generate_short_uuid()

        # Validate description (Pydantic handles min/max length, but we use custom validator)
        valid, error = validate_description(request.description)
        if not valid:
            raise HTTPException(status_code=400, detail=f"Invalid description: {error}")

        # Build metadata JSON
        metadata = {
            "test_id": test_id,
            "description": request.description,
            "labels": request.labels
        }

        json_str = json.dumps(metadata, separators=(',', ':'))  # Compact JSON

        # Generate QR code
        qr = qrcode.QRCode(
            version=1,  # Auto-fit
            error_correction=qrcode.constants.ERROR_CORRECT_L,
            box_size=10,
            border=4,
        )
        qr.add_data(json_str)
        qr.make(fit=True)

        # Create PNG image in memory
        img = qr.make_image(fill_color="black", back_color="white")

        # Save to BytesIO buffer
        img_buffer = io.BytesIO()
        img.save(img_buffer, format='PNG')
        img_buffer.seek(0)

        # Return image as streaming response with custom header
        headers = {
            "X-Test-ID": test_id,
            "Content-Disposition": f"inline; filename=qr_{test_id}.png"
        }

        return StreamingResponse(
            img_buffer,
            media_type="image/png",
            headers=headers
        )

    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"QR generation failed: {str(e)}")


@app.post("/generate/config", response_class=StreamingResponse)
async def generate_config_qr(request: ConfigQRRequest):
    """
    Generate device configuration QR code.

    Accepts JSON payload with WiFi credentials and MQTT broker settings.
    Returns PNG image of the generated QR code.

    Args:
        request: ConfigQRRequest object with device configuration

    Returns:
        StreamingResponse containing PNG image bytes

    Raises:
        HTTPException: If validation fails or QR generation errors occur

    Example:
        ```bash
        curl -X POST http://localhost:8000/generate/config \\
          -H "Content-Type: application/json" \\
          -d '{
            "wifi_ssid": "MyNetwork",
            "wifi_password": "SecurePassword123",
            "mqtt_host": "mqtt.example.com",
            "mqtt_port": 1883,
            "mqtt_username": "device123",
            "mqtt_password": "secret",
            "device_id": "m3logger_001"
          }' \\
          --output config_qr.png
        ```
    """
    try:
        # Build configuration JSON
        config_data = {
            "type": "device_config",
            "version": "1.0",
            "wifi": {
                "ssid": request.wifi_ssid,
                "password": request.wifi_password
            },
            "mqtt": {
                "host": request.mqtt_host,
                "port": request.mqtt_port,
                "username": request.mqtt_username,
                "password": request.mqtt_password,
                "device_id": request.device_id
            }
        }

        json_str = json.dumps(config_data, separators=(',', ':'))  # Compact JSON

        # Check size (Tiny Code Reader limit: 256 bytes, use 220 safe limit)
        if len(json_str) > 220:
            raise HTTPException(
                status_code=400,
                detail=f"Config JSON too large ({len(json_str)} bytes, max 220)"
            )

        # Generate QR code
        qr = qrcode.QRCode(
            version=1,  # Auto-fit
            error_correction=qrcode.constants.ERROR_CORRECT_L,
            box_size=10,
            border=4,
        )
        qr.add_data(json_str)
        qr.make(fit=True)

        # Create PNG image in memory
        img = qr.make_image(fill_color="black", back_color="white")

        # Save to BytesIO buffer
        img_buffer = io.BytesIO()
        img.save(img_buffer, format='PNG')
        img_buffer.seek(0)

        # Return image as streaming response with custom header
        headers = {
            "X-Device-ID": request.device_id,
            "Content-Disposition": f"inline; filename=config_{request.device_id}.png"
        }

        return StreamingResponse(
            img_buffer,
            media_type="image/png",
            headers=headers
        )

    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Config QR generation failed: {str(e)}")


# TODO: Future S3 integration endpoint (M3L-67)
#
# @app.post("/generate-and-upload")
# async def generate_and_upload_qr(request: QRGenerateRequest):
#     """
#     Generate QR code and upload to S3 bucket.
#
#     Returns:
#         JSON object with S3 URL and metadata
#     """
#     # Implementation deferred to NEXT phase
#     # Will use boto3 to upload generated QR to S3
#     # Return: {"test_id": "...", "qr_url": "s3://...", "metadata": {...}}
#     raise HTTPException(status_code=501, detail="S3 upload not yet implemented")


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
