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
    python generate_qr.py --test-id A3F9K2M7 --description "walking_outdoor" --labels walking outdoor
    python generate_qr.py --random                           # Generate random test_id
    python generate_qr.py --help
"""

import argparse
import json
import random
import string
import sys

try:
    import qrcode
except ImportError:
    print("Error: qrcode library not found. Install with: pip install qrcode[pil]")
    sys.exit(1)


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
    return True, None


def validate_description(description):
    """Validate description is 1-64 characters."""
    if len(description) == 0:
        return False, "description cannot be empty"
    if len(description) > 64:
        return False, "description must be 64 characters or less"
    return True, None


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
    return True, None


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


def main():
    parser = argparse.ArgumentParser(
        description="Generate QR codes for M3 Data Logger test metadata",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Generate QR with specific test ID
  python generate_qr.py --test-id A3F9K2M7 --description "walking_outdoor" --labels walking outdoor

  # Generate QR with random test ID
  python generate_qr.py --random --description "running_indoor" --labels running indoor

  # Save to file
  python generate_qr.py --random --description "test1" --labels demo --output test1_qr.png

  # Generate batch of QR codes
  for i in {1..5}; do
    python generate_qr.py --random --description "test$i" --labels demo --output "test${i}_qr.png" --no-show
  done
        """
    )

    parser.add_argument('--test-id', type=str, help='8-character alphanumeric test ID (e.g., A3F9K2M7)')
    parser.add_argument('--random', action='store_true', help='Generate random test ID')
    parser.add_argument('--description', type=str, required=True, help='Test description (1-64 chars)')
    parser.add_argument('--labels', nargs='+', required=True, help='Labels (1-10 labels, each 1-32 chars)')
    parser.add_argument('--output', '-o', type=str, help='Output PNG file path')
    parser.add_argument('--no-show', action='store_true', help='Do not display QR in terminal')

    args = parser.parse_args()

    # Determine test_id
    if args.random:
        test_id = generate_short_uuid()
        print(f"Generated random test_id: {test_id}")
    elif args.test_id:
        test_id = args.test_id
    else:
        parser.error("Must provide either --test-id or --random")

    try:
        generate_qr_code(
            test_id=test_id,
            description=args.description,
            labels=args.labels,
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
