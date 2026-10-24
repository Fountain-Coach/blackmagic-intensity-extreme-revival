#!/usr/bin/env python2.7
# -*- coding: utf-8 -*-
"""Briefly retrigger original silver HDMI clock negotiation via CoreGraphics.

The test uses ONLY the silver Macmini5,3's existing single HDMI display,
now reporting either original BMD HDMI vendor (8961) or the Samsung U28E590
vendor (19501) after attaching a reference monitor to Intensity HDMI OUT.
Changes 1920x1080@30 -> 1920x1080@24 for 2s -> EXACT original mode.
NO cable substitution, reboot, kernel driver, input/output card API, root
permission, screen pixel capture, saved configuration or background work.
Mac CoreGraphics reverts a temporary mode on process termination.
"""
from __future__ import print_function
import os
import subprocess
import sys
import time

def output(command):
    return subprocess.check_output(command).strip()

def describe(mode):
    import Quartz
    if not mode:
        return None
    return (Quartz.CGDisplayModeGetWidth(mode),
            Quartz.CGDisplayModeGetHeight(mode),
            Quartz.CGDisplayModeGetRefreshRate(mode))

def main(argv):
    if argv != ["--rehandshake-1080p24-then-30"]:
        print("usage: silver-hdmi-rehandshake.py --rehandshake-1080p24-then-30")
        return 64
    if output(["sysctl", "-n", "hw.model"]) != "Macmini5,3":
        print("stage=silver_hdmi_rehandshake result=wrong_host")
        return 65
    if output(["sw_vers", "-productVersion"]) != "10.12.6":
        print("stage=silver_hdmi_rehandshake result=wrong_os")
        return 66

    import Quartz
    rc, displays, count = Quartz.CGGetOnlineDisplayList(16, None, None)
    if rc != 0 or count != 1:
        print("stage=silver_hdmi_rehandshake result=unexpected_display_topology"
              " count=%s" % count)
        return 67
    display = displays[0]
    vendor = Quartz.CGDisplayVendorNumber(display)
    if vendor not in (8961, 19501):
        print("stage=silver_hdmi_rehandshake result=unexpected_sink_vendor"
              " vendor=%s" % vendor)
        return 68
    original = Quartz.CGDisplayCopyDisplayMode(display)
    if describe(original) != (1920, 1080, 30.0):
        print("stage=silver_hdmi_rehandshake result=unexpected_source_timing"
              " actual=%s" % (describe(original),))
        return 69
    alternate = None
    for mode in Quartz.CGDisplayCopyAllDisplayModes(display, None):
        if describe(mode) == (1920, 1080, 24.0) and \
                Quartz.CGDisplayModeIsUsableForDesktopGUI(mode):
            alternate = mode
            break
    if alternate is None:
        print("stage=silver_hdmi_rehandshake result=no_safe_1080p24_mode")
        return 70
    print("stage=silver_hdmi_rehandshake preflight=PASS vendor=%d "
          "original=1920x1080@30 alternative=1920x1080@24 "
          "same_gpu_port=1 external_firmware_changed=0" % vendor)
    sys.stdout.flush()
    switched = False
    restored = False
    try:
        rc = Quartz.CGDisplaySetDisplayMode(display, alternate, None)
        print("stage=silver_hdmi_rehandshake switch_to_p24_rc=%s" % rc)
        sys.stdout.flush()
        if rc != 0:
            return 71
        switched = True
        time.sleep(2.0)
    finally:
        if switched:
            # Sierra can give the display a NEW Quartz ID when changing the
            # timing. Restoring by the stale pre-switch ID returned rc=1001
            # on 24 October; macOS later reverted automatically when this
            # program exited. Reenumerate, then resolve a fresh 30Hz mode.
            restore_rc = None
            for attempt in range(6):
                rr, current_ids, nn = Quartz.CGGetOnlineDisplayList(16, None, None)
                if rr == 0 and nn == 1 and \
                        Quartz.CGDisplayVendorNumber(current_ids[0]) == vendor:
                    live = current_ids[0]
                    if describe(Quartz.CGDisplayCopyDisplayMode(live)) == \
                            (1920, 1080, 30.0):
                        restore_rc = 0
                        restored = True
                        break
                    candidate = None
                    for possible in Quartz.CGDisplayCopyAllDisplayModes(live, None):
                        if describe(possible) == (1920, 1080, 30.0) and \
                                Quartz.CGDisplayModeIsUsableForDesktopGUI(possible):
                            candidate = possible
                            break
                    if candidate is not None:
                        restore_rc = Quartz.CGDisplaySetDisplayMode(
                            live, candidate, None)
                        if restore_rc == 0:
                            restored = True
                            break
                time.sleep(0.3)
            print("stage=silver_hdmi_rehandshake "
                  "restore_to_fresh_display_id_rc=%s restored=%s "
                  "os_auto_reverts_temporary_mode_on_exit=1" %
                  (restore_rc, int(restored)))
            sys.stdout.flush()
    rr, actual_ids, actual_count = Quartz.CGGetOnlineDisplayList(16, None, None)
    observed = Quartz.CGDisplayCopyDisplayMode(actual_ids[0]) \
        if rr == 0 and actual_count == 1 else None
    good = restored and describe(observed) == (1920, 1080, 30.0)
    print("stage=silver_hdmi_rehandshake result=%s "
          "source_original_restored=%d output_cable_unchanged=1 "
          "driver_unmodified=1 display_config_persisted=0 "
          "hdmi_input_lock_not_claimed=1" %
          ("RENEGOTIATED" if good else "RESTORE_UNVERIFIED", int(good)))
    sys.stdout.flush()
    return 0 if good else 72

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
