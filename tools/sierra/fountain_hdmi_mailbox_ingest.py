#!/usr/bin/env python2.7
"""Private HDMI JPEG ingress into silver Mac's disposable virtio block mailbox.

Sierra native FountainVideo.app authenticates with existing local private key.
Only loopback port 8483 is allowed. Only decklink-observed JPEG frames pass.
This is the hardware-host media producer, NOT a PCI/firmware driver.
"""
from __future__ import print_function

import argparse
import hmac
import json
import os
import stat
import sys
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, ROOT)
from simulator.hdmi_frame_mailbox import (
    DISK_BYTES, MAX_FRAME_BYTES, MailboxError, Writer)


class Handler(BaseHTTPRequestHandler):
    server_version = "FountainHDMIMailboxSierra/1"

    def reply(self, code, obj):
        body = json.dumps(obj, separators=(",", ":")).encode("ascii")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/health":
            return self.reply(200, {"media": "fountain-hdmi-mailbox-v1",
                                    "guest_network":False,
                                    "physical_hdmi_verified_by_mailbox":False,
                                    "last_host_commit":self.server.writer.seq})
        self.reply(404, {"error":"NOT_FOUND"})

    def do_POST(self):
        if self.path != "/api/scratch/ingest":
            return self.reply(404, {"error":"NOT_FOUND"})
        if not hmac.compare_digest(self.headers.get("Authorization", ""),
                                   "Bearer " + self.server.token):
            return self.reply(401, {"error":"LOCAL_AUTH_REQUIRED"})
        if self.headers.get("Content-Type") != "image/jpeg":
            return self.reply(415, {"error":"JPEG_REQUIRED"})
        if self.headers.get("X-Fountain-Origin") != "decklink-observed":
            return self.reply(422, {"error":"PHYSICAL_PROVENANCE_REQUIRED"})
        try:
            length = int(self.headers.get("Content-Length", "-1"))
            pts = int(self.headers.get("X-Fountain-PTS-Ns", "-1"))
            width = int(self.headers.get("X-Fountain-Width", "0"))
            height = int(self.headers.get("X-Fountain-Height", "0"))
            stream = self.headers.get("X-Fountain-Stream-ID", "")
        except (TypeError, ValueError):
            return self.reply(400, {"error":"INVALID_HEADERS"})
        if length < 4 or length > MAX_FRAME_BYTES:
            return self.reply(413, {"error":"BOUNDED_JPEG_REQUIRED"})
        frame = self.rfile.read(length)
        if len(frame) != length:
            return self.reply(422, {"error":"INCOMPLETE_FRAME"})
        try:
            seq = self.server.writer.submit(pts, width, height, frame, stream)
        except (MailboxError, ValueError, UnicodeError):
            return self.reply(422, {"error":"INVALID_HDMI_FRAME"})
        except OSError:
            return self.reply(503, {"error":"MAILBOX_WRITE_FAILED"})
        return self.reply(201, {"accepted":True, "seq":seq,
                                "origin":"decklink-observed",
                                "simulated":False,
                                "device_timestamp_used":False})

    def log_message(self, *args):
        # Never log source desktop imagery, credentials or precise timestamps.
        return


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--mailbox", required=True)
    p.add_argument("--token-file", required=True)
    p.add_argument("--bind", default="127.0.0.1")
    p.add_argument("--port", type=int, default=8483)
    a = p.parse_args()
    if a.bind != "127.0.0.1" or a.port != 8483:
        p.error("local loopback 127.0.0.1:8483 only")
    s = os.stat(a.token_file)
    if not stat.S_ISREG(s.st_mode) or (s.st_mode & 0o077):
        p.error("existing local token file must be private")
    with open(a.token_file, "rb") as fh:
        token = fh.read().strip()
    if len(token) < 32:
        p.error("32+ character local token required")
    writer = Writer(a.mailbox)
    server = HTTPServer((a.bind,a.port),Handler)
    server.token = token
    server.writer = writer
    print("stage=host_hdmi_mailbox status=ready bind=loopback port=8483 "
          "size=%d latest_seq=%d" % (DISK_BYTES, writer.seq))
    sys.stdout.flush()
    try:
        server.serve_forever(poll_interval=.05)
    finally:
        server.server_close()
        writer.close()


if __name__ == "__main__":
    main()
