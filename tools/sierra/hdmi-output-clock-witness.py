#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Read-only Sierra HDMI output clock and display-identity witness.

Uses public CoreGraphics and I/O Registry metadata. Never configures a display,
opens DeckLink, accesses PCI registers, captures images or measures HDMI wires.
Run: python tools/sierra/hdmi-output-clock-witness.py
Copyright (c) 2026 Fountain Coach contributors. MIT.
"""
from __future__ import print_function

import ctypes
import os
import plistlib
import subprocess
import sys

BMD_VENDOR = 8961
EXPECTED_MODEL = "Macmini5,3"
EXPECTED_OS = "10.12.6"


def ratio_hz(clock_hz, pixels_per_frame):
    if not isinstance(clock_hz, (int, long_type)) or not isinstance(pixels_per_frame, (int, long_type)):
        raise ValueError("clock/count must be positive integers")
    if clock_hz <= 0 or pixels_per_frame <= 0:
        raise ValueError("clock/count must be positive integers")
    return float(clock_hz) / float(pixels_per_frame)


def relative_percent(actual, nominal):
    if nominal <= 0:
        raise ValueError("nominal must be positive")
    return 100.0 * (actual / nominal - 1.0)


try:
    long_type = long
except NameError:
    long_type = int


def decode_plist(data):
    if hasattr(plistlib, "loads"):
        return plistlib.loads(data)
    return plistlib.readPlistFromString(data)


def get_coregraphics():
    cg = ctypes.CDLL("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")
    u32 = ctypes.c_uint32
    cg.CGGetOnlineDisplayList.argtypes = [u32, ctypes.POINTER(u32), ctypes.POINTER(u32)]
    cg.CGGetOnlineDisplayList.restype = ctypes.c_int
    cg.CGOpenGLDisplayMaskToDisplayID.argtypes = [u32]
    cg.CGOpenGLDisplayMaskToDisplayID.restype = u32
    cg.CGDisplayVendorNumber.argtypes = [u32]
    cg.CGDisplayVendorNumber.restype = u32
    cg.CGDisplayModelNumber.argtypes = [u32]
    cg.CGDisplayModelNumber.restype = u32
    cg.CGDisplayCopyDisplayMode.argtypes = [u32]
    cg.CGDisplayCopyDisplayMode.restype = ctypes.c_void_p
    cg.CGDisplayModeGetWidth.argtypes = [ctypes.c_void_p]
    cg.CGDisplayModeGetWidth.restype = ctypes.c_size_t
    cg.CGDisplayModeGetHeight.argtypes = [ctypes.c_void_p]
    cg.CGDisplayModeGetHeight.restype = ctypes.c_size_t
    cg.CGDisplayModeGetRefreshRate.argtypes = [ctypes.c_void_p]
    cg.CGDisplayModeGetRefreshRate.restype = ctypes.c_double
    cg.CGDisplayModeRelease.argtypes = [ctypes.c_void_p]
    return cg


def main():
    if len(sys.argv) != 1:
        print("usage: python hdmi-output-clock-witness.py")
        return 64
    if sys.platform != "darwin":
        print("stage=source_timing result=unsupported_host")
        return 20
    model = subprocess.check_output(["sysctl", "-n", "hw.model"]).strip()
    version = subprocess.check_output(["sw_vers", "-productVersion"]).strip()
    if model != EXPECTED_MODEL.encode("ascii") or version != EXPECTED_OS.encode("ascii"):
        print("stage=source_timing result=unexpected_host")
        return 21

    cg = get_coregraphics()
    u32 = ctypes.c_uint32
    display_ids = (u32 * 16)()
    count = u32()
    rc = cg.CGGetOnlineDisplayList(16, display_ids, ctypes.byref(count))
    if rc != 0:
        print("stage=source_timing result=display_inventory_failed rc=%d" % rc)
        return 22
    bmd_ids = [display_ids[i] for i in range(count.value)
               if cg.CGDisplayVendorNumber(display_ids[i]) == BMD_VENDOR
               and cg.CGDisplayModelNumber(display_ids[i]) == 0]
    if count.value != 1 or len(bmd_ids) != 1:
        print("stage=source_timing result=unexpected_display_topology online=%d bmd=%d" %
              (count.value, len(bmd_ids)))
        return 23
    display_id = bmd_ids[0]
    mode = cg.CGDisplayCopyDisplayMode(display_id)
    if not mode:
        print("stage=source_timing result=no_active_mode")
        return 24
    width = cg.CGDisplayModeGetWidth(mode)
    height = cg.CGDisplayModeGetHeight(mode)
    ui_hz = cg.CGDisplayModeGetRefreshRate(mode)
    cg.CGDisplayModeRelease(mode)
    print("stage=source_display result=BMD_HDMI width=%d height=%d mode_rate_hz=%.6f" %
          (width, height, ui_hz))
    if (width, height) != (1920, 1080) or abs(ui_hz - 30.0) > 0.01:
        print("stage=source_timing result=not_1080p30")
        return 25

    roots = decode_plist(subprocess.check_output(
        ["/usr/sbin/ioreg", "-a", "-r", "-c", "AppleIntelFramebuffer"]))
    matches = []
    for node in roots:
        index = node.get("IOFramebufferOpenGLIndex")
        clk = node.get("IOFBCurrentPixelClock")
        cnt = node.get("IOFBCurrentPixelCount")
        if not isinstance(index, (int, long_type)) or index < 0 or index >= 32:
            continue
        if cg.CGOpenGLDisplayMaskToDisplayID(u32(1 << index)) != display_id:
            continue
        if clk is None or cnt is None:
            print("stage=source_timing result=matched_framebuffer_no_clock")
            return 26
        matches.append((index, clk, cnt, node.get("IOFBCurrentPixelCountReal")))
    if len(matches) != 1:
        print("stage=source_timing result=framebuffer_mapping_ambiguous matches=%d" %
              len(matches))
        return 27
    index, clock_hz, pixel_count, real_count = matches[0]
    try:
        estimated_hz = ratio_hz(clock_hz, pixel_count)
    except ValueError:
        print("stage=source_timing result=invalid_registry_counts")
        return 28
    print("stage=framebuffer_mapping result=matched source_vendor=BMD pci_or_dma_access=0 gl_index=%d" % index)
    print("stage=source_registry_clock clock_hz=%d pixel_count=%d real_pixel_count=%s derived_hz=%.6f" %
          (clock_hz, pixel_count, str(real_count), estimated_hz))
    print("stage=nominal_comparison ui_mode_hz=%.6f deviation_vs_30_percent=%.6f deviation_vs_29_97_percent=%.6f" %
          (ui_hz, relative_percent(estimated_hz, 30.0),
           relative_percent(estimated_hz, 30000.0 / 1001.0)))
    print("stage=limits registry_derived_not_wire_measured=1 actual_color_encoding=unknown hdmi_input_lock=not_tested frame_pixels=not_tested")
    print("stage=safety capture=0 display_mode_changes=0 vendor_api=0 firmware_driver_changes=0 screenshots=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
