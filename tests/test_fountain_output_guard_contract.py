#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Hardware write admission guards for the owned Sierra visual instrument.

Pure text audit, no Blackmagic framework, output stream or PCI access.
Actual device busy refusal was separately witnessed on original Sierra.
"""
from __future__ import print_function
import os
import re
import unittest

ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE=os.path.join(ROOT,"src","fountain-video-output.cpp")
class Safety(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(SOURCE,"rb") as f:cls.src=f.read().decode("utf-8")
    def test_original_sierra_host_guard(self):
        self.assertIn('strcmp(model,"Macmini5,3")',self.src)
        self.assertIn('strncmp(kernel,"16.",3)',self.src)
    def test_model_guard(self):
        self.assertIn('strcmp(name,"Intensity Extreme")',self.src)
    def test_busy_read_and_preemptive_refusal(self):
        self.assertIn("0x64627374u,&busy",self.src)
        self.assertIn("if(brc||busy!=0)",self.src)
        self.assertIn("DEVICE_BUSY_OR_UNKNOWN",self.src)
    def test_exact_hdmi_selected_config(self):
        self.assertIn("0x766f636eu,&connection",self.src)
        self.assertIn("if(crc || !(connection & 2))",self.src)
        self.assertIn("HDMI_OUTPUT_NOT_SELECTED",self.src)
    def test_guards_run_before_output_enable(self):
        main=self.src.index("int main(")
        body=self.src[main:]
        self.assertLess(body.index("if(!original_sierra())"),
                        body.index("if(!device_owned_and_idle(device))"))
        self.assertLess(body.index("if(!device_owned_and_idle(device))"),
                        body.index("rc=((Enable)(*(void***)out)[6])"))
    def test_bounded_duration_and_interrupt(self):
        self.assertIn("requested>120",self.src)
        self.assertIn("requested<5",self.src)
        self.assertIn("signal(SIGINT,stop_handler)",self.src)
        self.assertIn("signal(SIGTERM,stop_handler)",self.src)
        self.assertIn("stage=disable_output",self.src)
    def test_no_capture_or_driver_replacement(self):
        self.assertNotIn("StartStreams",self.src)
        self.assertNotIn("EnableVideoInput",self.src)
        self.assertNotIn("IOConnectCallMethod",self.src)
        self.assertNotIn("kextload",self.src)

if __name__=="__main__":
    unittest.main()
