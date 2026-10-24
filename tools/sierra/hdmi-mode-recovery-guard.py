#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""HDMI mode-return guard for the original silver Mac mini (Sierra).

Default/verification paths never write a display mode. A recovery method is
implemented separately and can be tested with a fake Quartz interface; it is
NOT authorized for live use by the simulated watchdog acceptance test.

Supported:
    python tools/sierra/hdmi-mode-recovery-guard.py --inspect
    python tools/sierra/hdmi-mode-recovery-guard.py --dry-run

Deliberately no CLI exposing mutating mode changes on the current host.
A future separately audited controller may import restore_mode_30 after
physical console and boot-tested recovery have been witnessed.

Never opens the DeckLink runtime, starts video input, changes firmware, or
opens PCI resources. No screen pixel inspection.

Copyright (c) 2026 Fountain Coach contributors. MIT.
"""
from __future__ import print_function

import json
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time

EXPECTED_MODEL = "Macmini5,3"
EXPECTED_OS = "10.12.6"
BMD_VENDOR = 8961
BMD_PRODUCT = 0
ORIGINAL_MODE = (1920, 1080, 30.0)
TEST_MODE = (1920, 1080, 25.0)


def mode_tuple(q, mode):
    return (int(q.CGDisplayModeGetWidth(mode)),
            int(q.CGDisplayModeGetHeight(mode)),
            float(q.CGDisplayModeGetRefreshRate(mode)))


def equal_modes(a, b):
    return (a[0] == b[0] and a[1] == b[1]
            and abs(a[2] - b[2]) < 0.01)


def inspect_quartz(q):
    """Freshly enumerate identifiers. Never retain an old CGDirectDisplayID."""
    rc, displays, count = q.CGGetOnlineDisplayList(16, None, None)
    if rc != 0 or count != 1:
        return {"ok": False, "reason": "unexpected_display_count",
                "count": int(count)}
    display = displays[0]
    vendor = int(q.CGDisplayVendorNumber(display))
    product = int(q.CGDisplayModelNumber(display))
    if (vendor, product) != (BMD_VENDOR, BMD_PRODUCT):
        return {"ok": False, "reason": "not_direct_BMD_EDID",
                "vendor": vendor, "product": product}
    mode = q.CGDisplayCopyDisplayMode(display)
    if not mode:
        return {"ok": False, "reason": "no_active_mode"}
    current = mode_tuple(q, mode)
    supported = q.CGDisplayCopyAllDisplayModes(display, None)
    if supported is None:
        return {"ok": False, "reason": "mode_enumeration_failed"}
    modes = [mode_tuple(q, value) for value in supported]
    p30 = [m for m in modes if equal_modes(m, ORIGINAL_MODE)]
    p25 = [m for m in modes if equal_modes(m, TEST_MODE)]
    return {"ok": True, "display": display,
            "current": current, "has_p30": bool(p30),
            "has_p25": bool(p25), "vendor": vendor,
            "product": product}


def decide_return(snapshot, assumed_current=None):
    """Purely describe necessary recovery; no mutation, no API access."""
    if not snapshot.get("ok"):
        return "BLOCKED_%s" % snapshot.get("reason", "unknown")
    current = assumed_current if assumed_current is not None else snapshot["current"]
    if equal_modes(current, ORIGINAL_MODE):
        return "ALREADY_ORIGINAL"
    if not snapshot["has_p30"]:
        return "BLOCKED_NO_ORIGINAL_MODE"
    return "RETURN_TO_ORIGINAL_REQUIRED"


def restore_mode_30(q, sleep=time.sleep, attempts=12):
    """Uncalled, separately gated recovery implementation. Fresh ID each try.

    May only be invoked after independent console/recovery proof, never by
    --inspect / --dry-run. Returns a detailed outcome, not a guarantee.
    """
    for attempt in range(attempts):
        snapshot = inspect_quartz(q)
        decision = decide_return(snapshot)
        if decision == "ALREADY_ORIGINAL":
            return "VERIFIED_ORIGINAL"
        if decision == "RETURN_TO_ORIGINAL_REQUIRED":
            display = snapshot["display"]
            supported = q.CGDisplayCopyAllDisplayModes(display, None)
            targets = [m for m in supported if equal_modes(mode_tuple(q, m),
                                                            ORIGINAL_MODE)]
            if targets:
                rc = q.CGDisplaySetDisplayMode(display, targets[0], None)
                if rc == 0:
                    # CGDirectDisplayID can change during the mode switch.
                    # The subsequent pass independently re-enumerates it.
                    sleep(0.2)
                    continue
        sleep(0.25)
    return "UNVERIFIED_ORIGINAL"


def ensure_sierra_host():
    if sys.platform != "darwin":
        raise RuntimeError("not_darwin")
    model = subprocess.check_output(["sysctl", "-n", "hw.model"]).strip()
    version = subprocess.check_output(["sw_vers", "-productVersion"]).strip()
    if model != EXPECTED_MODEL.encode("ascii") or version != EXPECTED_OS.encode("ascii"):
        raise RuntimeError("wrong_machine_or_os")


def live_snapshot():
    ensure_sierra_host()
    import Quartz
    return inspect_quartz(Quartz)


def audit_write(path, content):
    # The verification fixture owns a dedicated /tmp directory.
    tmp = path + ".tmp.%d" % os.getpid()
    with open(tmp, "wb") as f:
        f.write(json.dumps(content, sort_keys=True).encode("utf-8"))
        f.write(b"\n")
    os.rename(tmp, path)


def read_json(path):
    with open(path, "rb") as f:
        return json.load(f)


def pid_alive(pid):
    try:
        os.kill(int(pid), 0)
        return True
    except OSError:
        return False


def watch_only(lease_file, audit_file):
    """Independent watchdog process. Critical: no mutating mode path here."""
    try:
        lease = read_json(lease_file)
        if lease.get("contract") != "FC_SIERRA_HDMI_GUARD_SIMULATION_V1":
            raise ValueError("not_a_simulation_lease")
        if lease.get("fixture") != "pretend_source_1080p25":
            raise ValueError("unknown_simulated_source")
        created = float(lease["created"])
        expires = float(lease["expires"])
        controller_pid = int(lease["controller_pid"])
        if expires - created <= 0 or expires - created > 12:
            raise ValueError("lease_out_of_bounds")
        if time.time() - created > 12:
            raise ValueError("lease_stale")
        why = "lease_expired"
        for _ in range(80):
            if not pid_alive(controller_pid):
                why = "controller_died"
                break
            if time.time() >= expires:
                break
            time.sleep(0.10)
        snap = live_snapshot()
        actual = decide_return(snap)
        hypothetical = decide_return(snap, TEST_MODE)
        result = {
            "stage": "simulated_hdmi_recovery",
            "trigger": why,
            "live_display_safe": (snap.get("ok") and
                                  equal_modes(snap["current"], ORIGINAL_MODE)),
            "actual_decision": actual,
            "synthetic_decision": hypothetical,
            "display_set_calls": 0,
            "stream_calls": 0,
            "simulated": True,
            "live_restoration_qualified": False,
        }
        audit_write(audit_file, result)
        return 0 if (result["live_display_safe"] and
                     actual == "ALREADY_ORIGINAL" and
                     hypothetical == "RETURN_TO_ORIGINAL_REQUIRED") else 31
    except Exception as ex:
        audit_write(audit_file, {"stage": "simulated_hdmi_recovery",
                                 "result": "FAILED",
                                 "reason": str(ex),
                                 "display_set_calls": 0,
                                 "live_restoration_qualified": False})
        return 32


def dry_run():
    ensure_sierra_host()
    before = live_snapshot()
    if not before["ok"] or not equal_modes(before["current"], ORIGINAL_MODE):
        print("stage=guard_preflight result=fail snapshot=%s" % before)
        return 22
    if not before["has_p25"] or not before["has_p30"]:
        print("stage=guard_preflight result=missing_standard_mode")
        return 23
    print("stage=guard_preflight result=pass source=BMD_HDMI"
          " current=1080p30 candidate=1080p25 recovery_target=1080p30")
    sys.stdout.flush()
    root = tempfile.mkdtemp(prefix="fc-hdmi-watchdog-dry-")
    controller = None
    watchdog = None
    try:
        lease_file = os.path.join(root, "lease.json")
        audit_file = os.path.join(root, "audit.json")
        # A harmless dummy process stands in for a hung/failed test controller.
        controller = subprocess.Popen(
            [sys.executable, "-c", "import time;time.sleep(30)"],
            stdin=open(os.devnull, "rb"),
            stdout=open(os.devnull, "wb"),
            stderr=subprocess.STDOUT)
        now = time.time()
        audit_write(lease_file, {
            "contract": "FC_SIERRA_HDMI_GUARD_SIMULATION_V1",
            "fixture": "pretend_source_1080p25",
            "created": now, "expires": now + 5.0,
            "controller_pid": controller.pid})
        watchdog = subprocess.Popen(
            [sys.executable, os.path.abspath(__file__), "--watch-only",
             lease_file, audit_file],
            stdin=open(os.devnull, "rb"),
            stdout=open(os.devnull, "wb"),
            stderr=subprocess.STDOUT,
            close_fds=True)
        # Failure injection: SIGKILL, not a graceful controller return.
        time.sleep(0.3)
        os.kill(controller.pid, signal.SIGKILL)
        controller.wait()
        print("stage=guard_fault_injection result=controller_killed"
              " separate_watchdog_pid=%d" % watchdog.pid)
        sys.stdout.flush()
        for _ in range(75):
            if os.path.exists(audit_file):
                break
            time.sleep(0.10)
        if not os.path.exists(audit_file):
            print("stage=guard_observation result=timed_out")
            return 40
        audit = read_json(audit_file)
        watchdog.wait()
        after = live_snapshot()
        passed = (watchdog.returncode == 0 and
                  audit.get("trigger") == "controller_died" and
                  audit.get("actual_decision") == "ALREADY_ORIGINAL" and
                  audit.get("synthetic_decision") == "RETURN_TO_ORIGINAL_REQUIRED"
                  and audit.get("display_set_calls") == 0 and
                  audit.get("live_restoration_qualified") is False and
                  after.get("ok") and
                  equal_modes(after["current"], ORIGINAL_MODE))
        print("stage=guard_observation result=%s"
              " trigger=%s actual=%s simulated25=%s"
              " display_writes=0 video_streams=0" %
              ("pass" if passed else "fail", audit.get("trigger"),
               audit.get("actual_decision"),
               audit.get("synthetic_decision")))
        print("stage=qualification dry_run_only=1"
              " physical_restore_from_1080p25_NOT_TESTED=1"
              " live_mode_change_authorized=0")
        return 0 if passed else 41
    finally:
        if controller is not None and controller.poll() is None:
            controller.kill()
            controller.wait()
        if watchdog is not None and watchdog.poll() is None:
            watchdog.kill()
            watchdog.wait()
        shutil.rmtree(root)


def inspect():
    try:
        s = live_snapshot()
        print("stage=guard_inspect bmd_present=%s"
              " current=%s mode_1080p25=%s mode_1080p30=%s"
              " mode_change_calls=0" %
              (s.get("ok"), s.get("current"),
               s.get("has_p25"), s.get("has_p30")))
        return 0 if s.get("ok") and equal_modes(s["current"], ORIGINAL_MODE) else 21
    except Exception as ex:
        print("stage=guard_inspect result=unavailable reason=%s" % ex)
        return 20


def main(argv):
    if argv == ["--inspect"]:
        return inspect()
    if argv == ["--dry-run"]:
        try:
            return dry_run()
        except Exception as ex:
            print("stage=guard_dry_run result=exception reason=%s" % ex)
            return 33
    if len(argv) == 3 and argv[0] == "--watch-only":
        return watch_only(argv[1], argv[2])
    print("usage: %s --inspect|--dry-run" % os.path.basename(__file__))
    return 64


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
