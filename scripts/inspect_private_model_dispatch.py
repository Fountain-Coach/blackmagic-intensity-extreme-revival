#!/usr/bin/env python3
"""Read a privately supplied Desktop Video 10.4 Mach-O model-dispatch table.

Never modifies the original executable, any system installation, or hardware.
Fails closed unless SHA-256 matches the single version analyzed for this
investigation. Outputs only the selected device ID and its model table entry.

Copyright (c) 2026 Fountain Coach contributors. MIT.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ORIGINAL_SHA256 = "c4ec945aa46d07123788e66c219203026d1dcddffad20d1a55b8b5b4943d3780"
MODEL_TABLE_VA = 0x56BE2
MODEL_TABLE_COUNT = 0x4D
MODEL_TABLE_STRIDE = 16
CPU_X86_64 = 0x01000007
MH_MAGIC_64 = 0xFEEDFACF
LC_SEGMENT_64 = 0x19


def translate_va(blob, va):
    """Map a Mach-O VM address into the corresponding file offset."""
    if len(blob) < 32:
        raise ValueError("Truncated Mach-O header")
    magic, cpu, _, _, ncmds, sizeofcmds, _, _ = struct.unpack_from(
        "<IiiIIIII", blob, 0)
    if magic != MH_MAGIC_64 or cpu != CPU_X86_64:
        raise ValueError("Only little-endian x86-64 Mach-O is supported")
    if ncmds > 4096 or 32 + sizeofcmds > len(blob):
        raise ValueError("Invalid Mach-O load commands")

    offset = 32
    matches = []
    for _ in range(ncmds):
        if offset + 8 > len(blob):
            raise ValueError("Truncated load command")
        command, size = struct.unpack_from("<II", blob, offset)
        if size < 8 or offset + size > 32 + sizeofcmds:
            raise ValueError("Invalid load command size")
        if command == LC_SEGMENT_64:
            if size < 72:
                raise ValueError("Truncated 64-bit segment")
            seg = struct.unpack_from("<II16sQQQQiiII", blob, offset)
            name = seg[2].split(b"\0")[0].decode("ascii", "replace")
            start, span, file_start, file_length = seg[3:7]
            if (name == "__DATA" and start <= va < start + span
                    and va - start < file_length):
                result = file_start + (va - start)
                if result >= len(blob):
                    raise ValueError("Mapped address exceeds file")
                matches.append(result)
        offset += size
    if len(matches) != 1:
        raise ValueError("Model table not uniquely mapped from __DATA")
    return matches[0]


def extract_device_matches(blob, table_offset, device_id, count=MODEL_TABLE_COUNT):
    """Return (index, model-code) tuples; model-code precedes the ID by 2 bytes."""
    if count < 1 or table_offset < 2:
        raise ValueError("Invalid table configuration")
    if table_offset + (count - 1) * MODEL_TABLE_STRIDE + 2 > len(blob):
        raise ValueError("Model table exceeds file")
    matches = []
    for i in range(count):
        offset = table_offset + i * MODEL_TABLE_STRIDE
        pci_id = struct.unpack_from("<H", blob, offset)[0]
        if pci_id == device_id:
            matches.append((i, blob[offset - 2]))
    return matches


def analyze_private_copy(blob, device_id):
    digest = hashlib.sha256(blob).hexdigest()
    if digest != ORIGINAL_SHA256:
        raise ValueError("Unsupported binary SHA-256; no inference permitted")
    table_offset = translate_va(blob, MODEL_TABLE_VA)
    matches = extract_device_matches(blob, table_offset, device_id)
    if len(matches) != 1:
        raise ValueError("Expected exactly one matching model-table entry")
    index, model_code = matches[0]
    return {
        "status": "static_table_match",
        "sha256": digest,
        "pci_device_id": f"0x{device_id:04x}",
        "model_table_index": index,
        "model_code": f"0x{model_code:02x}",
        "model_table_va": f"0x{MODEL_TABLE_VA:x}",
        "runtime_class_verified": False,
        "hardware_access": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path,
                        help="Locally authorized DeckLink_Driver Mach-O; not uploaded")
    parser.add_argument("--device-id", default="0xa124",
                        help="PCI device ID, default original Intensity Extreme")
    args = parser.parse_args()
    try:
        device_id = int(args.device_id, 0)
        if not 0 <= device_id <= 0xFFFF:
            raise ValueError("Device ID outside 16-bit PCI range")
        result = analyze_private_copy(args.binary.read_bytes(), device_id)
    except (OSError, ValueError) as exc:
        print("status=unverified reason=" + str(exc), file=sys.stderr)
        return 2
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
