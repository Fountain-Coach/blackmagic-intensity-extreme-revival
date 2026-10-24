#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Retired Sierra cmod-based HDMI 4:2:2 experiment.

The original script attempted to set cmod, which is the *reported selected
colour*, not a supported user-controlled 422 output setting. The actual
cyuv request was later observed to select RGB via Apple IOGraphics 515.x.

This compatibility command refuses all experiments. It preserves only a
delegation to the current tested cyuv=automatic restoration command.
Source and hardware are never modified by the blocked routes.
"""
from __future__ import print_function
import os
import subprocess
import sys

RECOVERY="/tmp/fc-hdmi-ycbcr422-cyuv-bounded-trial.py"


def run_trial():
    raise RuntimeError("IOGraphics_RGB_rewrite_blocks_422_trials")


def main(argv):
    if argv==["--restore-only"]:
        if not os.path.isfile(RECOVERY):
            print("stage=legacy_cmod result=BLOCKED missing_safe_recovery")
            return 65
        return subprocess.call([sys.executable, RECOVERY, "--restore-only"])
    if argv in (["--trial-422"],["--preflight"]) or (
            len(argv)>0 and argv[0]=="--watchdog"):
        print("stage=legacy_cmod result=BLOCKED_RETIRED_CMOD_EXPERIMENT"
              " no_source_change=1 no_vendor_calls=1"
              " recovery_via=hdmi-ycbcr422-cyuv-bounded-trial.py")
        return 65
    print("usage: --restore-only (delegated safe cyuv automatic restoration);"
          " all cmod experiments retired")
    return 64


if __name__=="__main__":
    sys.exit(main(sys.argv[1:]))
