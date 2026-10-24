"""Real HTTP tests of the full software-only video lab, no device access."""
from io import BytesIO
import json
from http.server import ThreadingHTTPServer
from threading import Thread
import unittest
from urllib.error import HTTPError
from urllib.request import Request, urlopen
from zipfile import ZipFile

import simulator.lab_server as service
from simulator.lab import VideoInstrumentLab


class VirtualLabHTTPTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), service.LabHandler)
        cls.url = "http://127.0.0.1:%d" % cls.server.server_port
        cls.thread = Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join(timeout=3)

    def setUp(self):
        with service.LAB_LOCK:
            service.LAB = VideoInstrumentLab()

    def req(self, path, payload=None, headers=None):
        hs = headers or {}
        if payload is not None:
            data = json.dumps(payload).encode("utf-8")
            hs = {"Content-Type": "application/json", **hs}
            method = "POST"
        else:
            data = None
            method = "GET"
        request = Request(self.url + path, data=data, method=method, headers=hs)
        try:
            with urlopen(request, timeout=8) as response:
                return response.status, response.read(), dict(response.headers)
        except HTTPError as error:
            return error.code, error.read(), dict(error.headers)

    def getjson(self, path):
        status, data, headers = self.req(path)
        return status, json.loads(data), headers

    def postjson(self, path, payload):
        status, data, headers = self.req(path, payload)
        return status, json.loads(data), headers

    def test_dashboard_renders_without_camera(self):
        code, html, h = self.req("/lab")
        self.assertEqual(code, 200)
        self.assertIn(b"SIMULATED ONLY", html)
        self.assertIn(b"Input / virtual receiver", html)
        self.assertIn(b"Output / virtual transmitter", html)
        self.assertEqual(h.get("X-Fountain-Simulation"), "true")

    def test_capabilities_are_explicitly_virtual(self):
        code, caps, _ = self.getjson("/api/lab/capabilities")
        self.assertEqual(code, 200)
        self.assertFalse(caps["physical_device_access"])
        self.assertIn("two-device-loopback", caps["behaviors"])
        self.assertEqual(len(caps["modes"]), 16)
        self.assertIn("firmware", " ".join(caps["not_emulated"]))

    def test_two_device_loopback_transfers_real_generated_bytes(self):
        status, s, _ = self.postjson("/api/lab/configure",
                                   {"scenario":"loopback","mode":"720p50"})
        self.assertEqual(status, 200)
        self.assertEqual(s["device_instances"], 2)
        code, s, _ = self.postjson("/api/lab/step", {"count":3})
        self.assertEqual(code, 200)
        self.assertEqual(s["input"]["delivered"], 3)
        self.assertEqual(s["output"]["delivered"], 3)
        self.assertTrue(s["loopback"]["last_video_audio_pts_identical"])
        for endpoint in ("/api/lab/input.png","/api/lab/output.png"):
            code, blob, headers = self.req(endpoint)
            self.assertEqual(code, 200)
            self.assertTrue(blob.startswith(b"\x89PNG\r\n\x1a\n"))
            self.assertEqual(headers.get("X-Fountain-Simulation"), "true")
        _, incoming, ih = self.req("/api/lab/input.2vuy")
        _, outgoing, oh = self.req("/api/lab/output.2vuy")
        self.assertEqual(incoming, outgoing)
        self.assertEqual(len(incoming), 1280*720*2)
        self.assertEqual(ih.get("X-Fountain-PTS-Ns"), oh.get("X-Fountain-PTS-Ns"))
        self.assertEqual(ih.get("X-Fountain-Video-CRC32"),
                         oh.get("X-Fountain-Video-CRC32"))
        _, audio_in, _ = self.req("/api/lab/input.pcm")
        _, audio_out, _ = self.req("/api/lab/output.pcm")
        self.assertEqual(audio_in, audio_out)
        self.assertEqual(len(audio_in), 960*4)

    def test_camera_off_then_software_power_on(self):
        self.postjson("/api/lab/configure", {"scenario":"camera-off"})
        _, s, _ = self.postjson("/api/lab/step", {"count":2})
        self.assertEqual(s["input"]["delivered"], 0)
        code, e, _ = self.req("/api/lab/input.png")
        self.assertEqual(code, 409)
        self.assertEqual(json.loads(e)["error"],"NO_VIRTUAL_FRAME")
        _, s, _ = self.postjson("/api/lab/source", {"power":True})
        _, s, _ = self.postjson("/api/lab/step", {"count":1})
        self.assertTrue(s["input"]["signal_locked"])
        self.assertEqual(s["input"]["delivered"], 1)

    def test_loss_invalidates_input_frame_immediately(self):
        self.postjson("/api/lab/configure", {"scenario":"camera"})
        self.postjson("/api/lab/step", {"count":1})
        self.assertEqual(self.req("/api/lab/input.png")[0],200)
        _, s, _ = self.postjson("/api/lab/source", {"connected":False})
        self.assertFalse(s["input"]["signal_locked"])
        self.assertEqual(self.req("/api/lab/input.png")[0],409)

    def test_auto_detect_hot_swap_and_metadata(self):
        self.postjson("/api/lab/configure",
                      {"scenario":"camera","mode":"720p50",
                       "auto_detect":"continuous"})
        self.postjson("/api/lab/source", {"mode":"1080i50"})
        _, state, _ = self.postjson("/api/lab/step", {"count":1})
        self.assertEqual(state["input"]["mode"],"1080i50")
        self.assertTrue(state["input"]["last_frame"]["interlaced"])
        self.assertIn("format_changed", [e["event"] for e in state["events"]])

    def test_injected_output_underflow_recovery(self):
        self.postjson("/api/lab/configure", {"scenario":"loopback","mode":"PAL"})
        self.postjson("/api/lab/source", {"output_auto_fill":False})
        _, s, _ = self.postjson("/api/lab/step", {"count":3})
        self.assertEqual(s["output"]["dropped"],3)
        self.assertEqual(s["input"]["delivered"],0)
        self.postjson("/api/lab/queue", {"pattern":"checker","count":5})
        _, s, _ = self.postjson("/api/lab/step", {"count":5})
        self.assertEqual(s["input"]["delivered"],5)
        self.assertEqual(s["output"]["last_frame"]["pattern"],"checker")
        self.assertTrue(s["loopback"]["last_video_audio_pts_identical"])

    def test_downloadable_archive_contains_exact_yuv_and_pcm(self):
        self.postjson("/api/lab/configure", {"scenario":"camera","mode":"PAL"})
        self.postjson("/api/lab/step", {"count":2})
        code, archive, headers = self.req("/api/lab/archive.zip")
        self.assertEqual(code,200)
        self.assertEqual(headers.get("X-Fountain-Simulation"), "true")
        with ZipFile(BytesIO(archive)) as f:
            manifest=json.loads(f.read("manifest.json"))
            self.assertFalse(manifest["captured_from_hardware"])
            self.assertEqual(len(manifest["frames"]),2)
            for m in manifest["frames"]:
                root="synthetic-%06d" % m["frame"]
                self.assertEqual(len(f.read(root+".2vuy")),m["video_bytes"])
                self.assertEqual(len(f.read(root+".pcm")),m["audio"]["bytes"])

    def test_legacy_simulation_still_works(self):
        code, health, _ = self.getjson("/health")
        self.assertEqual(code,200)
        self.assertFalse(health["physical_device_access"])
        code, cap, _ = self.getjson("/api/capabilities")
        self.assertEqual(code,200)
        self.assertEqual(len(cap["modes"]),16)

    def test_cross_origin_state_mutation_blocked(self):
        code, data, _ = self.postjson("/api/lab/step", {"count":1})
        self.assertEqual(code,200)
        headers={"Origin":"https://attacker.example"}
        code, err, _ = self.req("/api/lab/step", {"count":1}, headers)
        self.assertEqual(code,403)
        self.assertEqual(json.loads(err)["error"],"ORIGIN_FORBIDDEN")

    def test_http_error_status_correct(self):
        code, err, _=self.req("/api/lab/input.png")
        self.assertEqual(code,409)
        self.assertEqual(json.loads(err)["error"],"NO_VIRTUAL_FRAME")
        code, err, _ = self.postjson("/api/lab/step", {"count":0})
        self.assertEqual(code,409)
        self.assertEqual(err["error"],"INVALID_COUNT")
        code, err, _ = self.postjson("/api/lab/source", {"virtual_pci":123})
        self.assertEqual(code,409)
        self.assertEqual(err["error"],"UNKNOWN_CONTROL")

    def test_conflict_and_stop(self):
        _, s, _ = self.postjson("/api/lab/configure", {"scenario":"playback"})
        code, result, _ = self.postjson("/api/lab/conflict", {})
        self.assertEqual(code,200)
        self.assertTrue(result["blocked"])
        code, s, _ = self.postjson("/api/lab/stop", {})
        self.assertEqual(code,200)
        self.assertFalse(s["output"]["streaming"])


    def test_output_url_blocks_stale_frames_after_underflow(self):
        self.postjson("/api/lab/configure", {"scenario":"loopback","mode":"PAL"})
        self.postjson("/api/lab/step", {"count":1})
        self.assertEqual(self.req("/api/lab/output.png")[0], 200)
        self.postjson("/api/lab/source", {"output_auto_fill":False})
        self.postjson("/api/lab/step", {"count":1})
        self.assertEqual(self.req("/api/lab/output.png")[0], 409)
        self.postjson("/api/lab/queue", {"count":1,"pattern":"moving"})
        self.postjson("/api/lab/step", {"count":1})
        self.assertEqual(self.req("/api/lab/output.png")[0], 200)


if __name__ == "__main__":
    unittest.main()
