#!/usr/bin/env python3
"""OFFLINE EXPERIMENT ONLY: produce a byte-guarded CMIO repair candidate.

No vendor binary is distributed by this repository. This tool only accepts the
SHA-256-verified, locally owned 2015 DeckLinkCMIO binary, never installs, loads,
codesigns, or modifies the input. The output WILL invalidate vendor signature.
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import struct
import sys

ORIGINAL_SHA256 = {
    "6f1eda3ee6521dcb4c62df86f4f4e64e7e10a35dd47b39ee97d35472ca469da3": "universal",
    "3774f9acb44c9b4eb909fb4263b0c022aa21d63c550f2f859f5d01a9a78ad577": "x86_64",
}
CPU_TYPE_X86_64 = 0x01000007
PREIMAGE = {0x6352: bytes.fromhex("4489fa"),
            0x6365: bytes.fromhex("0f8442010000")}
# (relative offset, replacement):
REPAIR = {0x6352: bytes.fromhex("89da90"),  # movl %ebx,%edx; nop
          0x6365: bytes.fromhex("0f8e42010000")}  # je -> jle (reject negative HRESULT)


class PatchError(ValueError):
    pass


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def x86_64_slice(data: bytes) -> tuple[int, int]:
    """Return offset and size of the x86_64 slice, from thin or big-endian fat Mach-O."""
    if data[:4] == bytes.fromhex("cffaedfe"):
        cputype = struct.unpack_from("<I", data, 4)[0]
        if cputype != CPU_TYPE_X86_64:
            raise PatchError("Thin Mach-O is not x86_64")
        return 0, len(data)
    if data[:4] != bytes.fromhex("cafebabe"):
        raise PatchError("Not the expected 32-bit fat or thin x86_64 Mach-O")
    count = struct.unpack_from(">I", data, 4)[0]
    if count == 0 or count > 32 or 8 + 20 * count > len(data):
        raise PatchError("Invalid Mach-O fat header")
    for i in range(count):
        cpu, _sub, offset, size, _align = struct.unpack_from(">IIIII", data, 8 + 20 * i)
        if cpu == CPU_TYPE_X86_64:
            if offset <= 0 or size < 0x636b or offset + size > len(data):
                raise PatchError("Invalid x86_64 slice boundaries")
            return offset, size
    raise PatchError("No x86_64 slice in Mach-O")


def patch_bytes(data: bytes) -> tuple[bytes, list[int]]:
    offset, size = x86_64_slice(data)
    for relative, expected in PREIMAGE.items():
        if relative + len(expected) > size or data[offset+relative:offset+relative+len(expected)] != expected:
            raise PatchError("Preimage mismatch at x86_64 offset 0x%x" % relative)
    output = bytearray(data)
    for relative, replacement in REPAIR.items():
        expected = PREIMAGE[relative]
        if len(expected) != len(replacement):
            raise PatchError("Patch length must not change")
        output[offset+relative:offset+relative+len(replacement)] = replacement
    changed = [i for i, (a, b) in enumerate(zip(data, output)) if a != b]
    expected_positions = {offset + i for i in (0x6352, 0x6353, 0x6354, 0x6366)}
    if set(changed) != expected_positions or len(output) != len(data):
        raise PatchError("Unexpected modification scope")
    return bytes(output), changed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Private, locally owned original DeckLinkCMIO binary")
    parser.add_argument("--output", type=Path, help="New offline candidate; NEVER an installed plugin")
    parser.add_argument("--dry-run", action="store_true", help="Validate original and patch without writing")
    args = parser.parse_args()
    if not args.dry_run and args.output is None:
        parser.error("Pass --dry-run or an explicit --output path")
    data = args.input.read_bytes()
    digest = sha(data)
    if digest not in ORIGINAL_SHA256:
        raise PatchError("Unrecognized source SHA-256 %s; refusing experimental patch" % digest)
    candidate, changes = patch_bytes(data)
    print("source_type=%s" % ORIGINAL_SHA256[digest])
    print("source_sha256=%s" % digest)
    print("candidate_sha256=%s" % sha(candidate))
    print("changed_file_offsets=%s" % ",".join("0x%x" % v for v in changes))
    print("IMPORTANT: Experimental, unvalidated runtime behavior; vendor code signature invalid.")
    print("Do not install or use as a CoreMediaIO plugin without controlled compatibility testing.")
    if args.dry_run:
        return 0
    target = args.output.expanduser().resolve()
    original = args.input.expanduser().resolve()
    if target == original:
        raise PatchError("Never overwrite the original")
    if str(target).startswith(("/Library/", "/System/", "/Applications/")):
        raise PatchError("Refusing to write into operating system locations")
    fd = os.open(str(target), os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(candidate)
    except Exception:
        target.unlink(missing_ok=True)
        raise
    print("offline_output=%s" % target)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, PatchError) as exc:
        print("patch_refused=%s" % exc, file=sys.stderr)
        raise SystemExit(1)
