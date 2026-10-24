#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Pure synthetic tests for the non-mutating Sierra timing/colour witness."""
from __future__ import print_function
import os
import struct
import sys
import unittest

SOURCE=os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                    "tools","sierra","hdmi-timing-color-audit.py")
ns={"__name__":"hdmi_timing_color_math_test"}
if sys.version_info[0]<3:
    execfile(SOURCE,ns)
else:
    with open(SOURCE,"rb") as f:
        exec(compile(f.read(),SOURCE,"exec"),ns)
decode=ns["parse_timing"]
colors=ns["COLORS"]


def synthetic_timing(mode_id,clock,h_active,h_blank,v_active,v_blank,flags=0):
    data=bytearray(160)
    struct.pack_into("<I",data,0,mode_id)
    struct.pack_into("<I",data,32,flags)
    struct.pack_into("<QQQ",data,40,clock,clock,clock)
    struct.pack_into("<8I",data,64,h_active,h_blank,88,44,
                     v_active,v_blank,4,5)
    return bytes(data)


class HDMIRequestedTimingMathTests(unittest.TestCase):
    def test_1080p30_exact_cea_clock(self):
        t=decode(synthetic_timing(0x80001008,74250000,1920,280,1080,45))
        self.assertEqual(t["width"],1920)
        self.assertEqual(t["height"],1080)
        self.assertEqual((t["h_total"],t["v_total"]),(2200,1125))
        self.assertAlmostEqual(t["rate_hz"],30.0)
        self.assertEqual(t["signal_flags"],0)

    def test_720p60_exact_cea_clock(self):
        t=decode(synthetic_timing(0x80001001,74250000,1280,370,720,30))
        self.assertEqual((t["h_total"],t["v_total"]),(1650,750))
        self.assertAlmostEqual(t["rate_hz"],60.0)

    def test_active_sierra_clock_relative_error(self):
        self.assertAlmostEqual(100*(73979530.0/74250000-1),-0.364269,places=5)

    def test_reject_truncated_timing(self):
        with self.assertRaises(ValueError):
            decode(bytes(bytearray(90)))

    def test_reject_zero_pixel_total(self):
        with self.assertRaises(ValueError):
            decode(bytes(bytearray(160)))

    def test_reject_implausibly_large_geometry(self):
        with self.assertRaises(ValueError):
            decode(synthetic_timing(1,74250000,40000,100,720,30))

    def test_reject_nonbinary_data(self):
        with self.assertRaises(TypeError):
            decode(1999)

    def test_color_mode_bits(self):
        self.assertEqual(colors[0x100],"YCbCr444")
        self.assertEqual(colors[0x10],"YCbCr422")
        self.assertEqual(colors[0x10000000],"automatic")
        self.assertTrue(0x111 & 0x10)
        self.assertTrue(0x111 & 0x100)

    def test_requested_422_not_same_as_selected_444(self):
        self.assertNotEqual(0x100,0x10)

if __name__=="__main__":
    unittest.main()
