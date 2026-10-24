"""Synthetic display-inventory tests; no hardware or user EDID data."""
import importlib.util
from pathlib import Path
import unittest

SCRIPT = (Path(__file__).resolve().parents[1] / "scripts" /
          "hdmi-source-preflight.py")
spec = importlib.util.spec_from_file_location("hdmi_source_preflight", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def d(name, timing, online=True):
    return {"name": name, "timing": timing, "online": online}


class HDMIInputPreflightTests(unittest.TestCase):
    def test_matching_bmd_1080p30(self):
        self.assertEqual(module.evaluate([
            d("Color LCD", ""),
            d("BMD HDMI", "1920 x 1080 @ 30.00Hz")], "1080p30"), 0)

    def test_known_wrong_monitor_must_fail(self):
        self.assertEqual(module.evaluate([
            d("Color LCD", ""),
            d("U28E590", "1920 x 1080 @ 30.00Hz")], "1080p30"), 2)

    def test_wrong_bmd_timing_must_fail(self):
        self.assertEqual(module.evaluate([
            d("BMD HDMI", "1920 x 1080 @ 60.00Hz")], "1080p30"), 3)

    def test_no_mode_reported_is_not_verified(self):
        self.assertEqual(module.evaluate([d("BMD HDMI", "")], "1080p30"), 3)

    def test_offline_bmd_sink_is_not_verified(self):
        self.assertEqual(module.evaluate([
            d("BMD HDMI", "1920 x 1080 @ 30.00Hz", False)], "1080p30"), 2)

    def test_wrong_refresh_rate_is_not_rounded(self):
        self.assertEqual(module.evaluate([
            d("BMD HDMI", "1920 x 1080 @ 29.97Hz")], "1080p30"), 3)

    def test_valid_720p60(self):
        self.assertEqual(module.evaluate([
            d("BMD HDMI", "1280 x 720 @ 60 Hz")], "720p60"), 0)

    def test_same_name_ambiguous_copies(self):
        self.assertEqual(module.evaluate([
            d("BMD HDMI", "1920 x 1080 @ 50 Hz"),
            d("BMD HDMI", "1920 x 1080 @ 30 Hz")], "1080p30"), 4)

    def test_nested_system_profile_format(self):
        fixture = [{"_dataType": "SPDisplaysDataType", "_items": [
            {"_name": "GPU", "spdisplays_ndrvs": [
                {"_name": "BMD HDMI",
                 "spdisplays_resolution": "1920 x 1080 @ 30.00Hz",
                 "spdisplays_online": "spdisplays_yes"},
                {"_name": "U28E590",
                 "spdisplays_resolution": "1920 x 1080 @ 50 Hz",
                 "spdisplays_online": "spdisplays_yes"}]}]}]
        actual = module.flatten_displays(fixture)
        self.assertEqual(len(actual), 2)
        self.assertEqual(actual[0]["name"], "BMD HDMI")
        self.assertEqual(module.evaluate(actual, "1080p30"), 0)

    def test_empty_inventory_not_a_source(self):
        self.assertEqual(module.flatten_displays([]), [])
        self.assertEqual(module.evaluate([], "1080p30"), 2)


if __name__ == "__main__":
    unittest.main()
