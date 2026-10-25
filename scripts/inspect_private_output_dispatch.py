#!/usr/bin/env python3
"""Static, SHA-pinned A124 HDMI output virtual-dispatch witness.

No device access, vendor code execution, firmware mutation or PCI I/O.
This is a narrowly scoped Mach-O symbol/vtable inspection of the exact
private Desktop Video 10.4 kernel image. Never infer an HDMI PHY-off
register merely because a method has an output- or EDID-related name.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

from inspect_private_model_dispatch import ORIGINAL_SHA256, translate_va

LC_SYMTAB = 2
VTABLE_PREFIX_BYTES = 16
MODEL_NAME = "com_blackmagic_design_iokit_DeckLink_HardwareAbstractionDeckLink"
HD = MODEL_NAME + "HDProPCIExpress"
EXTREME = MODEL_NAME + "HDExtreme"
PREFIX = "__ZTV"
HD_VTABLE = PREFIX + str(len(HD)) + HD
EXTREME_VTABLE = PREFIX + str(len(EXTREME)) + EXTREME

HD_BASE_MODE = (
    "__ZN69com_blackmagic_design_iokit_DeckLink_"
    "HardwareAbstractionDeckLinkHDPro18SetVideoOutputModeEj"
)
EXTREME_MODE = (
    "__ZN73com_blackmagic_design_iokit_DeckLink_"
    "HardwareAbstractionDeckLinkHDExtreme18SetVideoOutputModeEj"
)
EXTREME_3D = (
    "__ZN73com_blackmagic_design_iokit_DeckLink_"
    "HardwareAbstractionDeckLinkHDExtreme25AutoSwitch3DPackingFormatEv"
)
EXTREME_EDID = (
    "__ZN73com_blackmagic_design_iokit_DeckLink_"
    "HardwareAbstractionDeckLinkHDExtreme14ReadOutputEDIDEv"
)
EXTREME_SINK = (
    "__ZN73com_blackmagic_design_iokit_DeckLink_"
    "HardwareAbstractionDeckLinkHDExtreme27WriteHdmiSinkCapabilityBitsEP20HdmiSinkCapabilitiesPbPj"
)

def all_symbols(data: bytes) -> dict[str, int]:
    """Extract named nlist addresses; no external tools or binary execution."""
    if len(data) < 32:
        raise ValueError("truncated Mach-O")
    magic, cpu, _, _, ncmds, cmds_size, _, _ = struct.unpack_from("<IiiIIIII", data)
    if magic != 0xFEEDFACF or cpu != 0x01000007:
        raise ValueError("unexpected binary format")
    if ncmds > 4096 or cmds_size > len(data) - 32:
        raise ValueError("invalid Mach-O commands")
    off = 32
    table = None
    for _ in range(ncmds):
        if off + 8 > len(data):
            raise ValueError("truncated command")
        command, length = struct.unpack_from("<II", data, off)
        if length < 8 or off + length > 32 + cmds_size:
            raise ValueError("invalid command bounds")
        if command == LC_SYMTAB:
            if table is not None or length < 24:
                raise ValueError("ambiguous symbol table")
            _, _, symoff, nsyms, stroff, strsize = struct.unpack_from(
                "<IIIIII", data, off
            )
            table = (symoff, nsyms, stroff, strsize)
        off += length
    if table is None:
        raise ValueError("missing symbol table")
    symoff, nsyms, stroff, strsize = table
    if nsyms > 2000000 or symoff + 16 * nsyms > len(data):
        raise ValueError("invalid symbol entries")
    if stroff + strsize > len(data):
        raise ValueError("invalid string table")
    names = data[stroff:stroff + strsize]
    wanted = {HD_VTABLE, EXTREME_VTABLE, HD_BASE_MODE, EXTREME_MODE,
              EXTREME_3D, EXTREME_EDID, EXTREME_SINK}
    result = {}\n    for i in range(nsyms):
        string_offset, _, _, _, addr = struct.unpack_from(
            "<IBBHQ", data, symoff + 16 * i
        )
        if not string_offset or string_offset >= len(names):
            continue
        end = names.find(b"\0", string_offset)
        if end == -1:
            raise ValueError("unterminated Mach-O name")
        name = names[string_offset:end].decode("ascii", "replace")
        if name not in wanted:
            continue
        if name in result:
            raise ValueError("duplicate required symbol " + name)
        result[name] = addr
    return result

def pointer(data: bytes, virtual_address: int) -> int:
    off = translate_va(data, virtual_address)
    if off + 8 > len(data):
        raise ValueError("vtable entry outside original image")
    return struct.unpack_from("<Q", data, off)[0]

def evaluate(data: bytes) -> dict:
    digest = hashlib.sha256(data).hexdigest()
    if digest != ORIGINAL_SHA256:
        raise ValueError("unsupported or changed vendor driver SHA-256")
    syms = all_symbols(data)
    for name in (HD_VTABLE, EXTREME_VTABLE, HD_BASE_MODE,
                 EXTREME_MODE, EXTREME_3D, EXTREME_EDID, EXTREME_SINK):
        if name not in syms:
            raise ValueError("required A124 symbol missing: " + name)
    # In original HDExtreme::SetVideoOutputMode disassembly:
    # callq *0x280(%rax) where rax points to the HDProPCIExpress vtable
    # *symbol beginning*, not the normal C++ address point.
    base_target = pointer(data, syms[HD_VTABLE] + 0x280)
    if base_target != syms[HD_BASE_MODE]:
        raise ValueError("unexpected SetVideoOutputMode base target")
    # Tail jump at HDExtreme::SetVideoOutputMode + 0x68:
    # jmpq *0x810(%rax), rax = this->vptr (address point + 16).
    follow_target = pointer(
        data, syms[EXTREME_VTABLE] + VTABLE_PREFIX_BYTES + 0x810
    )
    if follow_target != syms[EXTREME_3D]:
        raise ValueError("unexpected HDExtreme mode follow-up target")
    if syms[EXTREME_MODE] != 0x2D2AC:
        raise ValueError("wrong A124 output mode implementation")
    if syms[EXTREME_EDID] != 0x2EC88 or syms[EXTREME_SINK] != 0x2F3FA:
        raise ValueError("wrong A124 sink implementation symbols")
    return {
        "status": "static_output_mode_dispatch_verified",
        "vendor_binary_sha256": digest,
        "device": "BDBD:A124",
        "model_code": "0x36",
        "mode_entry": hex(syms[EXTREME_MODE]),
        "base_mode_target": hex(base_target),
        "base_mode_name": "HDPro::SetVideoOutputMode",
        "follow_up_target": hex(follow_target),
        "follow_up_name": "HDExtreme::AutoSwitch3DPackingFormat",
        "edid_reader": hex(syms[EXTREME_EDID]),
        "sink_capabilities_writer": hex(syms[EXTREME_SINK]),
        "hdmi_phy_off_register_resolved": False,
        "hdmi_tx_electrical_state_observed": False,
        "proprietary_bytes_published": False,
        "live_hardware_access": False,
    }

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("private_original_driver", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(evaluate(args.private_original_driver.read_bytes()),
                         indent=2, sort_keys=True))
        return 0
    except (OSError, ValueError) as exc:
        print("status=unverified reason=" + str(exc), file=sys.stderr)
        return 2

if __name__ == "__main__":
    raise SystemExit(main())
