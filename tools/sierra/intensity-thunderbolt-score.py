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
import wave
import array
import math
from BaseHTTPServer import HTTPServer, BaseHTTPRequestHandler

LOOPBACK = "127.0.0.1"
PORT = 18743
PROBE = "/tmp/fc-intensity-status-readonly-20261025"
AUDIO_SCORE = "/tmp/fc-intensity-audio-score"
HTML = "/tmp/fc-intensity-thunderbolt-score-20261025.html"
CAPTURE_PROGRAM = "/tmp/fc-silver-linein-capture-20261025"
RECORDINGS = os.path.expanduser("~/Music/Fountain Coach/Die Maschine/Recordings")
latest_recording = None
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

def analog_status():
    if not os.path.isfile(AUDIO_SCORE) or not os.access(AUDIO_SCORE, os.X_OK):
        return {"connected": None, "route": None, "ready": False}
    result = execute([AUDIO_SCORE, "--inspect"])
    match = re.search(r"stage=analog_jack connected=([01]) source=([A-Za-z0-9.]{4}) jack_status=0 source_status=0 ready=([01])", result)
    if not match:
        return {"connected": None, "route": None, "ready": False}
    return {"connected": match.group(1) == "1",
            "route": match.group(2), "ready": match.group(3) == "1"}

def line_input_ready():
    if not os.path.isfile(CAPTURE_PROGRAM) or not os.access(CAPTURE_PROGRAM, os.X_OK):
        return False
    witness = execute([CAPTURE_PROGRAM, "--inspect"])
    return "stage=recorder_input_ready ready=1" in witness and (
        "stage=physical_input source=0x6c696e65 jack=1 mute=0" in witness)

def analyze_wav(path):
    stream = wave.open(path, "rb")
    try:
        rate = stream.getframerate()
        n = stream.getnframes()
        if stream.getnchannels()!=2 or stream.getsampwidth()!=2 or n<rate*4:
            return {"error":"UNEXPECTED_RECORDING_FORMAT"}
        stream.setpos(rate)
        raw = stream.readframes(n-rate)
    finally:
        stream.close()
    a = array.array("h")
    a.fromstring(raw)
    if sys.byteorder != "little":
        a.byteswap()
    values=[]
    for ch in (0,1):
        pts=a[ch::2]
        npts=len(pts)
        mean=sum(pts)/float(npts)
        sqsum=sum(float(v)*v for v in pts)/npts
        rms=math.sqrt(max(0.0,sqsum-mean*mean))
        values.append(round(20.0*math.log10(max(rms,0.01)/32768.0),1))
    return {"duration_seconds":float(n)/rate,"sample_rate":rate,
            "channels":2,"ac_rms_dbfs_after_first_second":values,
            "signal_above_minus_55_dbfs":max(values)>-55.0}

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
    analog = analog_status()
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
        "analog_jack_connected": analog["connected"],
        "analog_route": analog["route"],
        "analog_route_ready": analog["ready"],
        "line_input_ready": line_input_ready(),
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
        if self.path == "/" or self.path.startswith("/?"):
            self.reply(200, "text/html; charset=utf-8", open(HTML, "rb").read())
        elif self.path == "/api/snapshot":
            self.reply(200, "application/json; charset=utf-8",
                       json.dumps(snapshot(), sort_keys=True).encode("utf-8"))
        elif self.path == "/health":
            self.reply(200, "text/plain", "READ_ONLY_THUNDERBOLT_SCORE\n")
        elif self.path == "/api/recording/latest.wav":
            global latest_recording
            if not latest_recording or not os.path.isfile(latest_recording):
                self.reply(404, "text/plain", "No user-requested recording in this session.\n")
                return
            with open(latest_recording, "rb") as recorded:
                audio_bytes = recorded.read()
            self.send_response(200)
            self.send_header("Content-Type", "audio/wav")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Disposition", 'attachment; filename="%s"' %
                             os.path.basename(latest_recording))
            self.send_header("Content-Length", str(len(audio_bytes)))
            self.end_headers()
            self.wfile.write(audio_bytes)
        else:
            self.reply(404, "text/plain", "Not found\n")

    def do_HEAD(self):
        if self.path != "/api/recording/latest.wav" or not latest_recording or not os.path.isfile(latest_recording):
            self.send_response(404)
            self.end_headers()
            return
        self.send_response(200)
        self.send_header("Content-Type", "audio/wav")
        self.send_header("Content-Length", str(os.path.getsize(latest_recording)))
        self.send_header("Content-Disposition", 'attachment; filename="%s"' %
                         os.path.basename(latest_recording))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()

    def do_POST(self):
        origin = self.headers.get("Origin", "")
        referer = self.headers.get("Referer", "")
        allowed = "http://127.0.0.1:%d" % PORT
        if (origin and origin != allowed) or (referer and not referer.startswith(allowed + "/")):
            self.reply(403, "text/plain", "Cross-origin action refused.\n")
            return
        if self.path == "/stop":
            self.server.running = False
            self.reply(200, "text/plain", "Score stopped; no hardware state changed.\n")
            return
        if self.path == "/api/record":
            global latest_recording
            witness = snapshot()
            if not witness["output_disabled_guard"] or not witness["line_input_ready"]:
                self.reply(409, "application/json",
                           json.dumps({"result": "ANALOG_LINE_INPUT_NOT_READY"}))
                return
            if not os.path.isfile(CAPTURE_PROGRAM) or not os.access(CAPTURE_PROGRAM, os.X_OK):
                self.reply(503, "application/json",
                           json.dumps({"result": "RECORDER_NOT_INSTALLED"}))
                return
            try:
                if not os.path.isdir(RECORDINGS):
                    os.makedirs(RECORDINGS, 0o700)
                stem = "fc-die-maschine-line-in-" + time.strftime("%Y%m%d-%H%M%S")
                name = None
                for n in range(20):
                    candidate = stem + ("-%02d" % n if n else "") + ".wav"
                    if not os.path.exists(os.path.join(RECORDINGS, candidate)):
                        name = candidate
                        break
                if not name:
                    raise RuntimeError("Recording filename collision")
                destination = os.path.join(RECORDINGS, name)
                child = subprocess.Popen(
                    ["/usr/bin/perl", "-e", "alarm 14; exec @ARGV",
                     CAPTURE_PROGRAM, "--seconds", "8", destination],
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                report = child.communicate()[0]
                valid = (child.returncode == 0 and
                         "stage=recording_end" in report and
                         "stop_code=0" in report and
                         os.path.isfile(destination))
                if valid:
                    analysis = analyze_wav(destination)
                    latest_recording = destination
                    result = {"result": "LINE_INPUT_RECORDED",
                              "file": destination,
                              "download": "/api/recording/latest.wav",
                              "analysis": analysis,
                              "return_through_phonic_proven": False,
                              "report": report[-1000:]}
                else:
                    result = {"result": "LINE_INPUT_RECORDING_FAILED",
                              "report": report[-1000:]}
                self.reply(200 if valid else 503, "application/json",
                           json.dumps(result, sort_keys=True))
            except Exception as error:
                self.reply(503, "application/json",
                           json.dumps({"result":"RECORDING_EXCEPTION",
                                       "detail":str(error)[:200]}))
            return
        modes = {"/api/audio/phonic": "--phonic",
                 "/api/audio/blackmagic": "--blackmagic"}
        if self.path not in modes:
            self.reply(404, "text/plain", "Not found\n")
            return
        witness = snapshot()
        if (not witness["pcie_tunnel"] or not witness["output_disabled_guard"] or
                witness["output_flags"] != "0x00000000"):
            self.reply(409, "application/json",
                       json.dumps({"result": "REFUSED_HARDWARE_PREFLIGHT"}))
            return
        program = "/tmp/fc-intensity-audio-score"
        if not os.path.isfile(program) or not os.access(program, os.X_OK):
            self.reply(503, "application/json",
                       json.dumps({"result": "NATIVE_AUDIO_PROGRAM_UNAVAILABLE"}))
            return
        route = modes[self.path]
        if route == "--phonic" and not witness["analog_route_ready"]:
            self.reply(409, "application/json",
                       json.dumps({"result": "ANALOG_JACK_NOT_DETECTED"}))
            return
        env = os.environ.copy()
        try:
            child = subprocess.Popen(
                ["/usr/bin/perl", "-e", "alarm 15; exec @ARGV", program, route],
                env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            report = child.communicate()[0]
            valid = (child.returncode == 0 and
                     "returned_buffers=8" in report and
                     "device_running_observed=1" in report and
                     "stop_code=0" in report)
        except Exception as err:
            report = "NATIVE_AUDIO_EXECUTION_ERROR: %s" % str(err)
            valid = False
        result = {
            "result": "COREAUDIO_COMPLETED" if valid else "COREAUDIO_NOT_ACCEPTED",
            "route": route[2:],
            "device_accepted_frames": valid,
            "audible_hearing": "NOT_MACHINE_MEASURED",
            "physical_blackmagic_port": "NOT_WITNESSED",
            "sound_generated_by": "SILVER_MAC",
            "report": report[-1200:],
        }
        self.reply(200 if valid else 503, "application/json",
                   json.dumps(result, sort_keys=True))

def main():
    global latest_recording
    if os.path.isdir(RECORDINGS):
        found = []
        for name in os.listdir(RECORDINGS):
            if name.startswith("fc-die-maschine-line-in-") and name.endswith(".wav"):
                path = os.path.join(RECORDINGS, name)
                if os.path.isfile(path) and os.path.realpath(path).startswith(
                        os.path.realpath(RECORDINGS) + os.sep):
                    found.append(path)
        if found:
            latest_recording = max(found, key=os.path.getmtime)
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
