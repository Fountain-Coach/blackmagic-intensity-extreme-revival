"""Deterministic regression suite for the Fountain virtual video instrument."""
import json
import unittest
import zlib

from simulator.model import (
    DeviceError, MODE_BY_NAME, MODES, VirtualIntensityExtreme, VirtualSource,
    make_yuv422_bars,
)
from simulator.server import frame_to_png


class HardwareContractTests(unittest.TestCase):
    def test_inventory_has_sixteen_modes(self):
        self.assertEqual(len(MODES), 16)
        self.assertEqual(len({m.name for m in MODES}), 16)

    def test_capability_origin_unambiguously_virtual(self):
        d = VirtualIntensityExtreme()
        c = d.capabilities()
        self.assertTrue(c["simulated"])
        self.assertFalse(c["physical_signal_verified"])
        self.assertFalse(c["proprietary_driver_required"])
        self.assertTrue(c["half_duplex"])

    def test_explicit_formats_and_422_bytes(self):
        m = MODE_BY_NAME["720p50"]
        data = make_yuv422_bars(m, 7)
        self.assertEqual(len(data), m.width * m.height * 2)
        self.assertEqual(data[0], 128)
        self.assertEqual(data[:4], bytes((128, 235, 128, 235)))

    def test_frame_pattern_reproducible_but_counter_changes(self):
        m = MODE_BY_NAME["PAL"]
        a = make_yuv422_bars(m, 10)
        b = make_yuv422_bars(m, 10)
        c = make_yuv422_bars(m, 11)
        self.assertEqual(a, b)
        self.assertNotEqual(a, c)
        self.assertEqual(zlib.crc32(a), zlib.crc32(b))

    def test_png_decodes_with_stdlib(self):
        m = MODE_BY_NAME["720p50"]
        image = frame_to_png(make_yuv422_bars(m), m.width, m.height)
        self.assertTrue(image.startswith(b"\x89PNG\r\n\x1a\n"))
        self.assertIn(b"IDAT", image)

    def test_capture_locked_delivers_timestamped_frames(self):
        d = VirtualIntensityExtreme(VirtualSource(mode="1080p30"))
        d.enable_capture("1080p30")
        d.start()
        self.assertTrue(d.signal_locked)
        frames = d.advance(3)
        self.assertEqual(len(frames), 3)
        self.assertEqual([f["frame"] for f in frames], [0, 1, 2])
        self.assertEqual(frames[2]["pts_ns"], 66_666_666)
        self.assertEqual(frames[0]["bytes"], 1920 * 1080 * 2)
        self.assertIn("not captured", frames[0]["provenance"])
        d.stop()
        self.assertEqual(d.snapshot()["activity"], "idle")

    def test_rational_timebase_for_5994(self):
        d = VirtualIntensityExtreme(VirtualSource(mode="720p59.94"))
        d.enable_capture("720p59.94")
        d.start()
        frames = d.advance(3)
        self.assertEqual(frames[1]["pts_ns"], 16_683_333)
        self.assertEqual(frames[2]["pts_ns"], 33_366_666)

    def test_mismatch_produces_no_frames(self):
        d = VirtualIntensityExtreme(VirtualSource(mode="720p60"))
        d.enable_capture("720p50")
        d.start()
        self.assertFalse(d.signal_locked)
        self.assertEqual(d.advance(5), [])
        self.assertEqual(d.dropped, 5)

    def test_disconnected_source_produces_no_frames(self):
        d = VirtualIntensityExtreme(VirtualSource(connected=False))
        d.enable_capture("720p50")
        d.start()
        self.assertFalse(d.signal_locked)
        self.assertEqual(d.advance(3), [])

    def test_protected_source_blocks_capture(self):
        d = VirtualIntensityExtreme(VirtualSource(protected=True))
        d.enable_capture("720p50")
        d.start()
        self.assertFalse(d.signal_locked)
        self.assertEqual(d.advance(3), [])

    def test_dropped_frames_have_gaps_in_logical_time(self):
        d = VirtualIntensityExtreme(VirtualSource(drop_every=3))
        d.enable_capture("720p50")
        d.start()
        frames = d.advance(7)
        self.assertEqual([f["frame"] for f in frames], [0, 1, 3, 4, 6])
        self.assertEqual(d.dropped, 2)

    def test_half_duplex_capture_first(self):
        d = VirtualIntensityExtreme()
        d.enable_capture("1080p25")
        with self.assertRaises(DeviceError) as failure:
            d.enable_playback("1080p25")
        self.assertEqual(failure.exception.code, "0x80000009")
        self.assertEqual(d.activity, "capture")

    def test_half_duplex_playback_first(self):
        d = VirtualIntensityExtreme()
        d.enable_playback("1080p25")
        with self.assertRaises(DeviceError) as failure:
            d.enable_capture("1080p25")
        self.assertEqual(failure.exception.code, "0x80000009")
        self.assertEqual(d.activity, "playback")

    def test_playback_without_source_succeeds(self):
        d = VirtualIntensityExtreme(VirtualSource(connected=False))
        d.enable_playback("720p50")
        d.start()
        self.assertEqual(len(d.advance(2)), 2)
        self.assertEqual(d.last_metadata["direction"], "playback")

    def test_unsupported_mode_and_pixel_format(self):
        d = VirtualIntensityExtreme()
        with self.assertRaises(DeviceError) as failure:
            d.enable_capture("4Kp60")
        self.assertEqual(failure.exception.code, "MODE_UNSUPPORTED")
        with self.assertRaises(DeviceError) as failure:
            d.enable_capture("1080p25", pixel_format="v210")
        self.assertEqual(failure.exception.code, "PIXEL_UNSUPPORTED")

    def test_start_without_enabling_is_invalid(self):
        d = VirtualIntensityExtreme()
        with self.assertRaises(DeviceError) as failure:
            d.start()
        self.assertEqual(failure.exception.code, "NOT_ENABLED")

    def test_invalid_batch_is_rejected(self):
        d = VirtualIntensityExtreme()
        d.enable_capture("720p50")
        d.start()
        for count in (0, 31):
            with self.assertRaises(DeviceError) as failure:
                d.advance(count)
            self.assertEqual(failure.exception.code, "INVALID_COUNT")

    def test_reconfigure_after_stop_clears_previous_frames(self):
        d = VirtualIntensityExtreme()
        d.enable_capture("720p50")
        d.start()
        d.advance(2)
        d.stop()
        d.enable_playback("1080p25")
        self.assertEqual(d.frame_number, 0)
        self.assertIsNone(d.last_frame)


if __name__ == "__main__":
    unittest.main()
