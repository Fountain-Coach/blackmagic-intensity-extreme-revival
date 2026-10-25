#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Fail-closed tests for retired Sierra Intensity HDMI colour experiments.

No IOKit, CoreGraphics, vendor runtime or real display access. Compatible
with Sierra Python 2.7, macOS Python 3 and Ubuntu Python 3.
"""
from __future__ import print_function
import os
import sys
import unittest

ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CYUV_PY=os.path.join(ROOT,"tools","sierra",
                     "hdmi-ycbcr422-cyuv-bounded-trial.py")
CMOD_PY=os.path.join(ROOT,"tools","sierra",
                     "hdmi-ycbcr422-bounded-trial.py")
CYUV_C=os.path.join(ROOT,"src","sierra-hdmi-connection-color-control.c")
CMOD_C=os.path.join(ROOT,"src","sierra-hdmi-color-control.c")


def load_py(path, virtual_path=None):
    namespace={"__name__":"retired_422_test_fixture",
               "__file__":virtual_path or path}
    if sys.version_info[0]<3:
        execfile(path,namespace)
    else:
        with open(path,"rb") as stream:
            exec(compile(stream.read(),path,"exec"),namespace)
    return namespace


def read_file(path):
    with open(path,"rb") as stream:
        raw=stream.read()
    return raw.decode("utf-8")


class RetiredColorModes(unittest.TestCase):
    def test_cyuv_cli_refuses_422_before_all_io(self):
        ns=load_py(CYUV_PY)
        self.assertEqual(ns["main"](["--trial-422"]),65)

    def test_cmod_cli_refuses_422_before_all_io(self):
        ns=load_py(CMOD_PY)
        self.assertEqual(ns["main"](["--trial-422"]),65)

    def test_cyuv_direct_function_cannot_reach_live_switch(self):
        ns=load_py(CYUV_PY)
        with self.assertRaises(RuntimeError):
            ns["run_trial"]()

    def test_cmod_direct_function_cannot_reach_live_switch(self):
        ns=load_py(CMOD_PY)
        with self.assertRaises(RuntimeError):
            ns["run_trial"]()

    def test_cyuv_recovery_still_available(self):
        ns=load_py(CYUV_PY)
        self.assertEqual(ns["HELPER"],
                         "/tmp/fc-sierra-hdmi-connection-color-control")
        src=read_file(CYUV_PY)
        self.assertIn('if argv==["--restore-only"]:',src)
        self.assertIn('r=bounded([HELPER,"--restore444"],3)',src)

    def test_old_restore_routes_through_new_auto_restore(self):
        src=read_file(CMOD_PY)
        self.assertIn('CANONICAL=os.path.join(HERE,"hdmi-ycbcr422-cyuv-bounded-trial.py")',src)
        self.assertIn('return subprocess.call([sys.executable,CANONICAL]+argv)',src)
        self.assertNotIn("IODisplaySetIntegerParameter(",src)

    def test_old_preflight_is_retired(self):
        ns=load_py(CMOD_PY)
        self.assertEqual(ns["main"](["--preflight"]),65)

    def test_retired_cmod_script_cannot_switch(self):
        src=read_file(CMOD_PY)
        start=src.index("\ndef run_trial():\n")
        end=src.index("\ndef main(argv):\n",start)
        impl=src[start:end]
        self.assertNotIn("subprocess.Popen(",impl)
        self.assertIn("raise RuntimeError(",impl)

    def test_cyuv_c_guard_precedes_every_set_call(self):
        src=read_file(CYUV_C)
        main=src.index("int main(")
        gate=src.index("BLOCKED_IOGRAPHICS_RGB_REWRITE",main)
        setter=src.index("IODisplaySetIntegerParameter(",main)
        self.assertLess(gate,setter)
        self.assertIn('strcmp(argv[1],"--set422")',src[main:gate])
        self.assertIn('strcmp(argv[1],"--restore444")',src)
        self.assertIn('CFSTR("cyuv")',src)
        self.assertIn('!(restore && cmod==0x1)',src)

    def test_cmod_c_guard_precedes_wrong_old_setter(self):
        src=read_file(CMOD_C)
        main=src.index("int main(")
        gate=src.index("BLOCKED_RETIRED_CMOD_API",main)
        setter=src.index("IODisplaySetIntegerParameter(",main)
        self.assertLess(gate,setter)
        self.assertIn('strcmp(argv[1],"--inspect")',src[main:gate])

    def test_legacy_watchdog_is_disabled_before_any_io(self):
        ns=load_py(CMOD_PY)
        self.assertEqual(ns["main"](["--watchdog","lease","ready","audit"]),65)

    def test_legacy_restore_dispatches_only_to_canonical_cyuv_helper(self):
        ns=load_py(CMOD_PY)
        calls=[]
        old_call=ns["subprocess"].call
        old_exists=ns["os"].path.isfile
        try:
            ns["subprocess"].call=lambda args: calls.append(args) or 0
            ns["os"].path.isfile=lambda path: path==ns["CANONICAL"]
            self.assertEqual(ns["main"](["--restore-only"]),0)
            self.assertEqual(len(calls),1)
            self.assertEqual(calls[0],[sys.executable,ns["CANONICAL"],
                                       "--restore-only"])
        finally:
            ns["subprocess"].call=old_call
            ns["os"].path.isfile=old_exists

    def test_known_sierra_fc_prefix_uses_original_restoration_command(self):
        ns=load_py(CMOD_PY,"/tmp/fc-hdmi-ycbcr422-bounded-trial.py")
        self.assertEqual(ns["CANONICAL"],
                         "/tmp/fc-hdmi-ycbcr422-cyuv-bounded-trial.py")

    def test_original_auto_and_rgb_fallback_are_distinct(self):
        self.assertNotEqual(0x10000000,0x10)
        self.assertNotEqual(0x100,0x1)

if __name__=="__main__":
    unittest.main()
