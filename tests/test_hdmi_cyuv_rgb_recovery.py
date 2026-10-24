#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Pure isolated tests for cyuv auto-recovery, including the observed RGB
fallback. Never loads IOKit, Quartz, DeckLink, or changes a display."""
from __future__ import print_function
import os
import sys
import unittest

ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE=os.path.join(ROOT,"tools","sierra",
                    "hdmi-ycbcr422-cyuv-bounded-trial.py")
C_SOURCE=os.path.join(ROOT,"src","sierra-hdmi-connection-color-control.c")
ns={"__name__":"ycbcr422_cyuv_recovery_tests"}
if sys.version_info[0]<3:
    execfile(SOURCE,ns)
else:
    with open(SOURCE,"rb") as f:
        exec(compile(f.read(),SOURCE,"exec"),ns)

def fake_inspection(cmod,cyuv):
    return ("stage=source_services bmd_apple_display=1"
            " bmd_quartz_online=1 registry_api_read=1 quartz_api_read=1\n"
            "stage=color_selected cmod=%s cyuv=%s supports_422=1"
            " supports_444=1\nstage=color_control result=READ_ONLY\n")%(cmod,cyuv)

class CyuvRestoreSafetyTests(unittest.TestCase):
    def test_original_auto_state_identified(self):
        v=fake_inspection("0x00000100","0x10000000")
        self.assertEqual(ns["current_color"](v),"0x00000100")
        self.assertEqual(ns["current_control"](v),"0x10000000")

    def test_rgb_fallback_not_misidentified_as_422(self):
        v=fake_inspection("0x00000001","0x00000010")
        self.assertEqual(ns["current_color"](v),"0x00000001")
        self.assertNotEqual(ns["current_color"](v),ns["TARGET"])
        self.assertEqual(ns["current_control"](v),ns["TARGET"])

    def test_verify_original_refuses_control_not_auto(self):
        old=ns["bounded"]
        try:
            ns["bounded"]=lambda cmd,seconds=3:{"rc":0,
               "text":fake_inspection("0x00000100","0x00000010")}
            self.assertFalse(ns["verify_original"]())
        finally:
            ns["bounded"]=old

    def test_automatic_restoration_after_rgb_fallback(self):
        state={"cmod":"0x00000001","cyuv":"0x00000010","restores":0}
        old=ns["bounded"]
        def stub(command,seconds=3):
            arg=command[-1]
            if arg=="--restore444":
                state["restores"]+=1
                state["cmod"]="0x00000100"
                state["cyuv"]="0x10000000"
                return {"rc":0,"text":"stage=color_write result=APPLIED"}
            if arg=="--inspect":
                return {"rc":0,"text":fake_inspection(state["cmod"],state["cyuv"])}
            raise AssertionError("unexpected_command")
        try:
            ns["bounded"]=stub
            self.assertFalse(ns["verify_original"]())
            result=ns["do_restore"]()
            self.assertTrue(result["verified"])
            self.assertEqual(state["restores"],1)
            self.assertTrue(ns["verify_original"]())
        finally:
            ns["bounded"]=old

    def test_c_source_restoration_allows_observed_rgb(self):
        with open(C_SOURCE,"rb") as f:
            body=f.read().decode("utf-8")
        self.assertIn("!(restore && cmod==0x1)",body)
        self.assertIn('CFSTR("cyuv")',body)
        self.assertNotIn("IODisplayCommitParameters(",body)

if __name__=="__main__":
    unittest.main()
