#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Synthetic-only safety tests for Sierra display recovery decisions.

No actual CoreGraphics API calls, no HDMI connector changes, no video streams.
Compatible with Sierra Python 2.7 and modern Python 3.
"""
from __future__ import print_function

import json
import os
import shutil
import sys
import tempfile
import time
import unittest

GUARD_PATH = os.path.join(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))), "tools", "sierra",
    "hdmi-mode-recovery-guard.py")

namespace = {"__name__": "hdmi_recovery_guard_tests"}
if sys.version_info[0] < 3:
    execfile(GUARD_PATH, namespace)
else:
    with open(GUARD_PATH, "rb") as f:
        exec(compile(f.read(), GUARD_PATH, "exec"), namespace)


class FakeQuartz(object):
    def __init__(self, rate=30.0, vendor=8961, product=0, count=1,
                 allow_25=True, allow_30=True, set_status=0, renew_id=False):
        self.display_id = 41
        self.current = (1920, 1080, rate)
        self.vendor = vendor
        self.product = product
        self.count = count
        self.allow_25 = allow_25
        self.allow_30 = allow_30
        self.set_status = set_status
        self.renew_id = renew_id
        self.set_calls = []
        self.ids_seen = []

    def CGGetOnlineDisplayList(self, n, unused1, unused2):
        return (0, [self.display_id] * self.count, self.count)

    def CGDisplayVendorNumber(self, did):
        return self.vendor

    def CGDisplayModelNumber(self, did):
        return self.product

    def CGDisplayCopyDisplayMode(self, did):
        self.ids_seen.append(did)
        return self.current

    def CGDisplayCopyAllDisplayModes(self, did, unused):
        modes = []
        if self.allow_30:
            modes.append((1920, 1080, 30.0))
        if self.allow_25:
            modes.append((1920, 1080, 25.0))
        return modes

    def CGDisplayModeGetWidth(self, mode):
        return mode[0]

    def CGDisplayModeGetHeight(self, mode):
        return mode[1]

    def CGDisplayModeGetRefreshRate(self, mode):
        return mode[2]

    def CGDisplaySetDisplayMode(self, did, mode, unused):
        self.set_calls.append((did, mode))
        if self.set_status == 0:
            self.current = mode
            if self.renew_id:
                self.display_id += 1
        return self.set_status


class RecoveryGuardTests(unittest.TestCase):
    def test_read_only_original_mode(self):
        q = FakeQuartz()
        s = namespace["inspect_quartz"](q)
        self.assertEqual(s["current"], (1920, 1080, 30.0))
        self.assertEqual(namespace["decide_return"](s), "ALREADY_ORIGINAL")
        self.assertEqual(q.set_calls, [])

    def test_recovery_needed_for_25(self):
        q = FakeQuartz(rate=25.0)
        s = namespace["inspect_quartz"](q)
        self.assertEqual(namespace["decide_return"](s),
                         "RETURN_TO_ORIGINAL_REQUIRED")
        self.assertEqual(q.set_calls, [])

    def test_unexpected_sink_fails_closed(self):
        q = FakeQuartz(vendor=19501)
        s = namespace["inspect_quartz"](q)
        self.assertFalse(s["ok"])
        self.assertEqual(namespace["decide_return"](s),
                         "BLOCKED_not_direct_BMD_EDID")
        self.assertEqual(q.set_calls, [])

    def test_extra_display_fails_closed(self):
        q = FakeQuartz(count=2)
        s = namespace["inspect_quartz"](q)
        self.assertFalse(s["ok"])
        self.assertEqual(namespace["decide_return"](s),
                         "BLOCKED_unexpected_display_count")
        self.assertEqual(q.set_calls, [])

    def test_missing_original_mode_fails_closed(self):
        q = FakeQuartz(rate=25.0, allow_30=False)
        s = namespace["inspect_quartz"](q)
        self.assertEqual(namespace["decide_return"](s),
                         "BLOCKED_NO_ORIGINAL_MODE")
        self.assertEqual(q.set_calls, [])

    def test_mock_restore_renews_display_id(self):
        q = FakeQuartz(rate=25.0, renew_id=True)
        status = namespace["restore_mode_30"](q, sleep=lambda _: None)
        self.assertEqual(status, "VERIFIED_ORIGINAL")
        self.assertEqual(q.set_calls, [(41, (1920, 1080, 30.0))])
        self.assertIn(42, q.ids_seen)

    def test_mock_restore_with_failed_system_set(self):
        q = FakeQuartz(rate=25.0, set_status=1001)
        status = namespace["restore_mode_30"](q, sleep=lambda _: None, attempts=2)
        self.assertEqual(status, "UNVERIFIED_ORIGINAL")
        self.assertEqual(len(q.set_calls), 2)

    def test_mock_restore_no_action_if_already_original(self):
        q = FakeQuartz(rate=30.0)
        status = namespace["restore_mode_30"](q, sleep=lambda _: None)
        self.assertEqual(status, "VERIFIED_ORIGINAL")
        self.assertEqual(q.set_calls, [])

    def test_mock_restore_never_changes_unsupported_sink(self):
        q = FakeQuartz(rate=25.0, vendor=19501)
        status = namespace["restore_mode_30"](q, sleep=lambda _: None,
                                               attempts=2)
        self.assertEqual(status, "UNVERIFIED_ORIGINAL")
        self.assertEqual(q.set_calls, [])

    def test_watcher_dead_controller_detected_without_mode_write(self):
        root = tempfile.mkdtemp(prefix="fc-guard-test-")
        old_snapshot = namespace["live_snapshot"]
        old_pid_alive = namespace["pid_alive"]
        try:
            namespace["live_snapshot"] = lambda: namespace["inspect_quartz"](
                FakeQuartz(rate=30.0))
            namespace["pid_alive"] = lambda pid: False
            now = time.time()
            lease = os.path.join(root, "lease.json")
            audit = os.path.join(root, "audit.json")
            with open(lease, "w") as f:
                json.dump({"contract": "FC_SIERRA_HDMI_GUARD_SIMULATION_V1",
                           "fixture": "pretend_source_1080p25",
                           "created": now, "expires": now + 2.0,
                           "controller_pid": 543210}, f)
            self.assertEqual(namespace["watch_only"](lease, audit), 0)
            with open(audit, "r") as f:
                result = json.load(f)
            self.assertEqual(result["trigger"], "controller_died")
            self.assertEqual(result["actual_decision"], "ALREADY_ORIGINAL")
            self.assertEqual(result["synthetic_decision"],
                             "RETURN_TO_ORIGINAL_REQUIRED")
            self.assertEqual(result["display_set_calls"], 0)
            self.assertEqual(result["live_restoration_qualified"], False)
        finally:
            namespace["live_snapshot"] = old_snapshot
            namespace["pid_alive"] = old_pid_alive
            shutil.rmtree(root)

    def test_watcher_rejects_non_simulation_lease(self):
        root = tempfile.mkdtemp(prefix="fc-guard-test-")
        try:
            lease = os.path.join(root, "lease.json")
            audit = os.path.join(root, "audit.json")
            with open(lease, "w") as f:
                json.dump({"contract": "unsafe_live_mode_change",
                           "fixture": "pretend_source_1080p25",
                           "created": time.time(),
                           "expires": time.time()+2.0,
                           "controller_pid": 543210}, f)
            self.assertEqual(namespace["watch_only"](lease, audit), 32)
            with open(audit, "r") as f:
                result = json.load(f)
            self.assertEqual(result["display_set_calls"], 0)
            self.assertEqual(result["live_restoration_qualified"], False)
        finally:
            shutil.rmtree(root)


if __name__ == "__main__":
    unittest.main()
