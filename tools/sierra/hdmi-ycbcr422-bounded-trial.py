#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Retired legacy cmod colour trial — safe compatibility entrypoint.

On Sierra IOGraphics 515.x the selected cmod property is not the write
surface, and nonautomatic cyuv requests select RGB, not YCbCr 4:2:2.

No HDMI modes, pixel colour, DeckLink streams, PCI or firmware changes.
The only supported restoration path is the adjacent source-owned cyuv
controller's --restore-only action, which restores original cyuv=auto.
The legacy watchdog and 4:2:2 test are permanently blocked.
"""
from __future__ import print_function
import os
import subprocess
import sys

HERE=os.path.dirname(os.path.abspath(__file__))
CANONICAL=os.path.join(HERE,"hdmi-ycbcr422-cyuv-bounded-trial.py")
# The original Sierra diagnostics are installed under /tmp with an fc- prefix;
# the repository stores matching scripts under tools/sierra without it.
# Never search PATH or silently invoke a different program for restoration.
if os.path.basename(os.path.abspath(__file__)).startswith("fc-"):
    CANONICAL=os.path.join(HERE,"fc-hdmi-ycbcr422-cyuv-bounded-trial.py")


def run_trial():
    """Retired: never change source colour from this entrypoint."""
    raise RuntimeError("IOGraphics_RGB_rewrite_blocks_422_trials")


def main(argv):
    if argv==["--trial-422"]:
        print("stage=color_trial result=BLOCKED_IOGRAPHICS_RGB_REWRITE"
              " no_source_change=1 no_vendor_calls=1"
              " restore_only_remains_available=1")
        return 65
    if len(argv)>=1 and argv[0]=="--watchdog":
        print("stage=legacy_color_watchdog result=BLOCKED_RETIRED_CMOD_PATH"
              " no_io=1")
        return 65
    if argv==["--preflight"]:
        print("stage=legacy_color_preflight result=BLOCKED_RETIRED_CMOD_PATH no_io=1")
        return 65
    if argv==["--restore-only"]:
        if not os.path.isfile(CANONICAL):
            print("stage=legacy_color_route result=CANONICAL_CYUV_HELPER_MISSING")
            return 66
        return subprocess.call([sys.executable,CANONICAL]+argv)
    print("usage: --preflight|--restore-only|--trial-422 (disabled)"
          "|--watchdog (disabled)")
    return 64


if __name__=="__main__":
    sys.exit(main(sys.argv[1:]))
