"""Synthetic tests: never contain or execute vendor software."""
import importlib.util
from pathlib import Path
import struct
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "scripts" / "inspect_private_model_dispatch.py"
spec = importlib.util.spec_from_file_location("inspect_private_model_dispatch", SOURCE)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def synthetic_macho():
    data = bytearray(4096)
    struct.pack_into("<IiiIIIII", data, 0,
                     module.MH_MAGIC_64, module.CPU_X86_64,
                     0, 0, 1, 72, 0, 0)
    struct.pack_into("<II16sQQQQiiII", data, 32,
                     module.LC_SEGMENT_64, 72,
                     b"__DATA".ljust(16, b"\0"),
                     0x100, 0xf00, 0x100, 0xf00,
                     0, 0, 0, 0)
    return data


class PrivateModelDispatchTests(unittest.TestCase):
    def test_synthetic_dispatch_entry(self):
        data = synthetic_macho()
        table_va = 0x200
        entry = table_va + 2 * module.MODEL_TABLE_STRIDE
        struct.pack_into("<H", data, entry, 0xa124)
        data[entry - 2] = 0x36
        mapped = module.translate_va(data, table_va)
        self.assertEqual(mapped, table_va)
        self.assertEqual(module.extract_device_matches(
            data, mapped, 0xa124, count=3), [(2, 0x36)])

    def test_duplicate_match_is_detectable(self):
        data = synthetic_macho()
        for i in (1, 2):
            struct.pack_into("<H", data,
                             0x200 + i * module.MODEL_TABLE_STRIDE, 0xa124)
        matches = module.extract_device_matches(data, 0x200, 0xa124, count=3)
        self.assertEqual(len(matches), 2)

    def test_out_of_bounds_table_fails(self):
        data = synthetic_macho()
        with self.assertRaisesRegex(ValueError, "exceeds file"):
            module.extract_device_matches(data, len(data) - 1, 0xa124)

    def test_unsupported_binary_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "Unsupported binary SHA"):
            module.analyze_private_copy(synthetic_macho(), 0xa124)


if __name__ == "__main__":
    unittest.main()
