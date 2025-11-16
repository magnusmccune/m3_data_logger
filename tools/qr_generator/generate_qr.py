#!/usr/bin/env python3
"""
QR Code Generator for M3 Data Logger

Generates QR codes with test metadata in JSON format for scanning by the M3 Data Logger.

New format (M3L-64):
{
  "test_id": "A3F9K2M7",        // 8-char alphanumeric shortUUID
  "description": "walking_outdoor",
  "labels": ["walking", "outdoor"]
}

Usage:
    python generate_qr.py --description "walking_outdoor" --labels walking outdoor        # Auto-generates test_id
    python generate_qr.py --test-id A3F9K2M7 --description "walking_outdoor" --labels walking outdoor
    python generate_qr.py --help
"""

import argparse
import json
import random
import re
import string
import sys

try:
    import qrcode
except ImportError:
    print("Error: qrcode library not found. Install with: pip install qrcode[pil]")
    sys.exit(1)


# QR Size Constraints (Tiny Code Reader hardware limit: 256 bytes)
# JSON structure overhead ~147 bytes, leaving ~73 bytes for field data
QR_MAX_PAYLOAD_BYTES = 220

# Field length limits optimized for QR size (cannot max all fields simultaneously)
# Realistic configs fit comfortably (see README for examples)
WIFI_SSID_MAX_LEN = 16      # IEEE 802.11 allows 32, reduced for QR size
WIFI_PASSWORD_MAX_LEN = 16  # WPA2 minimum 8, maximum 16 for QR size
MQTT_HOST_MAX_LEN = 40      # Fits most hostnames (AWS IoT needs abbreviation)
MQTT_USERNAME_MAX_LEN = 10  # Optional field
MQTT_PASSWORD_MAX_LEN = 10  # Optional field
DEVICE_ID_MAX_LEN = 10      # Reasonable identifier length


def generate_short_uuid(length=8):
    """
    Generate a short UUID-style ID using alphanumeric characters.

    Excludes ambiguous characters: 0, O, 1, I, l to avoid confusion.

    Args:
        length: Number of characters (default 8)

    Returns:
        String of random alphanumeric characters (e.g., "A3F9K2M7")
    """
    # Use uppercase letters and digits, excluding ambiguous characters
    chars = ''.join(set(string.ascii_uppercase + string.digits) - set('01IOl'))
    return ''.join(random.choice(chars) for _ in range(length))


def validate_test_id(test_id):
    """Validate test_id is exactly 8 alphanumeric characters."""
    if len(test_id) != 8:
        return False, "test_id must be exactly 8 characters"
    if not test_id.isalnum():
        return False, "test_id must be alphanumeric only"
    return True, ""


def validate_description(description):
    """Validate description is 1-64 characters."""
    if len(description) == 0:
        return False, "description cannot be empty"
    if len(description) > 64:
        return False, "description must be 64 characters or less"
    return True, ""


def validate_labels(labels):
    """Validate labels array has 1-10 items, each 1-32 characters."""
    if len(labels) == 0:
        return False, "must provide at least 1 label"
    if len(labels) > 10:
        return False, "cannot exceed 10 labels"
    for label in labels:
        if len(label) == 0:
            return False, f"label '{label}' cannot be empty"
        if len(label) > 32:
            return False, f"label '{label}' exceeds 32 characters"
    return True, ""


def validate_wifi_ssid(ssid):
    """Validate WiFi SSID (1-16 chars, printable ASCII per IEEE 802.11)."""
    if not 1 <= len(ssid) <= WIFI_SSID_MAX_LEN:
        return False, f"SSID must be 1-{WIFI_SSID_MAX_LEN} characters (IEEE 802.11 allows 32, reduced for QR size)"

    # IEEE 802.11 allows printable ASCII (0x20-0x7E)
    if not all(0x20 <= ord(c) <= 0x7E for c in ssid):
        return False, "SSID contains non-printable characters (use printable ASCII)"

    return True, ""


def validate_wifi_password(password):
    """Validate WiFi password (WPA2 requirements, max 16 chars for QR size)."""
    pwd_len = len(password)

    # For QR size constraints, limit to 16 chars (still secure)
    if not 8 <= pwd_len <= 16:
        return False, "Password must be 8-16 characters (WPA2 requirement + QR size limit)"

    # WPA2 passphrase: printable ASCII only (0x20-0x7E)
    if not all(0x20 <= ord(c) <= 0x7E for c in password):
        return False, "Password must contain only printable ASCII characters (ASCII 32-126)"

    return True, ""


def validate_mqtt_host(host):
    """Validate MQTT broker host (DNS or IP format)."""
    # DNS pattern
    dns_pattern = r'^[a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(\.[a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*$'
    # IPv4 pattern
    ipv4_pattern = r'^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$'

    if re.match(dns_pattern, host) or re.match(ipv4_pattern, host):
        return True, ""
    return False, "Invalid MQTT host format (must be DNS name or IPv4 address)"


def validate_mqtt_port(port):
    """Validate MQTT port range (1-65535)."""
    if not 1 <= port <= 65535:
        return False, f"Port {port} out of valid range (1-65535)"
    return True, ""


def validate_device_id(device_id):
    """Validate device ID (1-16 characters for QR size)."""
    if not 1 <= len(device_id) <= DEVICE_ID_MAX_LEN:
        return False, f"Device ID must be 1-{DEVICE_ID_MAX_LEN} characters"
    return True, ""


def generate_qr_code(test_id, description, labels, output_path=None, show=True):
    """
    Generate QR code with test metadata.

    Args:
        test_id: 8-character alphanumeric test ID
        description: Human-readable test description (1-64 chars)
        labels: List of label strings (1-10 labels, each 1-32 chars)
        output_path: Optional path to save QR code image
        show: If True, display QR code in terminal

    Returns:
        JSON string of metadata
    """
    # Validate inputs
    valid, error = validate_test_id(test_id)
    if not valid:
        raise ValueError(f"Invalid test_id: {error}")

    valid, error = validate_description(description)
    if not valid:
        raise ValueError(f"Invalid description: {error}")

    valid, error = validate_labels(labels)
    if not valid:
        raise ValueError(f"Invalid labels: {error}")

    # Build JSON metadata
    metadata = {
        "test_id": test_id,
        "description": description,
        "labels": labels
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

    # Display in terminal if requested
    if show:
        print("\nQR Code (scan with M3 Data Logger):")
        qr.print_ascii(invert=True)

    # Save to file if path provided
    if output_path:
        img = qr.make_image(fill_color="black", back_color="white")
        img.save(output_path)
        print(f"\n✓ QR code saved to: {output_path}")

    # Print metadata for verification
    print(f"\nMetadata JSON ({len(json_str)} bytes):")
    print(json.dumps(metadata, indent=2))

    return json_str


def generate_config_qr(wifi_ssid, wifi_password, mqtt_host, mqtt_port, mqtt_username, mqtt_password, device_id, output_path=None, show=True):
    """
    Generate device configuration QR code.

    Args:
        wifi_ssid: WiFi network SSID (1-32 chars, printable ASCII)
        wifi_password: WiFi password (8-16 chars for WPA2 + QR size)
        mqtt_host: MQTT broker host (DNS name or IPv4 address, max 64 chars)
        mqtt_port: MQTT broker port (1-65535)
        mqtt_username: MQTT username (optional, max 16 chars)
        mqtt_password: MQTT password (optional, max 16 chars)
        device_id: Device identifier string (max 16 chars)
        output_path: Optional path to save QR code image
        show: If True, display QR code in terminal

    Returns:
        JSON string of configuration data
    """
    # Validate inputs
    valid, error = validate_wifi_ssid(wifi_ssid)
    if not valid:
        raise ValueError(f"Invalid WiFi SSID: {error}")

    valid, error = validate_wifi_password(wifi_password)
    if not valid:
        raise ValueError(f"Invalid WiFi password: {error}")

    valid, error = validate_mqtt_host(mqtt_host)
    if not valid:
        raise ValueError(f"Invalid MQTT host: {error}")

    # Additional length validation for MQTT host
    if len(mqtt_host) > MQTT_HOST_MAX_LEN:
        raise ValueError(f"MQTT host too long ({len(mqtt_host)} chars, max {MQTT_HOST_MAX_LEN})")

    valid, error = validate_mqtt_port(mqtt_port)
    if not valid:
        raise ValueError(f"Invalid MQTT port: {error}")

    # Validate optional fields
    if len(mqtt_username) > MQTT_USERNAME_MAX_LEN:
        raise ValueError(f"MQTT username too long ({len(mqtt_username)} chars, max {MQTT_USERNAME_MAX_LEN})")

    if len(mqtt_password) > MQTT_PASSWORD_MAX_LEN:
        raise ValueError(f"MQTT password too long ({len(mqtt_password)} chars, max {MQTT_PASSWORD_MAX_LEN})")

    valid, error = validate_device_id(device_id)
    if not valid:
        raise ValueError(f"Invalid device ID: {error}")

    # Build JSON configuration
    config_data = {
        "type": "device_config",
        "version": "1.0",
        "wifi": {
            "ssid": wifi_ssid,
            "password": wifi_password
        },
        "mqtt": {
            "host": mqtt_host,
            "port": mqtt_port,
            "username": mqtt_username,
            "password": mqtt_password,
            "device_id": device_id
        }
    }

    json_str = json.dumps(config_data, separators=(',', ':'))  # Compact JSON
    json_size = len(json_str)

    # Check total size against Tiny Code Reader limit
    if json_size > QR_MAX_PAYLOAD_BYTES:
        field_usage = (
            f"  WiFi SSID: {len(wifi_ssid)}/{WIFI_SSID_MAX_LEN} chars\n"
            f"  WiFi Password: {len(wifi_password)}/{WIFI_PASSWORD_MAX_LEN} chars\n"
            f"  MQTT Host: {len(mqtt_host)}/{MQTT_HOST_MAX_LEN} chars\n"
            f"  MQTT Username: {len(mqtt_username)}/{MQTT_USERNAME_MAX_LEN} chars\n"
            f"  MQTT Password: {len(mqtt_password)}/{MQTT_PASSWORD_MAX_LEN} chars\n"
            f"  Device ID: {len(device_id)}/{DEVICE_ID_MAX_LEN} chars\n"
            f"\nNote: You cannot max out all fields simultaneously.\n"
            f"Reduce longest fields (MQTT host, SSID, or passwords) to fit."
        )
        raise ValueError(
            f"Config JSON too large ({json_size} bytes, max {QR_MAX_PAYLOAD_BYTES})\n"
            f"Current field usage:\n{field_usage}"
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

    # Display in terminal if requested
    if show:
        print("\nConfiguration QR Code (scan with M3 Data Logger):")
        qr.print_ascii(invert=True)

    # Save to file if path provided
    if output_path:
        img = qr.make_image(fill_color="black", back_color="white")
        img.save(output_path)
        print(f"\n✓ Config QR code saved to: {output_path}")

    # Print configuration for verification (mask passwords)
    config_display = config_data.copy()
    config_display["wifi"]["password"] = "********"
    if config_display["mqtt"]["password"]:
        config_display["mqtt"]["password"] = "********"

    print(f"\nConfiguration JSON ({len(json_str)} bytes):")
    print(json.dumps(config_display, indent=2))

    return json_str


def main():
    parser = argparse.ArgumentParser(
        description="Generate QR codes for M3 Data Logger (metadata or device configuration)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples - Metadata Mode:
  # Generate metadata QR with auto-generated test ID (default)
  python generate_qr.py --mode metadata --description "walking_outdoor" --labels walking outdoor

  # Generate metadata QR with specific test ID
  python generate_qr.py --mode metadata --test-id A3F9K2M7 --description "running_indoor" --labels running indoor

  # Save to file
  python generate_qr.py --mode metadata --description "test1" --labels demo --output test1_qr.png

Examples - Config Mode:
  # Generate device configuration QR
  python generate_qr.py --mode config \\
    --wifi-ssid "MyNetwork" \\
    --wifi-password "SecurePassword123" \\
    --mqtt-host "mqtt.example.com" \\
    --mqtt-port 1883 \\
    --mqtt-username "device123" \\
    --mqtt-password "secret" \\
    --device-id "m3logger_001"

  # Save config QR to file
  python generate_qr.py --mode config \\
    --wifi-ssid "TestNet" --wifi-password "password123" \\
    --mqtt-host "192.168.1.100" --device-id "m3logger_test" \\
    --output config_qr.png
        """
    )

    # Mode selection
    parser.add_argument('--mode', choices=['metadata', 'config'], required=True,
                        help='QR code generation mode (metadata or config)')

    # Metadata mode arguments
    parser.add_argument('--test-id', type=str, help='[metadata] 8-character alphanumeric test ID (optional, auto-generated if not provided)')
    parser.add_argument('--description', type=str, help='[metadata] Test description (1-64 chars)')
    parser.add_argument('--labels', nargs='+', help='[metadata] Labels (1-10 labels, each 1-32 chars)')

    # Config mode arguments
    parser.add_argument('--wifi-ssid', type=str, help='[config] WiFi SSID (1-32 chars)')
    parser.add_argument('--wifi-password', type=str, help='[config] WiFi password (min 8 chars)')
    parser.add_argument('--mqtt-host', type=str, help='[config] MQTT broker host (DNS or IP)')
    parser.add_argument('--mqtt-port', type=int, default=1883, help='[config] MQTT broker port (default: 1883)')
    parser.add_argument('--mqtt-username', type=str, default='', help='[config] MQTT username (optional)')
    parser.add_argument('--mqtt-password', type=str, default='', help='[config] MQTT password (optional)')
    parser.add_argument('--device-id', type=str, help='[config] Device identifier')

    # Common arguments
    parser.add_argument('--output', '-o', type=str, help='Output PNG file path')
    parser.add_argument('--no-show', action='store_true', help='Do not display QR in terminal')

    args = parser.parse_args()

    try:
        if args.mode == 'metadata':
            # Validate required metadata arguments
            if not args.description:
                parser.error("Metadata mode requires --description")
            if not args.labels:
                parser.error("Metadata mode requires --labels")

            # Determine test_id (auto-generate if not provided)
            if args.test_id:
                test_id = args.test_id
            else:
                test_id = generate_short_uuid()
                print(f"Generated test_id: {test_id}")

            generate_qr_code(
                test_id=test_id,
                description=args.description,
                labels=args.labels,
                output_path=args.output,
                show=not args.no_show
            )

        elif args.mode == 'config':
            # Validate required config arguments
            required_config_args = ['wifi_ssid', 'wifi_password', 'mqtt_host', 'device_id']
            missing = [arg for arg in required_config_args if not getattr(args, arg)]
            if missing:
                parser.error(f"Config mode requires: {', '.join('--' + arg.replace('_', '-') for arg in missing)}")

            generate_config_qr(
                wifi_ssid=args.wifi_ssid,
                wifi_password=args.wifi_password,
                mqtt_host=args.mqtt_host,
                mqtt_port=args.mqtt_port,
                mqtt_username=args.mqtt_username,
                mqtt_password=args.mqtt_password,
                device_id=args.device_id,
                output_path=args.output,
                show=not args.no_show
            )

    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
