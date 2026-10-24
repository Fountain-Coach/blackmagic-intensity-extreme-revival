"""Synthetic metadata fixtures for the private exact-HDMI verifier only.

These tests DO NOT make a simulated source pass hardware acceptance.
The actual Sierra gate remains conditioned on original BMD visl=true and
real borrowed UYVY pixels captured from the fixed HDMI physical fixture.
"""
import hashlib
import importlib.util
import os
from pathlib import Path
import stat
import tempfile
import unittest


class PrivateRoundTripVerifierTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix="fc-hdmi-verify-test-")
        base=Path(self.tmp.name)
        self.folder=base / "recording-SYNTHETIC_TEST"
        self.folder.mkdir(mode=0o700)
        self.log=base/"vendor-output.log"
        self.log.write_text(
            "stage=replay result=api_accepted recorded_frames=30 submitted=60 "
            "byte_identical_output_buffers=60 visual_monitor_witness=NOT_MEASURED\n"
            "stage=roundtrip result=DIGITAL_ACCEPTED\n")
        self.log.chmod(0o600)
        spec=importlib.util.spec_from_file_location(
            "fc_roundtrip_verify",
            str(Path(__file__).resolve().parents[1] /
                "tools/sierra/verify_hdmi_roundtrip_recording.py"))
        self.module=importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.module)
        self.module.BASE=str(base)
        self.module.FRAME_BYTES=16
        lines=["index\twidth\theight\trow_bytes\thost_monotonic_ns\tbytes\tsha256"]
        for n in range(30):
            raw=(bytes([n])*16)
            fp=self.folder/("frame-%02d.uyvy"%n)
            fp.write_bytes(raw)
            fp.chmod(0o600)
            digest=hashlib.sha256(raw).hexdigest()
            lines.append("%d\t1920\t1080\t3840\t%d\t16\t%s"%
                         (n,1000000+n*33333333,digest))
        manifest=self.folder/"manifest.tsv"
        manifest.write_text("\n".join(lines)+"\n")
        manifest.chmod(0o600)

    def tearDown(self):
        self.tmp.cleanup()

    def test_complete_digital_verifier_success_not_optical(self):
        self.assertEqual(self.module.verify(str(self.folder),str(self.log)),0)
        result=self.folder/"independent-digital-verification.json"
        import json
        data=json.loads(result.read_text())
        self.assertEqual(data["captured_frames"],30)
        self.assertEqual(data["output_submissions_api_accepted"],60)
        self.assertFalse(data["physical_monitor_E2E_accepted"])
        self.assertFalse(data["monitor_optical_pixels_recorded"])
        self.assertEqual(stat.S_IMODE(result.stat().st_mode),0o600)

    def test_corruption_fails_closed(self):
        fp=self.folder/"frame-09.uyvy"
        fp.write_bytes(b"X"*16)
        with self.assertRaisesRegex(ValueError,"raw_frame_hash_mismatch"):
            self.module.verify(str(self.folder),str(self.log))

    def test_missing_vendor_ack_fails_closed(self):
        self.log.write_text("stage=replay result=unverified\n")
        with self.assertRaisesRegex(ValueError,"missing_actual_60_frame_vendor_replay_ack"):
            self.module.verify(str(self.folder),str(self.log))

    def test_changed_frame_count_fails_closed(self):
        (self.folder/"frame-29.uyvy").unlink()
        with self.assertRaisesRegex(ValueError,"raw_frame_missing"):
            self.module.verify(str(self.folder),str(self.log))

if __name__=="__main__":
    unittest.main()
