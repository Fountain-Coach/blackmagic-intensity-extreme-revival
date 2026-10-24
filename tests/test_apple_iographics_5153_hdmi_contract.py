#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Pure IOGraphics 515.3 policy contract and driver clock-data provenance.

The public Apple IOGraphics source tag IOGraphics-515.3 is the nearest
available source for Sierra's installed IOGraphicsFamily 515.4.

References (not downloaded at runtime):
https://github.com/apple-oss-distributions/IOGraphics/blob/IOGraphics-515.3/IOGraphicsFamily/IOFramebuffer.cpp
    IOFramebufferParameterHandler::doIntegerSet
    IOFramebuffer::doSetup
    AUTO_COLOR_MODE constant

This test is a source-contract model, NOT proof of the precise 515.4 binary
or actual HDMI wire values. It does not import IOKit or change a machine.
"""
from __future__ import print_function
import unittest

RGB=0x1
YCbCR422=0x10
YCbCR444=0x100
AUTO=0x10000000
REQUESTED_PIXEL_CLOCK_HZ=74250000
REPORTED_DISPLAY_CLOCK_HZ=73979530


def iographics_5153_selected_color(request, ycbcr_allowed, supported_mask):
    """Mirror the documented IOGraphics 515.3 automatic/otherwise branch."""
    if request==AUTO and ycbcr_allowed and (YCbCR444 & supported_mask):
        return YCbCR444
    return RGB


def clock_from_driver_timing_info(timing_clock_hz,
                                  min_pixel_clock_hz,
                                  max_pixel_clock_hz,
                                  vbl_throttle_enabled):
    """Model the IOFBCurrentPixelClock property source in doSetup."""
    permitted=(min_pixel_clock_hz==max_pixel_clock_hz
               and min_pixel_clock_hz>0)
    if vbl_throttle_enabled and permitted and timing_clock_hz>0:
        return min_pixel_clock_hz
    return timing_clock_hz


class IOGraphics5153ContractTests(unittest.TestCase):
    def test_auto_can_select_444(self):
        self.assertEqual(
            iographics_5153_selected_color(AUTO,True,0x111),YCbCR444)

    def test_explicit_422_request_becomes_rgb_even_when_supported(self):
        self.assertEqual(
            iographics_5153_selected_color(YCbCR422,True,0x111),RGB)

    def test_explicit_444_request_also_becomes_rgb(self):
        self.assertEqual(
            iographics_5153_selected_color(YCbCR444,True,0x111),RGB)

    def test_auto_without_allowed_ycbcr_becomes_rgb(self):
        self.assertEqual(
            iographics_5153_selected_color(AUTO,False,0x111),RGB)

    def test_auto_without_supported_444_becomes_rgb(self):
        self.assertEqual(
            iographics_5153_selected_color(AUTO,True,RGB|YCbCR422),RGB)

    def test_reported_clock_property_can_reflect_driver_timing_info(self):
        self.assertEqual(
            clock_from_driver_timing_info(REPORTED_DISPLAY_CLOCK_HZ,
                                          REQUESTED_PIXEL_CLOCK_HZ,
                                          REQUESTED_PIXEL_CLOCK_HZ,False),
            REPORTED_DISPLAY_CLOCK_HZ)

    def test_throttle_can_use_driver_min_clock_if_min_equals_max(self):
        self.assertEqual(
            clock_from_driver_timing_info(REPORTED_DISPLAY_CLOCK_HZ,
                                          REQUESTED_PIXEL_CLOCK_HZ,
                                          REQUESTED_PIXEL_CLOCK_HZ,True),
            REQUESTED_PIXEL_CLOCK_HZ)

    def test_unequal_clock_limits_do_not_override_timing_clock(self):
        self.assertEqual(
            clock_from_driver_timing_info(REPORTED_DISPLAY_CLOCK_HZ,
                                          74100000,74300000,True),
            REPORTED_DISPLAY_CLOCK_HZ)

    def test_7425MHz_requested_is_not_7398MHz_registry(self):
        self.assertNotEqual(REQUESTED_PIXEL_CLOCK_HZ,REPORTED_DISPLAY_CLOCK_HZ)
        self.assertAlmostEqual(
            100*(float(REPORTED_DISPLAY_CLOCK_HZ)/
                 REQUESTED_PIXEL_CLOCK_HZ-1.0),-0.364269,places=5)


if __name__=="__main__":
    unittest.main()
