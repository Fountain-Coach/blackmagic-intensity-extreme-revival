#!/usr/bin/env python3
"""End-to-end witness of a live SOFTWARE-ONLY Fountain video laboratory.

Runs controlled software scenarios through HTTP: camera-off, restored input,
format hot-swap, video+audio, 2-device loopback, same-device half-duplex error,
and archive integrity. It deliberately restores the visible loopback demo.

No Blackmagic, camera, HDMI, Syphon or PCI access and no third-party package.

Usage:
    python3 scripts/accept-software-video-lab.py --url http://127.0.0.1:8272
"""
from __future__ import annotations

import argparse
from io import BytesIO
import json
from urllib.error import HTTPError
from urllib.request import Request, urlopen
from zipfile import ZipFile
from zlib import crc32


def request(base, path, value=None):
    kwargs = {}
    if value is not None:
        kwargs = {
            "method": "POST",
            "data": json.dumps(value).encode("utf-8"),
            "headers": {"Content-Type": "application/json"},
        }
    call = Request(base.rstrip("/") + path, **kwargs)
    try:
        with urlopen(call, timeout=12) as response:
            status, body, headers = response.status, response.read(), response.headers
    except HTTPError as e:
        status, body, headers = e.code, e.read(), e.headers
    assert_true(headers.get("X-Fountain-Simulation") == "true",
                path + ": provenance header is absent")
    return status, body


def read_json(base, path, value=None):
    code, body = request(base, path, value)
    return code, json.loads(body)


def assert_true(value, message):
    if not value:
        raise AssertionError(message)


def step(base, ticks):
    code, state = read_json(base, "/api/lab/step", {"count": ticks})
    assert_true(code == 200, "step failed")
    return state


def run(base):
    cases = 0

    code, cap = read_json(base, "/api/lab/capabilities")
    assert_true(code == 200 and cap["device_count"] == 2 and
                cap["physical_device_access"] is False and
                len(cap["modes"]) == 16, "not a full software lab")
    cases += 1

    code, state = read_json(base, "/api/lab/configure",
                            {"scenario": "camera-off", "mode": "720p50"})
    assert_true(code == 200 and not state["source"]["power"], "camera-off setup failed")
    state = step(base, 4)
    assert_true(state["totals"]["received"] == 0 and
                state["totals"]["input_missing"] == 4, "camera-off unexpectedly captured")
    code, _ = request(base, "/api/lab/input.png")
    assert_true(code == 409, "no-source preview unexpectedly served pixels")
    cases += 1

    code, state = read_json(base, "/api/lab/source", {"power": True})
    assert_true(code == 200, "virtual power restore failed")
    state = step(base, 2)
    assert_true(state["input"]["signal_locked"] and
                state["totals"]["received"] == 2 and
                state["input"]["last_frame"]["pts_ns"] == 100_000_000,
                "input power recovery or timing failed")
    code, video = request(base, "/api/lab/input.2vuy")
    meta = state["input"]["last_frame"]
    assert_true(code == 200 and len(video) == meta["video_bytes"] and
                ("%08x" % crc32(video)) == meta["video_crc32"], "video payload mismatch")
    code, audio = request(base, "/api/lab/input.pcm")
    assert_true(code == 200 and len(audio) == meta["audio"]["bytes"] and
                ("%08x" % crc32(audio)) == meta["audio"]["crc32"],
                "audio payload mismatch")
    cases += 1

    code, blob = request(base, "/api/lab/archive.zip")
    assert_true(code == 200, "cannot download synthetic archive")
    with ZipFile(BytesIO(blob)) as archive:
        manifest = json.loads(archive.read("manifest.json"))
        assert_true(manifest["simulated"] and
                    not manifest["captured_from_hardware"] and
                    len(manifest["frames"]) == 2, "archive provenance or size mismatch")
        for frame in manifest["frames"]:
            root = "synthetic-%06d" % frame["frame"]
            stored = archive.read(root + ".2vuy")
            pcm = archive.read(root + ".pcm")
            assert_true(("%08x" % crc32(stored)) == frame["video_crc32"] and
                        ("%08x" % crc32(pcm)) == frame["audio"]["crc32"],
                        "archive content CRC mismatch")
    cases += 1

    code, _ = read_json(base, "/api/lab/configure",
                        {"scenario": "camera", "mode": "720p50",
                         "auto_detect": "continuous"})
    assert_true(code == 200, "format-change setup failed")
    step(base, 2)
    code, _ = read_json(base, "/api/lab/source", {"mode": "1080i50"})
    assert_true(code == 200, "changing source format failed")
    changed = step(base, 1)
    meta = changed["input"]["last_frame"]
    assert_true(changed["input"]["mode"] == "1080i50" and
                meta["pts_ns"] == 40_000_000 and meta["interlaced"] and
                meta["audio"]["start_sample"] == 1920,
                "hot-swap failed to preserve logical frame/audio time")
    cases += 1

    code, _ = read_json(base, "/api/lab/configure",
                        {"scenario": "loopback", "mode": "720p50",
                         "pattern": "checker"})
    assert_true(code == 200, "loopback setup failed")
    state = step(base, 3)
    assert_true(state["input"]["delivered"] == 3 and
                state["output"]["delivered"] == 3 and
                state["loopback"]["last_video_audio_pts_identical"],
                "virtual loopback failed")
    iv = request(base, "/api/lab/input.2vuy")[1]
    ov = request(base, "/api/lab/output.2vuy")[1]
    ia = request(base, "/api/lab/input.pcm")[1]
    oa = request(base, "/api/lab/output.pcm")[1]
    assert_true(iv == ov and ia == oa and
                state["input"]["last_frame"]["pts_ns"] ==
                state["output"]["last_frame"]["pts_ns"],
                "loopback failed byte-and-timestamp identity")
    cases += 1

    code, conflict = read_json(base, "/api/lab/conflict", {})
    assert_true(code == 200 and conflict["blocked"] and
                conflict["code"] == "0x80000009" and
                conflict["state"]["input"]["streaming"] and
                conflict["state"]["output"]["streaming"],
                "same-device half-duplex rule not enforced")
    cases += 1

    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:8272",
                        help="Full lab HTTP origin, normally loopback or private WLAN")
    args = parser.parse_args()
    count = 0
    try:
        count = run(args.url)
        print("stage=full_software_lab result=pass scenarios=%d "
              "hardware_access=0 real_camera_required=0" % count)
        return 0
    except Exception as e:
        print("stage=full_software_lab result=fail reason=%s: %s" %
              (type(e).__name__, e))
        return 1
    finally:
        # Leave the browser in the useful, immediately visible demo state.
        try:
            read_json(args.url, "/api/lab/configure",
                      {"scenario": "loopback", "mode": "720p50",
                       "pattern": "moving"})
            step(args.url, 2)
        except Exception as e:
            print("stage=software_lab_restore result=unavailable reason=%s" %
                  type(e).__name__)


if __name__ == "__main__":
    raise SystemExit(main())
