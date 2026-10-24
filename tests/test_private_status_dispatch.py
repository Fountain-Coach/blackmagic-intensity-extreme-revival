"""Synthetic Mach-O fixtures only; no private driver bytes are committed."""
import importlib.util
from pathlib import Path
import struct
import sys
import unittest

SOURCE = (Path(__file__).resolve().parents[1] / "scripts" /
          "inspect_private_status_dispatch.py")
sys.path.insert(0, str(SOURCE.parent))
spec = importlib.util.spec_from_file_location("inspect_private_status_dispatch", SOURCE)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def synthetic_macho():
    data = bytearray(0x3000)
    # Header and two load commands: __DATA and LC_SYMTAB.
    struct.pack_into("<IiiIIIII", data, 0,
                     0xFEEDFACF, 0x01000007, 0, 0, 2, 96, 0, 0)
    struct.pack_into("<II16sQQQQiiII", data, 32,
                     0x19, 72, b"__DATA".ljust(16, b"\0"),
                     0x1000, 0x1000, 0x1000, 0x1000,
                     0, 0, 0, 0)
    names = [module.VTABLE_SYMBOL, module.READ_SYMBOL, module.WRITE_SYMBOL,
             module.FIRMWARE_SYMBOL]
    strings = b"\0"
    name_offsets = []
    for name in names:
        name_offsets.append(len(strings))
        strings += name.encode("ascii") + b"\0"
    values = [0x1100, 0x2300, 0x2400, 0x2500]
    struct.pack_into("<IIIIII", data, 104, module.LC_SYMTAB, 24,
                     0x300, len(names), 0x400, len(strings))
    for i, (name_offset, value) in enumerate(zip(name_offsets, values)):
        struct.pack_into("<IBBHQ", data, 0x300 + i * 16,
                         name_offset, 0x0f, 1, 0, value)
    data[0x400:0x400 + len(strings)] = strings
    struct.pack_into("<Q", data, 0x1100 + 16 + module.READ_SLOT_BYTES, 0x2300)
    struct.pack_into("<Q", data, 0x1100 + 16 + module.WRITE_SLOT_BYTES, 0x2400)
    struct.pack_into("<Q", data, 0x1100 + 16 + module.FIRMWARE_SLOT_BYTES, 0x2500)
    data[0x1800 + module.A124_MODEL_CODE * 16] = 0x0b
    return data


class HDExtremeDispatchTests(unittest.TestCase):
    def test_symbols_and_vtable_offsets(self):
        data = synthetic_macho()
        symbols = module.symbol_addresses(data)
        self.assertEqual(symbols[module.VTABLE_SYMBOL], 0x1100)
        self.assertEqual(module.read_slot_target(
            data, symbols[module.VTABLE_SYMBOL], module.READ_SLOT_BYTES), 0x2300)
        self.assertEqual(module.read_slot_target(
            data, symbols[module.VTABLE_SYMBOL], module.WRITE_SLOT_BYTES), 0x2400)

    def test_firmware_version_selector_and_table_byte(self):
        data = synthetic_macho()
        self.assertEqual(module.read_slot_target(
            data, 0x1100, module.FIRMWARE_SLOT_BYTES), 0x2500)
        self.assertEqual(module.expected_firmware_version(
            data, 0x1800, module.A124_MODEL_CODE), 0x0b)

    def test_invalid_model_code_rejected(self):
        data = synthetic_macho()
        with self.assertRaisesRegex(ValueError, "Model code out"):
            module.expected_firmware_version(data, 0x1800, 0x4d)

    def test_independent_read_and_write_slots(self):
        data = synthetic_macho()
        struct.pack_into("<Q", data, 0x1100 + 16 + module.READ_SLOT_BYTES, 0x2400)
        self.assertNotEqual(module.read_slot_target(
            data, 0x1100, module.READ_SLOT_BYTES),
            module.symbol_addresses(data)[module.READ_SYMBOL])

    def test_bad_symbol_table_bounds(self):
        data = synthetic_macho()
        struct.pack_into("<I", data, 104 + 12, 99999999)
        with self.assertRaisesRegex(ValueError, "Symbol table outside"):
            module.symbol_addresses(data)

    def test_bad_vtable_slot_location(self):
        data = synthetic_macho()
        with self.assertRaises(ValueError):
            module.read_slot_target(data, 0x2900, module.READ_SLOT_BYTES)

    def test_missing_symtab(self):
        data = synthetic_macho()
        struct.pack_into("<I", data, 104, 0x99)
        with self.assertRaisesRegex(ValueError, "No Mach-O symbol table"):
            module.symbol_addresses(data)

    def test_unsupported_binary_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "Unsupported binary SHA"):
            module.analyze_private_copy(synthetic_macho())


if __name__ == "__main__":
    unittest.main()
