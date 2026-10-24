#!/usr/bin/env python2.7
# -*- coding: utf-8 -*-
"""Write source-owned guest reader tools ONLY into a reserved mailbox sector.

No guest ext4, operating-system disk or mounted filesystem is modified by
Sierra. The reserved area begins past the maximum JPEG payload in slot zero.
Guest reads the bundle over the readonly virtio block at startup and installs
those public source files into its persistent guest ext4, after verifying SHA.
"""
from __future__ import print_function
import argparse
import hashlib
import os
import stat
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
from simulator.hdmi_frame_mailbox import (DISK_BYTES, HEADER_BYTES,
                                          MAX_FRAME_BYTES, SLOT_BYTES, write_all)

META_OFF = HEADER_BYTES + MAX_FRAME_BYTES    # offset 266240, sector 65
BUNDLE_OFF = META_OFF + HEADER_BYTES         # sector 66
MAX_BUNDLE = SLOT_BYTES - BUNDLE_OFF         # 253952, slot0 ONLY


def main():
    a = argparse.ArgumentParser()
    a.add_argument("--mailbox", required=True)
    args = a.parse_args()
    info = os.stat(args.mailbox)
    if not stat.S_ISREG(info.st_mode) or info.st_size != DISK_BYTES or \
       info.st_mode & 0o077 or info.st_nlink != 1:
        a.error("private, dedicated raw mailbox required")
    data = sys.stdin.read(MAX_BUNDLE + 1)
    if not (4 < len(data) <= MAX_BUNDLE and data.startswith("\x1f\x8b")):
        a.error("complete, bounded gzip bundle required")
    digest = hashlib.sha256(data).hexdigest()
    record = "FCHBOOT1 %d %s\n" % (len(data), digest)
    fd = os.open(args.mailbox, os.O_RDWR)
    try:
        # A single writer fills the reserved zone only, leaving image slots
        # and any existing frame sequence headers untouched.
        write_all(fd, data, BUNDLE_OFF)
        os.fsync(fd)
        write_all(fd, record.ljust(HEADER_BYTES, "\x00"), META_OFF)
        os.fsync(fd)
    finally:
        os.close(fd)
    print("stage=hdmi_mailbox_bootstrap result=written "
          "bundle_bytes=%d sha256=%s header_sector=65 payload_sector=66" %
          (len(data), digest))


if __name__ == "__main__":
    main()
