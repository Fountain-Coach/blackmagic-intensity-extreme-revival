"""Browser and machine-readable full software video instrument.

No Blackmagic library import, PCI hardware, real camera, Syphon or HDMI needed.

Run:
    python3 -m simulator.lab_server --host 127.0.0.1 --port 8172
Visit:
    http://127.0.0.1:8172/lab

Default localhost binding is intentional; the API is a synthetic-only,
unauthenticated local test instrument. Explicit private LAN binding is supported
for trusted lab machines, but do not publish it onto the Internet.
"""
from __future__ import annotations

import argparse
import json
from http.server import ThreadingHTTPServer
from pathlib import Path
from threading import RLock
from urllib.parse import urlsplit

from .lab import DeviceError, VideoInstrumentLab
from .server import Handler as LegacyHandler, frame_to_png

LAB = VideoInstrumentLab()
LAB_LOCK = RLock()
PAGE = Path(__file__).with_name("web").joinpath("lab.html")

ENDPOINTS = {
    "/api/lab/configure", "/api/lab/step", "/api/lab/source",
    "/api/lab/queue", "/api/lab/conflict", "/api/lab/stop",
}


class LabHandler(LegacyHandler):
    server_version = "FountainFullVideoSimulation/2.0"

    def do_GET(self):
        path = urlsplit(self.path).path
        if path in ("/lab", "/lab/", "/lab.html"):
            return self._reply(200, PAGE.read_bytes(), "text/html; charset=utf-8")
        if path == "/api/lab/capabilities":
            with LAB_LOCK:
                return self._json(200, LAB.capabilities())
        if path == "/api/lab/state":
            with LAB_LOCK:
                return self._json(200, LAB.snapshot())
        if path == "/api/lab/archive.zip":
            with LAB_LOCK:
                try:
                    data = LAB.archive_zip()
                except DeviceError as e:
                    return self._json(409, {"simulated": True, "error": e.code,
                                            "message": str(e)})
            return self._binary(200, data, "application/zip",
                                "attachment; filename=fountain-SYNTHETIC-frames.zip")
        if path in ("/api/lab/input.png", "/api/lab/output.png",
                    "/api/lab/input.2vuy", "/api/lab/output.2vuy",
                    "/api/lab/input.pcm", "/api/lab/output.pcm"):
            direction = "input" if "/input." in path else "output"
            extension = path.rsplit(".", 1)[-1]
            with LAB_LOCK:
                packet = LAB.frame(direction)
                if packet is None:
                    return self._json(409, {"simulated": True,
                            "error": "NO_VIRTUAL_FRAME",
                            "message": "No active or retained virtual media for "+
                                       direction})
                if extension == "png":
                    data = frame_to_png(packet.video, packet.metadata["width"],
                                        packet.metadata["height"])
                    mime = "image/png"
                elif extension == "pcm":
                    data = packet.audio
                    mime = "audio/L16"  # binary is LE; see metadata below
                else:
                    data = packet.video
                    mime = "application/octet-stream"
                meta = packet.metadata
                headers = {"X-Fountain-Frame": str(meta["frame"]),
                           "X-Fountain-PTS-Ns": str(meta["pts_ns"]),
                           "X-Fountain-Video-CRC32": meta["video_crc32"],
                           "X-Fountain-Provenance": "SYNTHETIC"}
                if extension == "pcm":
                    mime = "application/octet-stream"  # signed PCM16LE, 48kHz stereo
            return self._binary(200, data, mime, None, headers)
        return super().do_GET()

    def _binary(self, code: int, body: bytes, mime: str, disposition=None,
                extra=None):
        self.send_response(code)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("X-Fountain-Simulation", "true")
        if disposition:
            self.send_header("Content-Disposition", disposition)
        for key, value in (extra or {}).items():
            self.send_header(key, value)
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        path = urlsplit(self.path).path
        if path not in ENDPOINTS:
            return super().do_POST()
        # Even on localhost, block cross-origin web pages from changing local
        # simulator state; a future private WLAN bind remains explicit.
        origin = self.headers.get("Origin")
        if origin:
            requested = urlsplit(origin)
            host = self.headers.get("Host", "")
            if (requested.scheme not in ("http", "https") or
                    requested.netloc.lower() != host.lower()):
                return self._json(403, {"simulated": True,
                                        "error": "ORIGIN_FORBIDDEN"})
        if self.headers.get("Content-Type", "").split(";",1)[0].strip() != "application/json":
            return self._json(415, {"error": "JSON_REQUIRED"})
        try:
            length = int(self.headers.get("Content-Length", "-1"))
        except ValueError:
            return self._json(400, {"error": "INVALID_CONTENT_LENGTH"})
        if not 0 <= length <= 8192:
            return self._json(413, {"error": "REQUEST_TOO_LARGE"})
        try:
            payload = json.loads(self.rfile.read(length))
            if not isinstance(payload, dict):
                raise ValueError("Expected an object")
        except (ValueError, UnicodeDecodeError):
            return self._json(400, {"error": "INVALID_JSON"})
        with LAB_LOCK:
            try:
                if path == "/api/lab/configure":
                    unknown = set(payload) - {
                        "scenario", "mode", "source_mode", "connector",
                        "pattern", "auto_detect"}
                    if unknown:
                        raise DeviceError("UNKNOWN_CONTROL", "Unknown configure property")
                    result = LAB.reset(**payload)
                elif path == "/api/lab/step":
                    if set(payload) - {"count"}:
                        raise DeviceError("UNKNOWN_CONTROL", "Unknown step property")
                    result = LAB.step(payload.get("count", 1))
                elif path == "/api/lab/source":
                    result = LAB.change_source(**payload)
                elif path == "/api/lab/queue":
                    if set(payload) - {"pattern", "count"}:
                        raise DeviceError("UNKNOWN_CONTROL", "Unknown queue property")
                    result = LAB.queue_output(**payload)
                elif path == "/api/lab/conflict":
                    if payload:
                        raise DeviceError("UNKNOWN_CONTROL", "No conflict options expected")
                    result = LAB.deliberate_half_duplex_conflict()
                else:
                    if payload:
                        raise DeviceError("UNKNOWN_CONTROL", "No stop options expected")
                    result = LAB.stop()
                return self._json(200, result)
            except DeviceError as e:
                return self._json(409, {"simulated": True, "error": e.code,
                                        "message": str(e), "state": LAB.snapshot()})
            except (TypeError, ValueError) as e:
                return self._json(400, {"simulated": True, "error": "BAD_OPTIONS",
                                        "message": str(e)})


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", default=8172, type=int)
    args = parser.parse_args()
    server = ThreadingHTTPServer((args.host, args.port), LabHandler)
    print("Fountain FULL SOFTWARE VIDEO LAB at http://%s:%d/lab" %
          (args.host, args.port), flush=True)
    print("stage=lab simulated=1 hardware_access=0", flush=True)
    try:
        server.serve_forever(poll_interval=.5)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
