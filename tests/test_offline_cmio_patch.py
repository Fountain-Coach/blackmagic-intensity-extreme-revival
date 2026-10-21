"""Tests use synthetic Mach-O structures, NEVER proprietary vendor binaries."""
import struct
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from offline_cmio_patch_candidate import (
    CPU_TYPE_X86_64, PatchError, PREIMAGE, patch_bytes, x86_64_slice,
)


def thin_fixture() -> bytes:
    b = bytearray(0x7000)
    b[:4] = bytes.fromhex("cffaedfe")
    struct.pack_into("<I", b, 4, CPU_TYPE_X86_64)
    for relative, preimage in PREIMAGE.items():
        b[relative:relative + len(preimage)] = preimage
    return bytes(b)


def fat_fixture() -> bytes:
    thin = thin_fixture()
    offset = 0x1000
    header = (bytes.fromhex("cafebabe") + struct.pack(">I", 1) +
              struct.pack(">IIIII", CPU_TYPE_X86_64, 3, offset, len(thin), 12))
    return header + b"\0" * (offset - len(header)) + thin


class OfflinePatchTests(unittest.TestCase):
    def test_thin_byte_exact_guarded_patch(self):
        original = thin_fixture()
        patched, modified = patch_bytes(original)
        self.assertEqual(len(patched), len(original))
        self.assertEqual(modified, [0x6352, 0x6353, 0x6354, 0x6366])
        self.assertEqual(patched[0x6352:0x6355], bytes.fromhex("89da90"))
        self.assertEqual(patched[0x6365:0x636b], bytes.fromhex("0f8e42010000"))
        self.assertEqual(original[0x6352:0x6355], bytes.fromhex("4489fa"))

    def test_universal_binary_preserves_unrelated_data(self):
        original = fat_fixture()
        offset, size = x86_64_slice(original)
        self.assertEqual(offset, 0x1000)
        self.assertEqual(size, 0x7000)
        patched, locations = patch_bytes(original)
        self.assertEqual(locations, [offset + p for p in (0x6352, 0x6353, 0x6354, 0x6366)])
        self.assertEqual(patched[:offset], original[:offset])
        self.assertEqual(patched[offset+0x6352:offset+0x6355], bytes.fromhex("89da90"))

    def test_preimage_mismatch_fails_closed(self):
        b = bytearray(thin_fixture())
        b[0x6353] = 0
        with self.assertRaisesRegex(PatchError, "Preimage mismatch"):
            patch_bytes(bytes(b))

    def test_unsupported_magic_fails_closed(self):
        with self.assertRaises(PatchError):
            x86_64_slice(b"not Mach-O")

    def test_wrong_architecture_fails_closed(self):
        b = bytearray(thin_fixture())
        struct.pack_into("<I", b, 4, 0x0100000c)
        with self.assertRaises(PatchError):
            x86_64_slice(bytes(b))

    def test_already_patched_fails_closed(self):
        original = thin_fixture()
        patched, _ = patch_bytes(original)
        with self.assertRaises(PatchError):
            patch_bytes(patched)


if __name__ == "__main__":
    unittest.main()
