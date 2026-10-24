#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Read-only HDMI source display preflight for Sierra (Python 2.7) and macOS 26.

Requires an active external display whose EDID-reported name is "BMD HDMI",
and a known video timing. This is *not* proof of HDCP status or delivered frames.

Usage:
  python scripts/hdmi-source-preflight.py --mode 1080p30

Exit 0: expected BMD HDMI sink and matching source computer display timing.
Exit 2: no BMD HDMI sink.
Exit 3: BMD HDMI present but source timing mismatched or unavailable.
Exit 4: more than one BMD HDMI endpoint is present (ambiguous source).
Exit 20: display inventory failed or had no data.
Exit 64: invalid arguments.
"""
from __future__ import print_function

import argparse
import plistlib
import re
import subprocess
import sys

MODES = {"1080p30": (1920, 1080, 30.0),
         "1080p25": (1920, 1080, 25.0),
         "720p60": (1280, 720, 60.0),
         "720p50": (1280, 720, 50.0)}
BMD_NAME = "BMD HDMI"
TIMING = re.compile(r"([0-9]+)\s*x\s*([0-9]+).*?@\s*([0-9]+(?:\.[0-9]+)?)\s*Hz",
                    re.IGNORECASE)


def parse_xml(raw):
    # Sierra provides plistlib.readPlistFromString, newer macOS .loads.
    if hasattr(plistlib, "loads"):
        return plistlib.loads(raw)
    return plistlib.readPlistFromString(raw)


def flatten_displays(profiler):
    result = []
    if not isinstance(profiler, list):
        return result
    for report in profiler:
        if not isinstance(report, dict) or report.get("_dataType") != "SPDisplaysDataType":
            continue
        for adapter in report.get("_items", []):
            if not isinstance(adapter, dict):
                continue
            for display in adapter.get("spdisplays_ndrvs", []):
                if not isinstance(display, dict):
                    continue
                label = display.get("_name", "unknown")
                mode = display.get("spdisplays_resolution", "")
                if not isinstance(label, type("")):
                    label = str(label)
                if not isinstance(mode, type("")):
                    mode = str(mode)
                online = display.get("spdisplays_online") == "spdisplays_yes"
                result.append({"name": label, "timing": mode, "online": online})
    return result


def timing_matches(label, expected):
    match = TIMING.search(label)
    if not match:
        return False
    width, height, hz = expected
    return (int(match.group(1)) == width and int(match.group(2)) == height and
            abs(float(match.group(3)) - hz) <= 0.01)


def evaluate(displays, mode_name):
    expected = MODES[mode_name]
    candidates = [d for d in displays
                  if d["online"] and d["name"].strip() == BMD_NAME]
    if not candidates:
        return 2
    if len(candidates) != 1:
        return 4
    if not timing_matches(candidates[0]["timing"], expected):
        return 3
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=sorted(MODES), default="1080p30")
    args = parser.parse_args(argv)
    try:
        process = subprocess.Popen(
            ["system_profiler", "SPDisplaysDataType", "-xml", "-detailLevel", "basic"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        output, errors = process.communicate()
        if process.returncode != 0:
            raise ValueError("system_profiler failed with code %s" % process.returncode)
        displays = flatten_displays(parse_xml(output))
        if not displays:
            raise ValueError("no display records in system_profiler output")
    except (OSError, ValueError, TypeError, KeyError, IndexError) as error:
        print("stage=source_preflight result=inventory_error", file=sys.stderr)
        return 20

    for number, display in enumerate(displays):
        # Deliberately do not publish EDID bytes, serial IDs or content.
        print("stage=display index=%d label=%s timing=%s online=%d" %
              (number, display["name"], display["timing"], int(display["online"])))
    status = evaluate(displays, args.mode)
    reason = {0: "edid_and_timing_witnessed_only",
              2: "bmd_endpoint_not_witnessed",
              3: "bmd_endpoint_timing_mismatch",
              4: "multiple_bmd_endpoints_ambiguous"}[status]
    print("stage=source_preflight result=%s requested_mode=%s"
          " source_video_and_hdcp_unverified=1" % (reason, args.mode))
    return status


if __name__ == "__main__":
    sys.exit(main())
