#!/usr/bin/env python
# Portable, synthetic timing math tests. No hardware access.
from __future__ import print_function
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WITNESS = os.path.join(ROOT, "tools", "sierra", "hdmi-output-clock-witness.py")
namespace = {"__name__": "hdmi_output_clock_math_tests"}
if sys.version_info[0] < 3:
    execfile(WITNESS, namespace)
else:
    with open(WITNESS, "rb") as f:
        exec(compile(f.read(), WITNESS, "exec"), namespace)
ratio_hz = namespace["ratio_hz"]
relative_percent = namespace["relative_percent"]

class SourceClockMathTests(unittest.TestCase):
    def test_standard_1080p30_cea_clock(self):
        self.assertAlmostEqual(ratio_hz(74250000, 2200*1125), 30.0, places=8)

    def test_observed_sierra_macmini_clock(self):
        self.assertAlmostEqual(ratio_hz(73979530, 2475000), 29.89071919, places=6)

    def test_deviation_from_exact_30(self):
        self.assertAlmostEqual(relative_percent(ratio_hz(73979530,2475000), 30.0), -0.364269, places=5)

    def test_deviation_from_29_97(self):
        value = relative_percent(ratio_hz(73979530,2475000),30000.0/1001.0)
        self.assertAlmostEqual(value,-0.264634,places=5)

    def test_pixel_count_and_clock_must_be_positive(self):
        for clk,pc in ((0,2475000),(74250000,0),(-1,2475000),(74250000,-10)):
            with self.assertRaises(ValueError):
                ratio_hz(clk,pc)

    def test_noninteger_clock_rejected(self):
        for clk,pc in ((74250000.0,2475000),("74250000",2475000),(74250000,"2475000")):
            with self.assertRaises(ValueError):
                ratio_hz(clk,pc)

    def test_nominal_comparison_requires_positive(self):
        with self.assertRaises(ValueError):
            relative_percent(30.0,0.0)

if __name__ == "__main__":
    unittest.main()
