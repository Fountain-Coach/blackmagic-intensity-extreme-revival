#!/usr/bin/env python3
"""G1 A124 static original-output dispatch provenance tests.

No vendor executable is shipped to Git. The genuine private binary is
loaded only from the local analysis host when --original-driver is supplied.
All mutations are made in memory, never on the original file.
"""
import argparse
import hashlib
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import inspect_private_output_dispatch as probe

ORIGINAL = None


class A124StaticOutputTests(unittest.TestCase):
    def test_reject_empty_macho(self):
        with self.assertRaises(ValueError):
            probe.all_symbols(b"")

    def test_reject_wrong_magic(self):
        with self.assertRaises(ValueError):
            probe.all_symbols(bytes(64))

    def test_reject_changed_original_hash(self):
        with self.assertRaises(ValueError):
            probe.evaluate(b"not the authorised driver")

    def test_exact_vtable_names(self):
        self.assertIn("HardwareAbstractionDeckLinkHDExtreme",
                      probe.EXTREME_VTABLE)
        self.assertIn("HDProPCIExpress", probe.HD_VTABLE)
        self.assertNotEqual(probe.EXTREME_VTABLE, probe.HD_VTABLE)

    def test_private_binary_required_for_real_dispatch_proof(self):
        if ORIGINAL is None:
            self.skipTest("requires --original-driver local path")
        result = probe.evaluate(ORIGINAL)
        self.assertEqual(result["status"],
                         "static_output_mode_dispatch_verified")
        self.assertEqual(result["device"], "BDBD:A124")
        self.assertEqual(result["vendor_binary_sha256"],
                         probe.ORIGINAL_SHA256)
        self.assertEqual(result["base_mode_target"], "0x1846e")
        self.assertEqual(result["root_mode_target"], "0x103ce")
        self.assertEqual(result["conversion_bits_target"], "0x32140")
        self.assertEqual(result["follow_up_target"], "0x32404")
        self.assertFalse(result["hdmi_phy_off_register_resolved"])
        self.assertFalse(result["live_hardware_access"])

    def test_tampering_rejected(self):
        if ORIGINAL is None:
            self.skipTest("requires --original-driver local path")
        tampered = bytearray(ORIGINAL)
        tampered[len(tampered)//2] ^= 1
        with self.assertRaisesRegex(ValueError, "SHA-256"):
            probe.evaluate(bytes(tampered))

    def test_unchanged_private_driver_digest(self):
        if ORIGINAL is None:
            self.skipTest("requires --original-driver local path")
        self.assertEqual(hashlib.sha256(ORIGINAL).hexdigest(),
                         probe.ORIGINAL_SHA256)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--original-driver", type=Path)
    args, rest = ap.parse_known_args()
    if args.original_driver is not None:
        ORIGINAL = args.original_driver.read_bytes()
    unittest.main(argv=[sys.argv[0]] + rest)
