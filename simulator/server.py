"""LAN preview and reproducible virtual capture API. No hardware is touched.

Usage: python3 -m simulator.server --host 127.0.0.1 --port 8172
The default loopback bind is deliberate; explicitly select a LAN host to share.
"""
from __future__ import annotations

import argparse
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from threading import RLock
import struct
import zlib

from .model import DeviceError, VirtualIntensityExtreme, VirtualSource, MODE_BY_NAME

DEVICE = VirtualIntensityExtreme()
LOCK = RLock()
PAGE = Path(__file__).with_name("web").joinpath("index.html")


def _chunk(tag: bytes, payload: bytes) -> bytes:
    return (struct.pack(">I", len(payload)) + tag + payload +
            struct.pack(">I", zlib.crc32(tag + payload) & 0xffffffff))


def _clip(value: int) -> int:
    return max(0, min(255, value))


def frame_to_png(frame: bytes, width: int, height: int, stride: int = 4) -> bytes:
    """PNG thumbnail derived from the actual virtual YUV422 frame."""
    w = (width + stride - 1) // stride
    h = (height + stride - 1) // stride
    lines = []
    for oy in range(h):
        y = min(height - 1, oy * stride)
        pixels = bytearray(b"\0")
        for ox in range(w):
            x = min(width - 1, ox * stride)
            offset = y * width * 2 + (x // 2) * 4
            cb = frame[offset] - 128
            yy = frame[offset + 1 + 2 * (x % 2)] - 16
            cr = frame[offset + 2] - 128
            r = _clip((298 * yy + 409 * cr + 128) >> 8)
            g = _clip((298 * yy - 100 * cb - 208 * cr + 128) >> 8)
            b = _clip((298 * yy + 516 * cb + 128) >> 8)
            pixels.extend((r, g, b))
        lines.append(bytes(pixels))
    encoded = zlib.compress(b"".join(lines), 6)
    signature = b"\x89PNG\r\n\x1a\n"
    return (signature + _chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
            _chunk(b"IDAT", encoded) + _chunk(b"IEND", b""))


class Handler(BaseHTTPRequestHandler):
    server_version = "FountainVirtualVideo/1.0"

    def _reply(self, code: int, body: bytes, mime: str) -> None:
        self.send_response(code)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("X-Fountain-Simulation", "true")
        self.end_headers()
        self.wfile.write(body)

    def _json(self, code: int, data: dict) -> None:
        self._reply(code, json.dumps(data, separators=(",", ":")).encode("utf-8"),
                    "application/json; charset=utf-8")

    def do_GET(self) -> None:
        path = self.path.split("?", 1)[0]
        if path in ("/", "/index.html"):
            self._reply(200, PAGE.read_bytes(), "text/html; charset=utf-8")
            return
        if path == "/api/capabilities":
            self._json(200, DEVICE.capabilities())
            return
        if path == "/api/state":
            with LOCK:
                self._json(200, DEVICE.snapshot())
            return
        if path == "/frame.png":
            with LOCK:
                frame = DEVICE.last_frame
                mode = DEVICE.last_metadata
                lock = DEVICE.signal_locked
                activity = DEVICE.activity
                if frame is None or mode is None or (
                        activity == "capture" and not lock):
                    self._json(409, {"simulated": True, "reason": "no_virtual_frame"})
                    return
                image = frame_to_png(frame, mode["width"], mode["height"])
            self._reply(200, image, "image/png")
            return
        if path == "/health":
            self._json(200, {"status": "ok", "simulated": True,
                             "physical_device_access": False})
            return
        self._json(404, {"error": "not_found"})

    def do_POST(self) -> None:
        length = int(self.headers.get("Content-Length", "0"))
        if not 0 <= length <= 8192:
            self._json(413, {"error": "request_too_large"})
            return
        try:
            request = json.loads(self.rfile.read(length) or b"{}")
            if not isinstance(request, dict):
                raise ValueError("A JSON object is required")
        except (ValueError, UnicodeDecodeError):
            self._json(400, {"error": "invalid_json"})
            return
        path = self.path.split("?", 1)[0]
        with LOCK:
            try:
                if path == "/api/configure":
                    mode = request.get("mode", "720p50")
                    source_mode = request.get("source_mode", mode)
                    direction = request.get("direction", "capture")
                    connected = request.get("connected", True)
                    protected = request.get("protected", False)
                    drop_every = request.get("drop_every", 0)
                    if not isinstance(connected, bool) or not isinstance(protected, bool):
                        raise DeviceError("BAD_OPTIONS", "Connected/protected must be Boolean")
                    if not isinstance(drop_every, int) or not (0 <= drop_every <= 120):
                        raise DeviceError("BAD_OPTIONS", "drop_every must be in 0..120")
                    if mode not in MODE_BY_NAME or source_mode not in MODE_BY_NAME:
                        raise DeviceError("MODE_UNSUPPORTED", "Unknown source or capture mode")
                    if direction not in ("capture", "playback"):
                        raise DeviceError("DIRECTION_UNSUPPORTED", "Use capture or playback")
                    DEVICE.stop()
                    DEVICE.source = VirtualSource(source_mode, connected, protected, drop_every)
                    if direction == "capture":
                        DEVICE.enable_capture(mode)
                    else:
                        DEVICE.enable_playback(mode)
                    DEVICE.start()
                    DEVICE.advance(1)
                    self._json(200, DEVICE.snapshot())
                    return
                if path == "/api/step":
                    DEVICE.advance(request.get("count", 1))
                    self._json(200, DEVICE.snapshot())
                    return
                if path == "/api/stop":
                    DEVICE.stop()
                    self._json(200, DEVICE.snapshot())
                    return
                if path == "/api/half-duplex":
                    try:
                        if DEVICE.activity == "capture":
                            DEVICE.enable_playback(DEVICE.mode.name)
                        elif DEVICE.activity == "playback":
                            DEVICE.enable_capture(DEVICE.mode.name)
                        else:
                            raise DeviceError("NOT_ENABLED", "First start a mode")
                    except DeviceError as error:
                        self._json(200, {"simulated": True, "blocked": True,
                                         "code": error.code, "message": str(error),
                                         "state": DEVICE.snapshot()})
                        return
                    self._json(500, {"error": "half_duplex_not_enforced"})
                    return
                self._json(404, {"error": "not_found"})
            except DeviceError as error:
                self._json(409, {"simulated": True, "error": error.code,
                                 "message": str(error), "state": DEVICE.snapshot()})
            except (TypeError, ValueError) as error:
                self._json(400, {"error": "bad_request", "message": str(error)})

    def log_message(self, format: str, *args) -> None:
        print("%s - %s" % (self.address_string(), format % args), flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(description="Fountain simulated video instrument")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8172)
    args = parser.parse_args()
    DEVICE.enable_capture("720p50")
    DEVICE.start()
    DEVICE.advance(1)
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print("Fountain VIDEO SIMULATION at http://%s:%d" % (args.host, args.port), flush=True)
    try:
        server.serve_forever(poll_interval=0.5)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
