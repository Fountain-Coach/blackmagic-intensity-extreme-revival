#!/usr/bin/env python2.7
# -*- coding: utf-8 -*-
"""Bounded HDMI timing retrain of the SAME original silver Mac display.

The physical source is ALWAYS the built-in HDMI OUT of Macmini5,3, connected
to the original Blackmagic Intensity Extreme HDMI IN, with its HDMI OUT
connected to the user's monitor. Does not change source, cabling, EDID bytes,
Thunderbolt drivers, firmware, Linux, video privacy, or system preferences.

Explicit --pulse-25-to-original-30 requests a supported 1080p25 display
mode for <=0.8s, then restores the exact CGDisplayModeRef that was active
(1920x1080@30). This is a reversible HDMI link re-clock diagnostic ONLY.
A safe fallback --restore-30-if-needed is available through SSH if a
process is abnormally terminated while the temporary 25Hz mode is active.
Do not select another display, a substitute source, or new resolution.
"""
from __future__ import print_function
import argparse
import os
import platform
import signal
import subprocess
import sys
import time
import Quartz

SOURCE_VENDOR = 8961
W,H,REFRESH = 1920,1080,30.0


def hz(mode):
    return float(Quartz.CGDisplayModeGetRefreshRate(mode))


def dimensions(mode):
    return (Quartz.CGDisplayModeGetWidth(mode),
            Quartz.CGDisplayModeGetHeight(mode))


def get_source():
    system=platform.mac_ver()[0]
    machine=subprocess.check_output(["sysctl","-n","hw.model"]).strip()
    if system!="10.12.6" or machine!="Macmini5,3":
        raise RuntimeError("Only original silver Sierra Macmini5,3 permitted")
    rc,ids,count=Quartz.CGGetOnlineDisplayList(16,None,None)
    if rc!=0 or count!=1:
        raise RuntimeError("Expected exactly one online source display")
    display=ids[0]
    if Quartz.CGDisplayVendorNumber(display)!=SOURCE_VENDOR or \
       not Quartz.CGDisplayIsMain(display) or \
       not Quartz.CGDisplayIsOnline(display):
        raise RuntimeError("Original BMD HDMI display missing or not main")
    original=Quartz.CGDisplayCopyDisplayMode(display)
    if not original or dimensions(original)!=(W,H):
        raise RuntimeError("Incorrect HDMI source resolution")
    if abs(hz(original)-REFRESH)>.01:
        raise RuntimeError("Original source is not 1080p30")
    return display,original


def matching(display,refresh):
    modes=Quartz.CGDisplayCopyAllDisplayModes(display,None)
    found=[m for m in modes if dimensions(m)==(W,H)
           and abs(hz(m)-refresh)<.01]
    if not found:
        raise RuntimeError("Expected CEA timing not offered by original HDMI")
    return found[0]


def apply(display,mode,name):
    rc=Quartz.CGDisplaySetDisplayMode(display,mode,None)
    now=Quartz.CGDisplayCopyDisplayMode(display)
    got=hz(now) if now else -1
    expected=hz(mode)
    print("stage=silver_link_retrain action=%s cg_rc=%d "
          "expected_hz=%.3f observed_hz=%.3f source_unchanged=1" %
          (name,rc,expected,got))
    sys.stdout.flush()
    if rc!=0 or not now or dimensions(now)!=(W,H) or \
       abs(got-expected)>.01:
        raise RuntimeError("Mac HDMI mode operation did not verify")


def interrupt(sig,frame):
    raise KeyboardInterrupt("signal=%d" % sig)


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("operation",choices=["--check","--pulse-25-to-original-30",
                                              "--restore-30-if-needed"])
    # argparse treats leading -- positional values as option-like; strip and
    # validate explicitly instead of relying on argparse's option parser.
    return parser


def run(argv):
    if len(argv)!=1 or argv[0] not in (
        "--check","--pulse-25-to-original-30","--restore-30-if-needed"):
        print("usage: fixed_silver_hdmi_retrain.py --check|--pulse-25-to-original-30|--restore-30-if-needed",
              file=sys.stderr)
        return 64
    operation=argv[0]
    # Restore is distinct: if a previous process crashed at 25Hz, don't
    # require current mode to equal 30 before allowing a safe recovery.
    if operation=="--restore-30-if-needed":
        system=platform.mac_ver()[0]
        machine=subprocess.check_output(["sysctl","-n","hw.model"]).strip()
        if system!="10.12.6" or machine!="Macmini5,3":
            return 65
        rc,ids,count=Quartz.CGGetOnlineDisplayList(16,None,None)
        if rc!=0 or count!=1 or Quartz.CGDisplayVendorNumber(ids[0])!=SOURCE_VENDOR:
            return 66
        display=ids[0]
        current=Quartz.CGDisplayCopyDisplayMode(display)
        if not current or dimensions(current)!=(W,H):
            return 67
        if abs(hz(current)-30)<.01:
            print("stage=silver_link_retrain recovery=already_1080p30")
            return 0
        if abs(hz(current)-25)>.01:
            return 68
        apply(display,matching(display,30),"recovery_original_1080p30")
        return 0
    display,original=get_source()
    candidate=matching(display,25)
    print("stage=silver_link_retrain fixture=silver_MAC_HDMI_OUT_to_BMD_IN "
          "output_monitor_unchanged=1 original=%.3fHz temporary=%.3fHz" %
          (hz(original),hz(candidate)))
    sys.stdout.flush()
    if operation=="--check":
        print("stage=silver_link_retrain result=READY physical_source_unchanged=1")
        return 0
    signal.signal(signal.SIGINT,interrupt)
    signal.signal(signal.SIGTERM,interrupt)
    switched=False
    failure=None
    try:
        apply(display,candidate,"brief_1080p25")
        switched=True
        time.sleep(0.7)
    except BaseException as e:
        failure=e
    finally:
        # Do not rely on a process end to restore the video link.
        try:
            apply(display,original,"restore_exact_original_1080p30")
            switched=False
        except BaseException as restoration:
            print("stage=silver_link_retrain result=RESTORE_FAILED error=%s" %
                  type(restoration).__name__,file=sys.stderr)
            return 71
    if failure:
        print("stage=silver_link_retrain result=FAILED error=%s" %
              type(failure).__name__,file=sys.stderr)
        return 72
    print("stage=silver_link_retrain result=RESTORED_SOURCE_1080p30 "
          "input_lock_not_implied=1 physical_source_same=1")
    return 0


if __name__=="__main__":
    try:
        sys.exit(run(sys.argv[1:]))
    except Exception as e:
        print("stage=silver_link_retrain result=NO_GO error=%s" %
              type(e).__name__,file=sys.stderr)
        sys.exit(73)
