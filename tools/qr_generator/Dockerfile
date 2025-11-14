# Dockerfile for M3 Data Logger QR Code Generator API
# Lightweight Python FastAPI service for generating QR codes
#
# Build: docker build -t qr-generator-api .
# Run:   docker run -p 8000:8000 qr-generator-api

FROM python:3.11-slim

# Set working directory
WORKDIR /app

# Install system dependencies for Pillow (image processing)
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    libjpeg-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy requirements first for layer caching
COPY requirements.txt .

# Install Python dependencies
RUN pip install --no-cache-dir -r requirements.txt

# Copy application code
COPY api.py .
COPY generate_qr.py .

# Expose FastAPI default port
EXPOSE 8000

# Set environment variables
ENV PYTHONUNBUFFERED=1

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD python -c "import urllib.request; urllib.request.urlopen('http://localhost:8000/health')"

# Run FastAPI with uvicorn
CMD ["uvicorn", "api:app", "--host", "0.0.0.0", "--port", "8000"]
