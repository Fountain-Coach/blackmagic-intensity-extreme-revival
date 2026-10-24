"""Synthetic observer log tests; not captured image or physical hardware data."""
import importlib.util
from pathlib import Path
import unittest

MODULE = (Path(__file__).resolve().parents[1] / "scripts" /
          "validate-capture-evidence.py")
spec = importlib.util.spec_from_file_location("validate_capture_evidence", MODULE)
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)

GOOD = """stage=requested_video_mode code=0x48703330
stage=set_callback status=0x00000000
stage=enable_input status=0x00000000
stage=start_streams status=0x00000000
stage=signal_lock rc=0x00000000 locked=0 detected_mode_rc=0x80000001 mode=0x0
stage=signal_lock_final rc=0x00000000 locked=1 detected_mode_rc=0x00000000 mode=0x48703330
stage=counts frames=91 valid=10 no_source=81 format_changes=1 observer_refcount=1
stage=first_valid_frame bytes_ok=1 width=1920 height=1080 row_bytes=3840 sample_size=4096 sample_fnv32=0x9876abcd timestamp_ok=1 time=0 duration=1000 timescale=30000
stage=complete exit=0
"""

class ValidatorTest(unittest.TestCase):
    def good(self, report=GOOD):
        return validator.validate_capture(report, "1080p30")

    def blocked(self, report):
        with self.assertRaises(ValueError):
            self.good(report)

    def test_good_synthetic_frame(self):
        result = self.good()
        self.assertEqual(result["valid"], 10)
        self.assertEqual(result["no_source"], 81)

    def test_source_not_locked(self):
        self.blocked(GOOD.replace("locked=1", "locked=0"))

    def test_no_actual_valid_frames(self):
        self.blocked(GOOD.replace("valid=10", "valid=0"))

    def test_missing_pixels(self):
        self.blocked(GOOD.replace("bytes_ok=1", "bytes_ok=0"))

    def test_missing_timestamp(self):
        self.blocked(GOOD.replace("timestamp_ok=1", "timestamp_ok=0"))

    def test_bad_dimensions(self):
        self.blocked(GOOD.replace("width=1920", "width=1280"))

    def test_missing_complete(self):
        self.blocked(GOOD.replace("stage=complete exit=0", ""))

    def test_bad_mode(self):
        self.blocked(GOOD.replace("code=0x48703330", "code=0x68703630"))

    def test_duplicate_status_is_ambiguous(self):
        self.blocked(GOOD + "stage=signal_lock_final rc=0x00000000 locked=1\n")

    def test_bad_frame_counters(self):
        self.blocked(GOOD.replace("frames=91", "frames=9"))

    def test_missing_hash(self):
        self.blocked(GOOD.replace("sample_fnv32=0x9876abcd",
                                  "sample_fnv32=none"))

    def test_device_provided_no_source_telemetry(self):
        actual = """stage=requested_video_mode code=0x48703330
stage=signal_lock_final rc=0x00000000 locked=0 detected_mode_rc=0x80000001 mode=0x0
stage=counts frames=84 valid=0 no_source=84
stage=first_valid_frame bytes_ok=0 width=0 height=0 row_bytes=0 sample_size=0 sample_fnv32=0x00000000 timestamp_ok=0 time=0 duration=0 timescale=30000
stage=complete exit=0
"""
        self.blocked(actual)

if __name__ == "__main__":
    unittest.main()
