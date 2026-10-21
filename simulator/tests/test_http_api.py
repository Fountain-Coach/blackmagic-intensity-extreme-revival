"""Loopback HTTP tests for the generated video preview."""
import json
import unittest
from http.server import ThreadingHTTPServer
from threading import Thread
from urllib.request import Request, urlopen

import simulator.server as service
from simulator.model import VirtualIntensityExtreme


class PreviewTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), service.Handler)
        cls.url = "http://127.0.0.1:%d" % cls.server.server_port
        cls.thread = Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join(timeout=3)

    def setUp(self):
        with service.LOCK:
            service.DEVICE = VirtualIntensityExtreme()

    def get(self, route):
        with urlopen(self.url + route, timeout=4) as response:
            return response.status, response.read(), dict(response.headers)

    def post(self, route, value):
        data = json.dumps(value).encode()
        request = Request(self.url + route, data=data,
                          headers={"Content-Type": "application/json"}, method="POST")
        with urlopen(request, timeout=5) as response:
            return json.loads(response.read())

    def test_health_explicitly_virtual(self):
        status, data, headers = self.get("/health")
        self.assertEqual(status, 200)
        self.assertEqual(headers.get("X-Fountain-Simulation"), "true")
        self.assertFalse(json.loads(data)["physical_device_access"])

    def test_manifest_has_sixteen_modes(self):
        status, data, _ = self.get("/api/capabilities")
        self.assertEqual(status, 200)
        self.assertEqual(len(json.loads(data)["modes"]), 16)

    def test_generated_frame_has_png_preview(self):
        state = self.post("/api/configure", {"direction": "capture", "mode": "720p50"})
        self.assertTrue(state["signal_locked"])
        state = self.post("/api/step", {"count": 2})
        self.assertEqual(state["frames_delivered"], 3)
        status, image, _ = self.get("/frame.png")
        self.assertEqual(status, 200)
        self.assertTrue(image.startswith(bytes((137, 80, 78, 71, 13, 10, 26, 10))))

    def test_half_duplex_error_is_visible(self):
        self.post("/api/configure", {"direction": "capture", "mode": "1080p25"})
        result = self.post("/api/half-duplex", {})
        self.assertTrue(result["blocked"])
        self.assertEqual(result["code"], "0x80000009")
        self.assertEqual(result["state"]["activity"], "capture")


if __name__ == "__main__":
    unittest.main()
