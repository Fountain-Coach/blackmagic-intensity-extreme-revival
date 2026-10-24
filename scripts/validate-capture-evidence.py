#!/usr/bin/env python3
"""Validate bounded DeckLink observer telemetry (not source video itself).

A PASS is only a capture-API evidence candidate. Trust in an actual physical
unprotected source requires the separate preflight and physical validation.
"""
import argparse
import re
import sys

MODES = {
    "1080p25": ("0x48703235", 1920, 1080),
    "1080p30": ("0x48703330", 1920, 1080),
    "720p60": ("0x68703630", 1280, 720),
}

def parse_stages(output):
    items = {}
    for line in output.splitlines():
        if not line.startswith("stage="):
            continue
        record = {}
        for part in line.split():
            if "=" not in part:
                continue
            key, value = part.split("=", 1)
            record[key] = value
        stage = record.get("stage")
        if stage:
            items.setdefault(stage, []).append(record)
    return items

def require_one(items, stage):
    records = items.get(stage, [])
    if len(records) != 1:
        raise ValueError("expected exactly one %s record" % stage)
    return records[0]

def checked_integer(row, key, minimum=0):
    if key not in row:
        raise ValueError("missing " + key)
    try:
        value = int(row[key], 0)
    except ValueError:
        raise ValueError("invalid " + key)
    if value < minimum:
        raise ValueError("invalid " + key + " range")
    return value

def validate_capture(output, mode):
    """Raises ValueError on any evidence gap; never asserts physical provenance."""
    items = parse_stages(output)
    code, width, height = MODES[mode]
    requested = require_one(items, "requested_video_mode")
    if requested.get("code") != code:
        raise ValueError("requested video mode differs")
    final_lock = require_one(items, "signal_lock_final")
    if final_lock.get("rc", "").lower() != "0x00000000" or final_lock.get("locked") != "1":
        raise ValueError("no final input signal lock")
    counters = require_one(items, "counts")
    frames = checked_integer(counters, "frames", 1)
    valid = checked_integer(counters, "valid", 1)
    missing = checked_integer(counters, "no_source")
    if valid > frames or missing + valid > frames:
        raise ValueError("inconsistent frame counters")
    first = require_one(items, "first_valid_frame")
    if first.get("bytes_ok") != "1" or first.get("timestamp_ok") != "1":
        raise ValueError("no first-frame pixel/timestamp evidence")
    if checked_integer(first, "width", 1) != width or checked_integer(first, "height", 1) != height:
        raise ValueError("unexpected frame geometry")
    stride = checked_integer(first, "row_bytes", width * 2)
    if stride > 65536:
        raise ValueError("unexpected frame stride")
    sampled = checked_integer(first, "sample_size", 1)
    if sampled > 4096 or sampled > stride * height:
        raise ValueError("bad frame sample")
    if checked_integer(first, "duration", 1) <= 0:
        raise ValueError("bad frame duration")
    if first.get("timescale") != "30000":
        raise ValueError("unexpected time scale")
    # Readability of the hash proves the bounded sample callback ran, not pixels.
    if not re.match(r"^0x[0-9a-fA-F]{8}$", first.get("sample_fnv32", "")):
        raise ValueError("missing first-frame fingerprint")
    completed = require_one(items, "complete")
    if completed.get("exit") != "0":
        raise ValueError("observer did not tear down cleanly")
    return {"frames": frames, "valid": valid, "no_source": missing,
            "sample": first["sample_fnv32"]}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=sorted(MODES), required=True)
    args = parser.parse_args()
    report = sys.stdin.read()
    try:
        result = validate_capture(report, args.mode)
    except ValueError as exc:
        print("stage=capture_acceptance result=blocked reason=%s" %
              str(exc).replace(" ", "_"))
        return 2
    print("stage=capture_acceptance result=api_frame_candidate"
          " frames=%d valid=%d no_source=%d sample=%s"
          " physical_source_verified_separately=0" %
          (result["frames"], result["valid"], result["no_source"],
           result["sample"]))
    return 0

if __name__ == "__main__":
    sys.exit(main())
