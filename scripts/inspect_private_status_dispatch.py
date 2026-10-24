#!/usr/bin/env python3
"""Verify HDExtreme status-reader virtual dispatch in the authorized private binary.

This read-only tool is pinned to the 2026-10-21 Desktop Video 10.4
kernel binary. It never reads device memory, loads a driver, invokes
third-party code, or distributes proprietary executable contents.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

from inspect_private_model_dispatch import ORIGINAL_SHA256, translate_va

VTABLE_SYMBOL = (
    "__ZTV73com_blackmagic_design_iokit_"
    "DeckLink_HardwareAbstractionDeckLinkHDExtreme"
)
READ_SYMBOL = (
    "__ZN79com_blackmagic_design_iokit_"
    "DeckLink_HardwareAbstractionDeckLinkHDProPCIExpress"
    "18ReadRegisterEndianEPVj"
)
WRITE_SYMBOL = (
    "__ZN79com_blackmagic_design_iokit_"
    "DeckLink_HardwareAbstractionDeckLinkHDProPCIExpress"
    "19WriteRegisterEndianEPVjj"
)

LC_SYMTAB = 2
READ_SLOT_BYTES = 0x7A0
WRITE_SLOT_BYTES = 0x7A8
# Itanium x86-64 ABI vtable pointer starts after offset-to-top and RTTI.
VTABLE_ADDRESS_POINT_DELTA = 16


def symbol_addresses(data):
    """Return explicitly defined Mach-O symbol VM addresses by raw name."""
    if len(data) < 32:
        raise ValueError("Truncated Mach-O header")
    magic, cpu, _, _, ncmds, sizeofcmds, _, _ = struct.unpack_from(
        "<IiiIIIII", data, 0
    )
    if magic != 0xFEEDFACF or cpu != 0x01000007:
        raise ValueError("Expected x86-64 little-endian Mach-O")
    if ncmds > 4096 or sizeofcmds > len(data) - 32:
        raise ValueError("Invalid load-command bounds")

    offset = 32
    symtab = None
    for _ in range(ncmds):
        if offset + 8 > len(data):
            raise ValueError("Truncated load command")
        command, size = struct.unpack_from("<II", data, offset)
        if size < 8 or offset + size > 32 + sizeofcmds:
            raise ValueError("Malformed load command")
        if command == LC_SYMTAB:
            if size < 24 or symtab is not None:
                raise ValueError("Malformed or duplicate symbol table")
            _, _, symoff, nsyms, stroff, strsize = struct.unpack_from(
                "<IIIIII", data, offset
            )
            symtab = symoff, nsyms, stroff, strsize
        offset += size
    if symtab is None:
        raise ValueError("No Mach-O symbol table")
    symoff, nsyms, stroff, strsize = symtab
    if (nsyms > 2000000 or symoff + nsyms * 16 > len(data) or
            stroff + strsize > len(data)):
        raise ValueError("Symbol table outside file")
    strings = data[stroff:stroff + strsize]
    symbols = {}
    for i in range(nsyms):
        strx, ntype, section, desc, value = struct.unpack_from(
            "<IBBHQ", data, symoff + i * 16
        )
        if not strx or strx >= len(strings):
            continue
        terminator = strings.find(b"\x00", strx)
        if terminator < 0:
            continue
        name = strings[strx:terminator].decode("ascii", "replace")
        if name in (VTABLE_SYMBOL, READ_SYMBOL, WRITE_SYMBOL):
            if name in symbols:
                raise ValueError("Duplicate named ABI symbol")
            symbols[name] = value
    return symbols


def read_slot_target(data, vtable_vmaddr, slot_bytes):
    """Read a vtable code pointer from Mach-O file bytes, not hardware."""
    vmaddr = vtable_vmaddr + VTABLE_ADDRESS_POINT_DELTA + slot_bytes
    offset = translate_va(data, vmaddr)
    if offset + 8 > len(data):
        raise ValueError("Vtable slot outside private binary")
    return struct.unpack_from("<Q", data, offset)[0]


def analyze_private_copy(data):
    digest = hashlib.sha256(data).hexdigest()
    if digest != ORIGINAL_SHA256:
        raise ValueError("Unsupported binary SHA-256; no inference permitted")
    symbols = symbol_addresses(data)
    for name in (VTABLE_SYMBOL, READ_SYMBOL, WRITE_SYMBOL):
        if name not in symbols:
            raise ValueError("Required Mach-O symbol unavailable: " + name)
    vtable = symbols[VTABLE_SYMBOL]
    read_target = read_slot_target(data, vtable, READ_SLOT_BYTES)
    write_target = read_slot_target(data, vtable, WRITE_SLOT_BYTES)
    if read_target != symbols[READ_SYMBOL] or write_target != symbols[WRITE_SYMBOL]:
        raise ValueError("HDExtreme virtual dispatch does not match expected ABI")
    return {
        "status": "static_vtable_match",
        "sha256": digest,
        "class": "HardwareAbstractionDeckLinkHDExtreme",
        "read_slot": hex(READ_SLOT_BYTES),
        "read_method": "HDProPCIExpress::ReadRegisterEndian",
        "write_slot": hex(WRITE_SLOT_BYTES),
        "write_method": "HDProPCIExpress::WriteRegisterEndian",
        "runtime_class_verified": False,
        "hardware_access": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(analyze_private_copy(args.binary.read_bytes()),
                         indent=2, sort_keys=True))
    except (OSError, ValueError) as exc:
        print("status=unverified reason=" + str(exc), file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
