"""Full feature simulator acceptance: no camera, Blackmagic device or HDMI."""
import json
from io import BytesIO
import unittest
from zipfile import ZipFile
from zlib import crc32

from simulator.lab import (
    VideoInstrumentLab, DeviceError, audio_interval, pcm_samples,
    make_packet, make_pattern, validate_mode, SCENARIOS,
    MAX_HISTORY, AUDIO_SAMPLE_RATE
)
from simulator.model import MODE_BY_NAME


class FullSoftwareVideoLabTests(unittest.TestCase):
    def setUp(self):
        self.lab = VideoInstrumentLab()

    def test_no_vendor_dependency_in_capability(self):
        c = self.lab.capabilities()
        self.assertTrue(c["simulated"])
        self.assertFalse(c["physical_device_access"])
        self.assertTrue(c["half_duplex_per_device"])
        self.assertEqual(c["device_count"], 2)
        self.assertEqual(len(c["modes"]), 16)
        self.assertEqual(len(c["scenarios"]), len(SCENARIOS))

    def test_capture_yuv_audio_and_timing(self):
        self.lab.reset("camera", "720p50", pattern="moving")
        s = self.lab.step(3)
        self.assertEqual(s["input"]["delivered"], 3)
        self.assertTrue(s["input"]["signal_locked"])
        frame = self.lab.frame("input")
        self.assertEqual(len(frame.video), 1280*720*2)
        self.assertEqual(frame.metadata["pts_ns"], 40000000)
        self.assertEqual(frame.metadata["frame"], 2)
        self.assertEqual(frame.metadata["audio"]["samples_per_channel"], 960)
        self.assertEqual(frame.metadata["audio"]["bytes"], 960*4)
        self.assertEqual(len(frame.audio), 960*4)
        self.assertEqual(frame.metadata["video_crc32"], "%08x" % crc32(frame.video))
        self.assertFalse(s["physical_device_access"])

    def test_deterministic_camera_pattern(self):
        self.lab.reset("camera", "PAL", pattern="checker")
        self.lab.step(2)
        first = self.lab.frame("input")
        self.lab.reset("camera", "PAL", pattern="checker")
        self.lab.step(2)
        second = self.lab.frame("input")
        self.assertEqual(first.video, second.video)
        self.assertEqual(first.audio, second.audio)
        self.assertEqual(first.metadata, second.metadata)

    def test_patterns_are_distinguishable(self):
        bars = make_pattern("PAL", 1, "bars")
        checker = make_pattern("PAL", 1, "checker")
        moving = make_pattern("PAL", 1, "moving")
        self.assertNotEqual(crc32(bars), crc32(checker))
        self.assertNotEqual(crc32(checker), crc32(moving))
        self.assertEqual(len(bars), len(checker))
        self.assertEqual(len(checker), len(moving))

    def test_camera_off_no_frames(self):
        s = self.lab.reset("camera-off", "720p50")
        self.assertFalse(s["source"]["power"])
        s = self.lab.step(4)
        self.assertFalse(s["input"]["signal_locked"])
        self.assertEqual(s["input"]["delivered"], 0)
        self.assertEqual(s["input"]["dropped"], 4)
        self.assertIsNone(self.lab.frame("input"))

    def test_camera_live_view_software_control(self):
        self.lab.reset()
        self.lab.change_source(live_view=False)
        s = self.lab.step(1)
        self.assertFalse(s["input"]["signal_locked"])
        self.lab.change_source(live_view=True)
        self.assertTrue(self.lab.step(1)["input"]["signal_locked"])

    def test_source_recovers_after_power_on(self):
        self.lab.reset("camera-off")
        self.lab.step(2)
        self.lab.change_source(power=True)
        s = self.lab.step(2)
        self.assertEqual(s["input"]["delivered"], 2)
        self.assertEqual(s["input"]["dropped"], 2)
        self.assertIn("signal_lock", [e["event"] for e in s["events"]])

    def test_disconnected_fault(self):
        self.lab.reset("disconnected")
        self.assertEqual(self.lab.step(3)["input"]["delivered"], 0)

    def test_protected_signal_is_not_captured(self):
        self.lab.reset("protected")
        self.assertEqual(self.lab.step(2)["input"]["delivered"], 0)
        self.assertIsNone(self.lab.frame("input"))

    def test_drop_every_three_preserves_pts_gaps(self):
        self.lab.reset("drops")
        s = self.lab.step(7)
        self.assertEqual(s["input"]["delivered"], 5)
        self.assertEqual(s["input"]["dropped"], 2)
        self.assertEqual([p.metadata["frame"] for p in self.lab.captured],
                         [0, 1, 3, 4, 6])
        self.assertEqual(len(self.lab.captured), 5)

    def test_mode_mismatch_without_detection(self):
        self.lab.reset("mismatch", "720p50", auto_detect="off")
        s = self.lab.step(3)
        self.assertFalse(s["input"]["signal_locked"])
        self.assertEqual(s["input"]["delivered"], 0)

    def test_mode_detection_once(self):
        self.lab.reset("mismatch", "720p50", auto_detect="once")
        s = self.lab.step()
        self.assertTrue(s["input"]["signal_locked"])
        self.assertEqual(s["input"]["mode"], "1080p25")
        self.assertEqual(s["auto_detect"], "off")
        self.assertIn("format_changed", [e["event"] for e in s["events"]])

    def test_mode_detection_continuous(self):
        self.lab.reset("camera", "720p50", auto_detect="continuous")
        self.lab.step(1)
        self.lab.change_source(mode="1080p25")
        self.assertEqual(self.lab.step(1)["input"]["mode"], "1080p25")
        self.lab.change_source(mode="720p50")
        self.assertEqual(self.lab.step(1)["input"]["mode"], "720p50")

    def test_half_duplex_conflict_receiver(self):
        self.lab.reset("camera")
        result = self.lab.deliberate_half_duplex_conflict()
        self.assertTrue(result["blocked"])
        self.assertEqual(result["code"], "0x80000009")
        self.assertTrue(result["state"]["input"]["streaming"])

    def test_half_duplex_conflict_transmitter(self):
        self.lab.reset("playback")
        result = self.lab.deliberate_half_duplex_conflict()
        self.assertTrue(result["blocked"])
        self.assertEqual(result["code"], "0x80000009")

    def test_two_separate_devices_loop_back_exact_bytes(self):
        self.lab.reset("loopback", "720p50", pattern="moving")
        s = self.lab.step(3)
        self.assertTrue(s["input"]["signal_locked"])
        self.assertTrue(s["input"]["streaming"])
        self.assertTrue(s["output"]["streaming"])
        self.assertTrue(s["loopback"]["last_video_audio_pts_identical"])
        self.assertEqual(s["input"]["delivered"], 3)
        self.assertEqual(s["output"]["delivered"], 3)
        self.assertEqual(self.lab.frame("input").video,
                         self.lab.frame("output").video)
        self.assertEqual(self.lab.frame("input").audio,
                         self.lab.frame("output").audio)

    def test_playback_only(self):
        self.lab.reset("playback", "1080p25")
        s = self.lab.step(2)
        self.assertEqual(s["input"]["activity"], "idle")
        self.assertEqual(s["output"]["delivered"], 2)
        self.assertIsNotNone(self.lab.frame("output"))
        self.assertIsNone(self.lab.frame("input"))

    def test_scheduled_output_overrides_auto_pattern(self):
        self.lab.reset("loopback", "PAL", pattern="bars")
        self.lab.queue_output("checker", count=1)
        s = self.lab.step(1)
        self.assertEqual(s["output"]["last_frame"]["pattern"], "checker")
        self.assertEqual(s["input"]["last_frame"]["pattern"], "checker")
        self.assertTrue(s["loopback"]["last_video_audio_pts_identical"])

    def test_scheduled_output_underflow_and_recovery(self):
        self.lab.reset("loopback", "PAL")
        self.lab.change_source(output_auto_fill=False)
        s = self.lab.step(2)
        self.assertEqual(s["output"]["dropped"], 2)
        self.assertEqual(s["input"]["delivered"], 0)
        self.lab.queue_output("checker", 2)
        s = self.lab.step(2)
        self.assertEqual(s["output"]["delivered"], 2)
        self.assertEqual(s["input"]["delivered"], 2)
        self.assertTrue(s["loopback"]["last_video_audio_pts_identical"])

    def test_output_queue_bound(self):
        self.lab.reset("playback")
        self.lab.queue_output("bars", 30)
        for _ in range(3):
            self.lab.queue_output("checker", 30)
        with self.assertRaises(DeviceError) as caught:
            self.lab.queue_output("moving", 1)
        self.assertEqual(caught.exception.code, "OUTPUT_QUEUE_FULL")

    def test_pcm_frames_fractional_rate_no_drift(self):
        starts = [audio_interval("720p59.94", n) for n in range(1001)]
        self.assertEqual(sum(count for _,count in starts),
                         1001*48000*1001//60000)
        self.assertEqual(starts[0][0], 0)
        for i in range(1,len(starts)):
            self.assertEqual(starts[i][0], starts[i-1][0]+starts[i-1][1])

    def test_audio_waveform_is_stereo_and_mutable(self):
        p = make_packet("PAL", 0, "bars", "test", "HDMI")
        muted = make_packet("PAL", 0, "bars", "test", "HDMI", True)
        self.assertNotEqual(p.audio, muted.audio)
        self.assertEqual(len(p.audio), len(muted.audio))
        self.assertEqual(muted.audio, b"\0"*len(muted.audio))

    def test_interlace_declared_not_falsely_emulated(self):
        p = make_packet("1080i50", 0, "bars", "test", "HDMI")
        self.assertTrue(p.metadata["interlaced"])
        self.assertFalse(self.lab.capabilities()["video"]["actual_interlace_fields"])

    def test_composite_rejects_hd_in_test_fixture(self):
        with self.assertRaises(DeviceError) as failure:
            validate_mode("1080p30", "Composite")
        self.assertEqual(failure.exception.code, "CONNECTOR_MODE_UNSUPPORTED")
        validate_mode("PAL", "Composite")

    def test_unknown_source_edit_is_atomic(self):
        before = self.lab.snapshot()
        with self.assertRaises(DeviceError):
            self.lab.change_source(power=False, fake_camera_property="oops")
        self.assertEqual(self.lab.snapshot(), before)

    def test_wrong_typed_options_rejected(self):
        for payload in ({"connected":1},{"drop_every":True},
                        {"pattern":"not-a-pattern"},{"auto_detect":"magic"}):
            with self.assertRaises(DeviceError):
                self.lab.change_source(**payload)

    def test_archive_zip_preserves_exact_video_and_audio(self):
        self.lab.reset("camera", "PAL")
        self.lab.step(2)
        raw = self.lab.archive_zip()
        with ZipFile(BytesIO(raw)) as archive:
            names = archive.namelist()
            self.assertIn("manifest.json", names)
            manifest = json.loads(archive.read("manifest.json"))
            self.assertTrue(manifest["simulated"])
            self.assertFalse(manifest["captured_from_hardware"])
            self.assertEqual(len(manifest["frames"]), 2)
            for packet in self.lab.captured:
                key = "synthetic-%06d" % packet.metadata["frame"]
                self.assertEqual(archive.read(key+".2vuy"),packet.video)
                self.assertEqual(archive.read(key+".pcm"),packet.audio)
        self.assertEqual(raw, self.lab.archive_zip())

    def test_archive_bounded_and_missing(self):
        with self.assertRaises(DeviceError) as failure:
            self.lab.archive_zip()
        self.assertEqual(failure.exception.code,"NO_FRAMES")
        self.lab.reset("camera","PAL")
        self.lab.step(10)
        self.assertEqual(len(self.lab.captured),MAX_HISTORY)

    def test_stop_releases_both_software_devices(self):
        self.lab.reset("loopback")
        self.lab.step(2)
        s=self.lab.stop()
        self.assertFalse(s["input"]["streaming"])
        self.assertFalse(s["output"]["streaming"])
        with self.assertRaises(DeviceError):
            self.lab.step(1)


if __name__ == "__main__":
    unittest.main()
