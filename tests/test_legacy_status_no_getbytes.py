#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Protect Sierra against the original Desktop Video 10.4 GetBytes crash.

The original 10.4 CDeckLinkStatus::GetBytes has a constant stub and
dereferences the output pointer. Supplying NULL crashes in user space.
The source-owned status probe must never call it.
Tests are static and run without connected hardware.
"""
from __future__ import print_function
import os
import re
import sys
import unittest

ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC=os.path.join(ROOT,"src","decklink-hdmi-sink-status.cpp")

class LegacyStatusSafety(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(SRC,"rb") as f:
            cls.source=f.read().decode("utf-8")

    def test_no_legacy_getbytes_invocation(self):
        self.assertFalse(bool(re.search(r'\(\s*\(\s*GetBytes\s*\)',self.source)))
        self.assertNotIn("typedef int32_t (*GetBytes)",self.source)

    def test_status_integer_only(self):
        self.assertIn("typedef int32_t (*ReadInteger)",self.source)
        self.assertIn('value(status,0x63766f6dU,"current_output_mode")',
                      self.source)

    def test_real_sierra_guard(self):
        self.assertIn('strcmp(model,"Macmini5,3")',self.source)
        self.assertIn('strncmp(kernel,"16.",3)',self.source)

    def test_no_capture_or_output_enable(self):
        self.assertNotIn("EnableVideoOutput",self.source)
        self.assertNotIn("StartStreams",self.source)
        self.assertNotIn("SetInt",self.source)

    def test_incident_explicitly_documented(self):
        self.assertIn("0x1f3e4",self.source)
        self.assertIn("0x80000003",self.source)

if __name__=="__main__":
    unittest.main()
