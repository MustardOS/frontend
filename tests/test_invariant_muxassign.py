import pytest
import ctypes
import os
import sys
import struct


# Simulate the buffer handling behavior that would be present in muxassign.c
# This test validates that any Python-level wrapper or reimplementation
# of the mux assignment logic never reads/writes beyond declared buffer lengths.

MAX_BUFFER_SIZE = 256  # Typical buffer size in muxassign.c context
FIELD_NAME_MAX = 64
ASSIGN_VALUE_MAX = 128


def safe_string_copy(destination_size: int, source: str) -> str:
    """
    Simulates safe string copy behavior (strcpy_s equivalent).
    Must truncate or reject strings exceeding destination_size - 1 (for null terminator).
    Returns the copied string or raises ValueError if source is None.
    """
    if source is None:
        raise ValueError("Source string cannot be None")
    
    max_chars = destination_size - 1  # Reserve space for null terminator
    if max_chars < 0:
        raise ValueError("Destination size must be at least 1")
    
    # Safe copy: truncate to fit within buffer
    result = source[:max_chars]
    return result


def mux_assign_field(field_name: str, value: str) -> dict:
    """
    Simulates mux assignment field processing as would occur in muxassign.c.
    Enforces buffer size limits to prevent overflow.
    """
    if not isinstance(field_name, str):
        raise TypeError("field_name must be a string")
    if not isinstance(value, str):
        raise TypeError("value must be a string")
    
    # Enforce buffer limits (simulating strcpy_s behavior)
    safe_field = safe_string_copy(FIELD_NAME_MAX, field_name)
    safe_value = safe_string_copy(ASSIGN_VALUE_MAX, value)
    
    return {
        "field_name": safe_field,
        "value": safe_value,
        "field_name_len": len(safe_field),
        "value_len": len(safe_value),
    }


def process_mux_buffer(data: bytes, declared_length: int) -> bytes:
    """
    Simulates buffer processing in muxassign.c.
    Must never read beyond declared_length bytes.
    """
    if declared_length < 0:
        raise ValueError("Declared length cannot be negative")
    
    if declared_length > MAX_BUFFER_SIZE:
        raise ValueError(f"Declared length {declared_length} exceeds maximum buffer size {MAX_BUFFER_SIZE}")
    
    # Only read up to declared_length bytes, never beyond
    safe_read = data[:declared_length]
    return safe_read


@pytest.mark.parametrize("payload", [
    # 2x oversized field names
    "A" * (FIELD_NAME_MAX * 2),
    # 10x oversized field names
    "B" * (FIELD_NAME_MAX * 10),
    # 2x oversized values
    "C" * (ASSIGN_VALUE_MAX * 2),
    # 10x oversized values
    "D" * (ASSIGN_VALUE_MAX * 10),
    # Exactly at boundary
    "E" * (FIELD_NAME_MAX - 1),
    # One over boundary
    "F" * FIELD_NAME_MAX,
    # Two over boundary
    "G" * (FIELD_NAME_MAX + 1),
    # Null bytes embedded in oversized string
    "H" * 50 + "\x00" + "I" * 200,
    # Format string attack payload oversized
    "%s%s%s%s%s%s%s%s%s%s" * 30,
    # SQL injection style oversized
    "' OR '1'='1" * 50,
    # Path traversal oversized
    "../../../etc/passwd" * 20,
    # Shell injection oversized
    "; cat /etc/passwd; " * 30,
    # Unicode oversized
    "\u0041\u0042\u0043" * 200,
    # Mixed special chars oversized
    "\x00\xff\xfe\x01" * 100,
    # Very large string (10000 chars)
    "X" * 10000,
    # Empty string (boundary case)
    "",
    # Single character
    "Y",
    # Exactly MAX_BUFFER_SIZE
    "Z" * MAX_BUFFER_SIZE,
    # MAX_BUFFER_SIZE + 1
    "W" * (MAX_BUFFER_SIZE + 1),
])
def test_buffer_read_never_exceeds_declared_length_field_name(payload):
    """Invariant: Buffer reads for field_name must never exceed FIELD_NAME_MAX - 1 bytes."""
    result = mux_assign_field(payload, "safe_value")
    
    # The resulting field_name must never exceed the declared buffer size minus null terminator
    assert result["field_name_len"] <= FIELD_NAME_MAX - 1, (
        f"Buffer overflow: field_name length {result['field_name_len']} "
        f"exceeds maximum allowed {FIELD_NAME_MAX - 1}"
    )
    
    # Verify the content is a proper prefix of the input (truncation, not corruption)
    assert result["field_name"] == payload[:FIELD_NAME_MAX - 1], (
        "Truncated string must be exact prefix of input"
    )
    
    # Ensure no data beyond the buffer boundary leaked through
    assert len(result["field_name"]) <= FIELD_NAME_MAX - 1


@pytest.mark.parametrize("payload", [
    # 2x oversized values
    "A" * (ASSIGN_VALUE_MAX * 2),
    # 10x oversized values
    "B" * (ASSIGN_VALUE_MAX * 10),
    # Exactly at boundary
    "C" * (ASSIGN_VALUE_MAX - 1),
    # One over boundary
    "D" * ASSIGN_VALUE_MAX,
    # Binary data oversized
    "\x00\x01\x02\x03" * 100,
    # Newline injection oversized
    "value\ninjected\n" * 50,
    # Carriage return injection oversized
    "value\r\ninjected\r\n" * 50,
    # Tab injection oversized
    "value\tinjected\t" * 50,
    # Very large value
    "V" * 10000,
    # Empty value
    "",
])
def test_buffer_read_never_exceeds_declared_length_value(payload):
    """Invariant: Buffer reads for value must never exceed ASSIGN_VALUE_MAX - 1 bytes."""
    result = mux_assign_field("safe_field", payload)
    
    assert result["value_len"] <= ASSIGN_VALUE_MAX - 1, (
        f"Buffer overflow: value length {result['value_len']} "
        f"exceeds maximum allowed {ASSIGN_VALUE_MAX - 1}"
    )
    
    assert result["value"] == payload[:ASSIGN_VALUE_MAX - 1], (
        "Truncated value must be exact prefix of input"
    )
    
    assert len(result["value"]) <= ASSIGN_VALUE_MAX - 1


@pytest.mark.parametrize("data,declared_length", [
    # Data larger than declared length - should only read declared_length bytes
    (b"A" * 512, 100),
    (b"B" * 1000, 50),
    (b"C" * MAX_BUFFER_SIZE * 2, MAX_BUFFER_SIZE),
    (b"D" * MAX_BUFFER_SIZE * 10, MAX_BUFFER_SIZE),
    # Exactly at max buffer size
    (b"E" * MAX_BUFFER_SIZE, MAX_BUFFER_SIZE),
    # Declared length smaller than data
    (b"\x00\xff" * 200, 10),
    # Binary payload with declared length
    (b"\x41\x42\x43\x44" * 100, 16),
    # Null bytes in data
    (b"\x00" * 500, 50),
    # Mixed binary data
    (bytes(range(256)) * 2, 128),
    # Empty data with zero declared length
    (b"", 0),
    # Single byte
    (b"X", 1),
    # Data with embedded null, declared length cuts before null
    (b"hello\x00world" * 10, 5),
])
def test_buffer_read_never_exceeds_declared_length_raw_buffer(data, declared_length):
    """Invariant: Raw buffer reads must never exceed the declared_length parameter."""
    result = process_mux_buffer(data, declared_length)
    
    # Result must never be longer than declared_length
    assert len(result) <= declared_length, (
        f"Buffer over-read: read {len(result)} bytes but declared length was {declared_length}"
    )
    
    # Result must be a prefix of the original data
    assert result == data[:declared_length], (
        "Buffer read result must be exact prefix up to declared_length"
    )
    
    # Ensure no bytes beyond declared_length were accessed
    if len(data) > declared_length:
        assert result != data, (
            "When data exceeds declared_length, result must be truncated"
        ) if len(data) > declared_length and declared_length < len(data) else None


@pytest.mark.parametrize("declared_length", [
    # Invalid/adversarial declared lengths
    -1,
    -100,
    -MAX_BUFFER_SIZE,
    MAX_BUFFER_SIZE + 1,
    MAX_BUFFER_SIZE * 2,
    MAX_BUFFER_SIZE * 10,
    sys.maxsize,
    2**31 - 1,
    2**32 - 1,
])
def test_invalid_declared_length_rejected(declared_length):
    """Invariant: Invalid declared lengths (negative or exceeding max) must be rejected."""
    data = b"A" * 100
    
    with pytest.raises((ValueError, OverflowError, TypeError)):
        process_mux_buffer(data, declared_length)


@pytest.mark.parametrize("field_name,value", [
    # Both fields oversized
    ("A" * 1000, "B" * 1000),
    # Field name oversized, value normal
    ("C" * 500, "normal_value"),
    # Field name normal, value oversized
    ("normal_field", "D" * 500),
    # Both at exact boundary
    ("E" * (FIELD_NAME_MAX - 1), "F" * (ASSIGN_VALUE_MAX - 1)),
    # Adversarial combined payloads
    ("%n%s%p%x" * 100, "$(cat /etc/passwd)" * 50),
    # Unicode attack
    ("\u202e" * 200, "\ufeff" * 200),
    # Null byte injection
    ("field\x00overflow" * 20, "value\x00overflow" * 20),
])
def test_combined_buffer_overflow_protection(field_name, value):
    """Invariant: Combined field_name and value processing must never exceed their respective buffer limits."""
    result = mux_assign_field(field_name, value)
    
    assert result["field_name_len"] <= FIELD_NAME_MAX - 1, (
        f"field_name buffer overflow detected: {result['field_name_len']} > {FIELD_NAME_MAX - 1}"
    )
    
    assert result["value_len"] <= ASSIGN_VALUE_MAX - 1, (
        f"value buffer overflow detected: {result['value_len']} > {ASSIGN_VALUE_MAX - 1}"
    )
    
    # Total combined length must not exceed sum of individual maximums
    total_len = result["field_name_len"] + result["value_len"]
    max_total = (FIELD_NAME_MAX - 1) + (ASSIGN_VALUE_MAX - 1)
    assert total_len <= max_total, (
        f"Combined buffer length {total_len} exceeds maximum {max_total}"
    )