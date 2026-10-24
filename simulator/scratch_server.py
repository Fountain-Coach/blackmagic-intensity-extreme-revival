"""LAN browser scratch receiver for a native Sierra video producer.

Runs independently of Blackmagic and of the full virtual-video lab.
POST /api/scratch/ingest: JPEG + authenticated source metadata.
GET /api/scratch/state: bounded rolling frame history and timestamps.
GET /api/scratch/frame/<seq>.jpg: immutable frame, forward/reverse playable.
GET /scratch: source-neutral browser jog/shuttle/stop UI.

A strong randomly generated token is REQUIRED at startup. Never expose this
unauthenticated viewer or receiver to a public Internet interface; frame
history can contain private pictures if hardware input is later selected.
There is no hardware I/O in this Python service.
"""
from __future__ import annotations

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
from threading import RLock
from urllib.parse import urlsplit

from .scratch import InvalidFrame, ScratchHistory, MAX_FRAME_BYTES

PAGE = Path(__file__).with_name("web") / "scratch.html"


class ScratchHandler(BaseHTTPRequestHandler):
    server_version = "FountainBrowserScratch/0.1"

    def respond(self, code, data, content_type, cache="no-store", extra=None):
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", cache)
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("X-Fountain-Scratch", "source-neutral")
        for name, value in (extra or {}).items():
            self.send_header(name, value)
        self.end_headers()
        self.wfile.write(data)

    def respond_json(self, code, data):
        body = json.dumps(data, separators=(",", ":")).encode("utf-8")
        self.respond(code, body, "application/json; charset=utf-8")

    def do_GET(self):
        path = urlsplit(self.path).path
        if path in ("/scratch", "/scratch/"):
            return self.respond(200, PAGE.read_bytes(),
                                "text/html; charset=utf-8")
        if path == "/api/scratch/state":
            return self.respond_json(200, self.server.history.snapshot())
        match = re.fullmatch(r"/api/scratch/frame/([0-9]{1,18})\.jpg", path)
        if match:
            frame = self.server.history.get(int(match.group(1)))
            if not frame:
                return self.respond_json(404, {"error":"FRAME_EVICTED_OR_UNKNOWN"})
            return self.respond(
                200, frame.data, "image/jpeg",
                cache="private, max-age=300, immutable",
                extra={
                    "X-Fountain-Frame-Seq": str(frame.seq),
                    "X-Fountain-Frame-PTS-Ns": str(frame.pts_ns),
                    "X-Fountain-Frame-Origin": frame.origin,
                    "X-Fountain-HDMI-Verified": "false",
                })
        return self.respond_json(404, {"error":"NOT_FOUND"})

    def do_POST(self):
        path = urlsplit(self.path).path
        if path != "/api/scratch/ingest":
            return self.respond_json(404, {"error":"NOT_FOUND"})
        header = self.headers.get("Authorization", "")
        # compare_digest is constant time for token comparison.
        import hmac
        if not hmac.compare_digest(header.encode("utf-8"),
                                   ("Bearer " + self.server.ingest_token).encode("utf-8")):
            return self.respond_json(401, {"error":"INGEST_AUTH_REQUIRED"})
        if self.headers.get("Content-Type") != "image/jpeg":
            return self.respond_json(415, {"error":"JPEG_REQUIRED"})
        try:
            count = int(self.headers.get("Content-Length", "-1"))
        except ValueError:
            return self.respond_json(400, {"error":"INVALID_LENGTH"})
        if not 4 <= count <= MAX_FRAME_BYTES:
            return self.respond_json(413, {"error":"FRAME_TOO_LARGE"})
        try:
            frame = self.server.history.push(
                self.rfile.read(count),
                int(self.headers.get("X-Fountain-PTS-Ns", "-1")),
                self.headers.get("X-Fountain-Origin", ""),
                int(self.headers.get("X-Fountain-Width", "0")),
                int(self.headers.get("X-Fountain-Height", "0")))
        except (ValueError, InvalidFrame) as e:
            return self.respond_json(422, {"error":"FRAME_REJECTED",
                                            "reason":str(e)})
        return self.respond_json(201, {"accepted":True,
                                       **frame.metadata})

    def log_message(self, fmt, *args):
        # Capture privacy: never log token, frame pixels or query payload.
        if self.path == "/api/scratch/ingest":
            return
        return super().log_message(fmt, *args)


class ScratchServer(ThreadingHTTPServer):
    daemon_threads = True
    def __init__(self, bind, token, history=None):
        if not token or len(token) < 32:
            raise ValueError("A 32+ character ingest token is required")
        super().__init__(bind, ScratchHandler)
        self.ingest_token = token
        self.history = history if history is not None else ScratchHistory()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8273)
    args = parser.parse_args()
    token = os.environ.get("FOUNTAIN_SCRATCH_INGEST_TOKEN", "")
    if not token:
        secret = Path.home() / ".config" / "fountain-video" / "scratch-ingest-token"
        try:
            token = secret.read_text(encoding="ascii").strip()
        except OSError:
            token = ""
    try:
        server = ScratchServer((args.host, args.port), token)
    except ValueError as e:
        parser.error(str(e))
    print("stage=scratch_server result=listening "
          "address=http://%s:%d/scratch "
          "ingest_authenticated=1 physical_device_access=0" %
          (args.host,args.port),flush=True)
    try:
        server.serve_forever(poll_interval=.3)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
