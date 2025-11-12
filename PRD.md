# M3 Data Logger - Product Requirements Document

## Context & Why Now

The need for reliable, flexible IoT data collection devices for generating time-series datasets has become critical for machine learning model training, system analysis, and experimental validation. Current solutions are either too complex (requiring custom PCB design) or too limited (lacking proper data framing and metadata capture). The M3 Data Logger leverages SparkFun's ecosystem to provide a turnkey solution for researchers, developers, and field technicians who need to capture sensor data with proper context and metadata.

**Market timing**: IoT adoption is accelerating, with edge ML requiring quality training datasets. The availability of mature Qwiic ecosystem components makes this the right time to build.

## Users & Jobs-to-be-Done (JTBD)

### Primary Users

**1. Research Engineers**
- JTBD: Capture high-quality IMU data for motion analysis and algorithm development
- Pain: Manual data annotation, lack of synchronized metadata
- Gain: Automatic data framing with QR-encoded metadata

**2. Field Technicians**
- JTBD: Collect sensor data from equipment/environments for diagnostics
- Pain: Complex setup procedures, no visual feedback during recording
- Gain: Simple button workflow with LED status indication

**3. ML/Data Scientists**
- JTBD: Generate labeled training datasets for model development
- Pain: Inconsistent data formatting, missing context
- Gain: Structured data with embedded labels and test metadata

## Business Goals & Success Metrics

### Leading Indicators
- Device activation rate: >80% of devices activated within 7 days
- Recording success rate: >95% of initiated recordings complete successfully
- Metadata capture rate: >90% of recordings include QR metadata
- Average recordings per device per week: >10

### Lagging Indicators
- Dataset quality score: >4.5/5 from users
- Time-to-first-dataset: <30 minutes from unboxing
- Device reliability: <2% failure rate in first 6 months
- Platform adoption: 100+ active devices in 12 months

## Functional Requirements

### NOW (MVP/Current Scope)

**FR1. Button-Triggered Recording Control**
- Single button press initiates recording sequence
- Acceptance: Button press detected within 100ms, LED begins blinking within 200ms

**FR2. QR Code Metadata Capture**
- Scan QR codes encoding JSON with `test` (string) and `labels` (array) properties
- Acceptance: QR scan completes in <2s, JSON parsed and validated

**FR3. IMU Data Logging**
- Record 6DoF IMU data (ISM330DHCX) at configurable sample rate
- Acceptance: Data logged at minimum 100Hz, max 1% sample loss

**FR4. MicroSD Storage**
- Write timestamped sensor data to SD card in structured format
- Acceptance: Data written with <10ms latency, file system corruption rate <0.1%

**FR5. LED Status Indication**
- Blinking: Ready for QR scan
- Solid: Recording active
- Off: Idle/recording complete
- Acceptance: State changes visible within 100ms

**FR6. Data Framing**
- Mark recording start/end with timestamps and metadata
- Acceptance: Frame markers accurate to ±10ms, metadata embedded in file header

**FR7. QR Code Generation Tool (Python CLI)**
- Python script to generate QR codes encoding metadata JSON (test name + labels)
- CLI interface for quick QR code creation during testing and field use
- Outputs printable QR code images (PNG) or displays in terminal
- Acceptance: Generate valid QR codes readable by Tiny Code Reader in <1s, support custom test names and multiple labels

### NEXT (Near-term Enhancements)

**FR8. MQTT Data Transmission**
- Stream sensor data to configured MQTT broker in real-time
- Acceptance: <100ms latency, automatic reconnection on network loss

**FR9. Extended Sensor Support**
- GPS module for location tracking
- RFID reader for asset identification
- Environmental & Gas Sensor - BME688
- Acceptance: All sensors sampled synchronously, data aligned by timestamp

**FR10. Network Configuration**
- WiFi credentials and MQTT broker settings stored persistently
- Acceptance: Settings survive power cycle, connection established in <5s

### LATER (Future Vision)

**FR11. QR-Based Configuration**
- Scan QR code to configure WiFi SSID, password, MQTT settings
- Acceptance: Configuration applied without device reset, validated before save

**FR12. NeoPixel Status Display**
- 8-LED strip showing detailed status (battery, storage, network, recording)
- Acceptance: Status updates in real-time, color coding follows standard conventions

**FR13. Configuration QR Code Generation**
- Extend Python QR generation tool to create configuration QR codes
- Support encoding WiFi credentials (SSID, password), MQTT broker settings (host, port, username, password)
- Validation of configuration parameters before encoding (valid SSID format, password strength, reachable MQTT host)
- Acceptance: Config QR codes successfully configure device, validation catches common errors

**FR14. QR Code Generation Web Application**
- Web-based interface for generating both metadata and configuration QR codes
- Form-based input with real-time validation and preview
- Download QR codes as PNG or print directly from browser
- Support bulk generation for multiple test scenarios
- Acceptance: Works on mobile and desktop browsers, responsive design, <2s page load time

**FR15. Cloud Dashboard**
- Web interface for device fleet management and data visualization
- Acceptance: Real-time device status, historical data access, export capabilities

## Non-Functional Requirements

### Performance
- Boot time: <3 seconds to operational state
- Battery life: >8 hours continuous recording on 2000mAh battery
- Storage efficiency: <100KB/minute for IMU-only recording

### Scale & Reliability
- Support 1000+ concurrent devices per MQTT broker
- 99.9% uptime for data recording functionality
- Graceful degradation when network unavailable

### SLOs/SLAs
- Data integrity: 99.99% of recorded samples retrievable
- Latency: Recording starts within 500ms of trigger
- Recovery: Automatic recovery from crashes within 10s

### Privacy & Security
- No PII stored on device
- MQTT connections use TLS 1.3
- SD card data encrypted at rest (LATER phase)
- Device authentication via unique certificates

### Observability
- Device logs accessible via serial console
- Error events transmitted via MQTT telemetry channel
- Storage/battery metrics available in status API

## Technical Architecture

### Hardware Stack
- **MCU**: ESP32 (on DataLogger IoT board)
- **Storage**: MicroSD card (FAT32 filesystem)
- **Sensors**: Qwiic I2C bus architecture
- **Power**: LiPo battery with charging circuit

### Software Components
- **Firmware**: Arduino/C++ on ESP32
- **Data Format**: JSON Lines for structured logging
- **Communication**: MQTT over WiFi (TLS)
- **Configuration**: Persistent storage in SPIFFS
- **Tooling**: Python CLI for QR code generation, future web application

### Data Flow
1. Sensor data → I2C bus → ESP32
2. ESP32 → SD card (primary storage)
3. ESP32 → MQTT broker (when connected)
4. MQTT broker → Cloud storage/processing

## In Scope / Out of Scope

### In Scope
- Hardware integration for listed components
- Core recording and transmission features
- QR code generation tooling (Python CLI and web app)
- Basic data validation and error handling
- Documentation and setup guides

### Out of Scope
- Custom sensor development
- Mobile app development
- Advanced analytics or ML on-device
- Regulatory certifications (FCC, CE)
- Custom enclosure design

## Rollout Plan

### Phase 1: NOW (Weeks 1-4)
- Implement core button/QR/recording workflow
- Test with 5 prototype devices
- **Guardrails**: Manual testing only, no field deployment
- **Kill switch**: Serial command to factory reset

### Phase 2: NEXT (Weeks 5-12)
- Add MQTT transmission and additional sensors
- Deploy to 20 beta users
- **Guardrails**: Rate limiting on MQTT, 1GB/day max transmission
- **Kill switch**: Remote disable via MQTT command

### Phase 3: LATER (Weeks 13+)
- Full feature set with configuration and NeoPixel
- Scale to 100+ devices
- **Guardrails**: Staged rollout by geography
- **Kill switch**: Cloud-based device management

## Risks & Mitigations

### Technical Risks
- **Risk**: I2C bus conflicts with multiple sensors
- **Mitigation**: Implement retry logic, adjustable clock speeds

- **Risk**: SD card corruption during write
- **Mitigation**: Journaling file system, periodic sync

### Operational Risks
- **Risk**: MQTT broker overload from device fleet
- **Mitigation**: Rate limiting, message batching, horizontal scaling

### User Risks
- **Risk**: Complex setup deters adoption
- **Mitigation**: Pre-configured devices, video tutorials, QR-based config

## Open Questions

1. **Data Format**: Should we use Protocol Buffers instead of JSON for efficiency?
2. **Sample Rates**: What's the optimal default IMU sampling rate for most use cases?
3. **Battery Management**: Should device auto-sleep after inactivity period?
4. **Data Retention**: How long should data remain on SD card before auto-deletion?
5. **Sensor Priority**: Which additional sensor should be implemented first in NEXT phase?
6. **Cloud Provider**: AWS IoT Core vs Azure IoT Hub vs custom MQTT broker?
7. **Firmware Updates**: OTA update mechanism priority and implementation approach?

## Success Criteria

**NOW Phase Success**:
- 5 working prototypes collecting IMU data
- 100+ successful recording sessions
- <5% failure rate on QR scanning
- Python QR generation tool in use for test preparation

**NEXT Phase Success**:
- 20 active beta users
- 10,000+ minutes of sensor data collected
- MQTT transmission success rate >95%

**LATER Phase Success**:
- 100+ devices in production
- 1M+ data points collected monthly
- User satisfaction score >4/5

---

*Document Version: 1.0*
*Created: 2025-11-12*
*Status: Draft - Pending Review*
