#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Die Maschine / Thunderbolt. Sierra Python 2.7, read-only hardware score.
The hardware contributes real PCIe and DeckLink status, NOT captured images.
All scene motion and optional sound are generated locally by the Mac.
No output/input enable, firmware, drivers, or HDMI operations.
"""
from __future__ import print_function
import json
import os
import re
import sys
import time
import subprocess
from BaseHTTPServer import HTTPServer, BaseHTTPRequestHandler

LOOPBACK = "127.0.0.1"
PORT = 18743
PROBE = "/tmp/fc-intensity-status-readonly-20261025"
HTML = "/tmp/fc-intensity-thunderbolt-score-20261025.html"
OUTPUT_LOCK = os.path.expanduser("~/Library/Application Support/Fountain Coach/Video Instrument/run/OUTPUT_DISABLED")
started = time.time()
deadline = started + 3600
sample_count = 0

def execute(argv):
    try:
        return subprocess.check_output(argv, stderr=subprocess.STDOUT)
    except Exception as error:
        return "UNAVAILABLE: " + str(error)

def parse_status(output):
    fields = {}
    for line in output.splitlines():
        if "stage=video_output_status key=" not in line:
            continue
        match = re.search(r"key=(\w+) rc=(0x[0-9a-f]+) value=(0x[0-9a-f]+)", line)
        if match:
            fields[match.group(1)] = {"rc": match.group(2), "value": match.group(3) if match.group(2) == "0x00000000" else None}
    return fields

def snapshot():
    global sample_count
    pci = execute(["/usr/sbin/ioreg", "-r", "-n", "Intensity Extreme@0", "-l", "-w0"])
    present = all(token in pci for token in [
        '"vendor-id" = <bdbd0000>', '"device-id" = <24a10000>',
        '"IOPCITunnelled" = Yes', '"IOPCIResourced" = Yes'])
    if present and os.path.isfile(PROBE):
        status = parse_status(execute([PROBE]))
    else:
        status = {}
    if present:
        sample_count += 1
    m = re.search(r'"IOPCIExpressLinkStatus" = ([0-9]+)', pci)
    return {
        "device": "Intensity Extreme" if present else "NOT DETECTED",
        "source": "Real Sierra IOKit PCIe registry and vendor DeckLink GetInt",
        "vendor_device": "BDBD:A124" if present else None,
        "pcie_tunnel": present,
        "pcie_link_status_raw": int(m.group(1)) if m and present else None,
        "output_mode": status.get("current_output_mode", {}).get("value"),
        "output_flags": status.get("current_output_flags", {}).get("value"),
        "busy_status_supported": status.get("busy", {}).get("rc") == "0x00000000",
        "busy": status.get("busy", {}).get("value"),
        "output_disabled_guard": os.path.isfile(OUTPUT_LOCK),
        "live_sample_count": sample_count,
        "sample_time": time.strftime("%Y-%m-%d %H:%M:%S"),
        "no_external_video_required": True,
        "frame_capture_verified": False,
        "frame_readback_verified": False,
        "motion_origin": "Mac software clock; hardware identity and register status set the score",
    }

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def reply(self, code, kind, data):
        self.send_response(code)
        self.send_header("Content-Type", kind)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path == "/":
            self.reply(200, "text/html; charset=utf-8", open(HTML, "rb").read())
        elif self.path == "/api/snapshot":
            self.reply(200, "application/json; charset=utf-8",
                       json.dumps(snapshot(), sort_keys=True).encode("utf-8"))
        elif self.path == "/health":
            self.reply(200, "text/plain", "READ_ONLY_THUNDERBOLT_SCORE\n")
        else:
            self.reply(404, "text/plain", "Not found\n")

    def do_POST(self):
        if self.path != "/stop":
            self.reply(404, "text/plain", "Not found\n")
            return
        self.server.running = False
        self.reply(200, "text/plain", "Score stopped; no hardware state changed.\n")

def main():
    if sys.platform != "darwin":
        raise SystemExit("Original Silver Mac only.")
    if not os.path.isfile(OUTPUT_LOCK):
        raise SystemExit("Refusing to start without the existing OUTPUT_DISABLED guard.")
    if not os.path.isfile(PROBE):
        raise SystemExit("Refusing to start without the verified read-only DeckLink status program.")
    if not os.path.isfile(HTML):
        raise SystemExit("Missing owned score HTML.")
    server = HTTPServer((LOOPBACK, PORT), Handler)
    server.timeout = 0.75
    server.running = True
    print("THUNDERBOLT_ONLY_SCORE_STARTED http://%s:%s/ until=%d" %
          (LOOPBACK, PORT, int(deadline)))
    sys.stdout.flush()
    while server.running and time.time() < deadline:
        server.handle_request()
    server.server_close()
    print("THUNDERBOLT_ONLY_SCORE_STOPPED output_guard_preserved=%s" % os.path.isfile(OUTPUT_LOCK))
    sys.stdout.flush()

if __name__ == "__main__":
    main()
