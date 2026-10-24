#!/usr/bin/env python2.7
"""Sierra-local authenticated HTTP -> xhyve COM2 media bridge.

This intentionally uses only Python 2.7 standard library and the ordinary
xhyve autopty device. The Blackmagic vendor API is NEVER opened here.
The isolated Linux Docker receiver owns media history; Sierra does not.
Only synthetic preview frames are admitted until physical HDMI acceptance.
"""
from __future__ import print_function
import argparse
import base64
import hmac
import json
import os
import stat
import sys
import termios
import threading
import time
import tty
try:
    from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
except ImportError:
    from http.server import BaseHTTPRequestHandler, HTTPServer

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
from simulator.serial_wire import (
    CHUNK, MAX_FRAME, STREAM_RE, WireError, crc32hex, chunks,
    header, read_line, send_line,
)


class SerialSender(object):
    def __init__(self, fd):
        self.fd = fd
        self.pending = bytearray()
        self.serial_lock = threading.Lock()
        self.fid = 0

    def expect(self, desired, timeout=6):
        got = read_line(self.fd, self.pending, timeout)
        if got != desired:
            raise WireError("unexpected serial acknowledgement")
        return got

    def submit(self, data, pts, width, height, stream):
        frame_header = header(self.fid + 1, pts, width, height, data, stream)
        with self.serial_lock:
            self.fid = self.fid % 999999999999 + 1
            fid = str(self.fid).encode("ascii")
            send_line(self.fd, frame_header)
            self.expect(b"H " + fid)
            for index, chunk in enumerate(chunks(data)):
                idx = str(index).encode("ascii")
                line = b"C " + fid + b" " + idx + b" " + (
                    base64.b64encode(chunk) + b":" + crc32hex(chunk))
                for attempt in range(4):
                    send_line(self.fd, line)
                    received = read_line(self.fd, self.pending)
                    if received == b"A " + fid + b" " + idx:
                        break
                    if received != b"R " + fid + b" " + idx:
                        raise WireError("invalid chunk acknowledgement")
                else:
                    raise WireError("chunk rejected")
            send_line(self.fd, b"E " + fid)
            response = read_line(self.fd, self.pending, timeout=10)
            prefix = b"D " + fid + b" "
            if not response.startswith(prefix):
                raise WireError("frame not committed")
            try:
                seq = int(response[len(prefix):])
                if seq < 0:
                    raise ValueError()
            except ValueError:
                raise WireError("invalid frame sequence")
            return seq


class LocalProxyHandler(BaseHTTPRequestHandler):
    server_version = "FountainSierraSerialMedia/0.1"

    def reply(self, code, info):
        data = json.dumps(info, separators=(",", ":")).encode("ascii")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path == "/health":
            self.reply(200, {"instrument":"fountain-serial-sierra-proxy",
                             "serial_transport":"enabled",
                             "physical_hdmi_capture_accepted":False,
                             "browser_viewer_proxied":False})
        else:
            self.reply(404, {"error":"NOT_FOUND"})

    def do_POST(self):
        if self.path != "/api/scratch/ingest":
            return self.reply(404, {"error":"NOT_FOUND"})
        bearer = self.headers.get("Authorization", "")
        if not hmac.compare_digest(bearer, "Bearer " + self.server.token):
            return self.reply(401, {"error":"INGEST_AUTH_REQUIRED"})
        if self.headers.get("Content-Type", "") != "image/jpeg":
            return self.reply(415, {"error":"JPEG_REQUIRED"})
        # On this trust boundary, vendor callbacks are not accepted evidence.
        if self.headers.get("X-Fountain-Origin") != "synthetic":
            return self.reply(422, {"error":"HARDWARE_ORIGIN_DISABLED"})
        try:
            size = int(self.headers.get("Content-Length", "-1"))
            pts = int(self.headers.get("X-Fountain-PTS-Ns", "-1"))
            width = int(self.headers.get("X-Fountain-Width", "0"))
            height = int(self.headers.get("X-Fountain-Height", "0"))
            stream = self.headers.get("X-Fountain-Stream-ID", "serial-sierra")
            if not 4 <= size <= MAX_FRAME or not STREAM_RE.match(
                    stream.encode("ascii")):
                return self.reply(413, {"error":"BOUNDED_FRAME_OR_STREAM_REQUIRED"})
            if not (0 <= pts < 2**63 and 1 <= width <= 4096 and 1 <= height <= 2160):
                return self.reply(422, {"error":"INVALID_FRAME_METADATA"})
        except (ValueError, UnicodeError):
            return self.reply(400, {"error":"INVALID_FRAME_HEADERS"})
        data = self.rfile.read(size)
        if len(data) != size or not data.startswith(b"\xff\xd8") or not data.endswith(b"\xff\xd9"):
            return self.reply(422, {"error":"COMPLETE_JPEG_REQUIRED"})
        try:
            seq = self.server.sender.submit(data, pts, width, height, stream)
        except (WireError, OSError):
            return self.reply(503, {"error":"SERIAL_GUEST_NOT_READY_OR_REJECTED"})
        return self.reply(201, {"accepted":True,
                                "seq":seq,
                                "origin":"synthetic",
                                "physical_hdmi_verified":False})

    def log_message(self, fmt, *args):
        # Never print image material, sender authorization or timestamp headers.
        if self.path != "/api/scratch/ingest":
            return
        return


def run(serial, token_file, bind, port):
    if bind != "127.0.0.1":
        raise ValueError("the Sierra proxy must bind only to loopback")
    st = os.stat(token_file)
    if not stat.S_ISREG(st.st_mode) or stat.S_IMODE(st.st_mode) & 0o077:
        raise ValueError("token file must be private 0600")
    with open(token_file, "rb") as fh:
        token = fh.read().strip()
    if len(token) < 32:
        raise ValueError("32+ character local token is required")
    if not serial.startswith("/dev/ttys"):
        raise ValueError("only an xhyve local autopty is accepted")
    fd = os.open(serial, os.O_RDWR | os.O_NOCTTY)
    tty.setraw(fd)
    termios.tcflush(fd, termios.TCIFLUSH)
    try:
        server = HTTPServer((bind, port), LocalProxyHandler)
        server.token = token
        server.sender = SerialSender(fd)
        print("stage=serial_host result=listening bound_to=loopback",
              "physical_hdmi_capture_accepted=0 guest_network_required=0",flush=True)
        try:
            server.serve_forever(poll_interval=.2)
        finally:
            server.server_close()
    finally:
        os.close(fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True,
                        help="ephemeral COM2 device printed by xhyve")
    parser.add_argument("--token-file", required=True)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8383)
    args = parser.parse_args()
    try:
        run(args.serial, args.token_file, args.bind, args.port)
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
