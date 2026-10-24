#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""One bounded 720p60 HDMI-source trial on the original Sierra Mac mini.

Exact immutable physical rig:
  silver Macmini5,3 HDMI OUT -> Intensity Extreme HDMI IN
  Blackmagic HDMI OUT disconnected; original Thunderbolt connected

This controller switches *only* the Mac's existing BMD HDMI display to its
advertised 1280x720 @ 60 Hz mode, runs the source-owned no-stream status probe
and restores 1920x1080 @ 30 Hz. An independent watchdog is armed before the
switch and restores even when the controller crashes. An externally scheduled
SSH fallback can independently invoke --restore-original.

The watchdog and recovery path cannot guarantee the host survives a graphics
driver/kernel hang; do not equate guard dry-runs with physical recovery.
No frame input streams, callbacks, pixel persistence, PCI MMIO, driver/firmware,
OS install, or alternate hardware ports are used.

Run as the existing non-root 'bene' user. Source ownership: Fountain Coach.
Copyright (c) 2026 Fountain Coach contributors. MIT.
"""
from __future__ import print_function
import ctypes
import imp
import json
import os
import plistlib
import shutil
import subprocess
import sys
import tempfile
import time

ORIGINAL = (1920, 1080, 30.0)
TRIAL = (1280, 720, 60.0)
VENDOR = 8961
LOCK_PROBE = "/tmp/fc-decklink-input-lock-probe"
WATCHDOG_DEADLINE_S = 13.0
RESULTS = "/tmp/fc-bmd-720p60-witness.json"
GUARD = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     "fc-hdmi-mode-recovery-guard.py")


def load_guard():
    return imp.load_source("fc_hdmi_recovery_module", GUARD)


def dump_json(path, value):
    tmp = path + ".tmp.%d" % os.getpid()
    with open(tmp, "wb") as f:
        f.write((json.dumps(value, sort_keys=True) + "\n").encode("utf-8"))
    os.rename(tmp, path)


def safe_read(path):
    try:
        with open(path, "rb") as f:
            return json.load(f)
    except Exception:
        return {}


def quartz_guarded():
    g = load_guard()
    g.ensure_sierra_host()
    import Quartz
    s = g.inspect_quartz(Quartz)
    return g, Quartz, s


def original_preflight():
    g, q, s = quartz_guarded()
    if not s.get("ok") or not g.equal_modes(s["current"], ORIGINAL):
        raise RuntimeError("BMD_source_not_original_1080p30")
    if not s.get("has_p30"):
        raise RuntimeError("original_mode_not_advertised")
    available = [
        m for m in q.CGDisplayCopyAllDisplayModes(s["display"], None)
        if g.equal_modes(g.mode_tuple(q,m), TRIAL)
        and q.CGDisplayModeIsUsableForDesktopGUI(m)
    ]
    if len(available) == 0:
        raise RuntimeError("source_has_no_usable_720p60_mode")
    if not os.path.isfile(LOCK_PROBE) or not os.access(LOCK_PROBE, os.X_OK):
        raise RuntimeError("no_bounded_720p60_decklink_probe")
    return g, q, s


def source_clock():
    """Read OS registry, not an independently measured HDMI wire clock."""
    try:
        raw = subprocess.check_output(
            ["/usr/sbin/ioreg","-a","-r","-c","AppleIntelFramebuffer"])
        nodes = plistlib.readPlistFromString(raw)
        cg=ctypes.CDLL("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")
        cg.CGOpenGLDisplayMaskToDisplayID.argtypes=[ctypes.c_uint32]
        cg.CGOpenGLDisplayMaskToDisplayID.restype=ctypes.c_uint32
        _,_,s=quartz_guarded()
        if not s.get("ok"):
            return {"status":"display_unavailable"}
        for n in nodes:
            idx=n.get("IOFramebufferOpenGLIndex")
            clk=n.get("IOFBCurrentPixelClock")
            pixels=n.get("IOFBCurrentPixelCount")
            if idx is None or clk is None or pixels is None:
                continue
            if idx<0 or idx>=32 or pixels<=0:continue
            if cg.CGOpenGLDisplayMaskToDisplayID(1 << idx)==s["display"]:
                return {"status":"read","clock_hz":clk,"pixels_per_frame":pixels,
                        "source_estimate_hz":float(clk)/pixels,
                        "not_wire_measured":True}
        return {"status":"active_framebuffer_clock_unavailable"}
    except Exception as e:
        return {"status":"failed","reason":str(e)[:140]}


def restore_original(attempts=44):
    g,q,s=quartz_guarded()
    # Guard restoration implements fresh identifier lookup at EACH iteration.
    result = g.restore_mode_30(q,sleep=time.sleep,attempts=attempts)
    after=g.inspect_quartz(q)
    verified=after.get("ok") and g.equal_modes(after["current"],ORIGINAL)
    return {"status":result,"verified":bool(verified),
            "current":after.get("current"),"display_ok":after.get("ok")}


def watchdog(lease_path, ready_path, audit_path):
    """Detached independent Sierra process. Restores if parent fails/expires."""
    result={"stage":"source_720p60_watchdog","display_write_possible":True}
    try:
        lease=safe_read(lease_path)
        if lease.get("contract")!="FC_HDMI_720P60_BOUNDED_TRIAL_V1":
            raise RuntimeError("wrong_lease_contract")
        if lease.get("deadline")-lease.get("started",0)>WATCHDOG_DEADLINE_S+1:
            raise RuntimeError("lease_overlong")
        original_preflight()
        dump_json(ready_path,{"armed":True,"pid":os.getpid()})
        while time.time()<lease["deadline"]:
            if os.path.exists(lease["done_file"]):
                result["trigger"]="controller_done"
                break
            try:
                os.kill(lease["controller_pid"],0)
            except OSError:
                result["trigger"]="controller_died"
                break
            time.sleep(0.12)
        if "trigger" not in result:result["trigger"]="deadline"
        result["restoration"]=restore_original()
        result["result"]="PASS" if result["restoration"]["verified"] else "FAILED"
        dump_json(audit_path,result)
        return 0 if result["restoration"]["verified"] else 40
    except Exception as ex:
        result["result"]="FAILED"
        result["reason"]=str(ex)[:220]
        # Still attempt to return to original, unless host preflight failed.
        try:
            result["restoration"]=restore_original()
        except Exception as rec:
            result["restoration_error"]=str(rec)[:220]
        dump_json(audit_path,result)
        return 41


def run_bounded_probe():
    proc=subprocess.Popen([LOCK_PROBE,"--probe-active-720p60"],
                          stdin=open(os.devnull,"rb"),
                          stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT)
    deadline=time.time()+6.0
    while proc.poll() is None and time.time()<deadline:
        time.sleep(0.08)
    if proc.poll() is None:
        proc.terminate()
        time.sleep(0.2)
        if proc.poll() is None:proc.kill()
    out=proc.communicate()[0]
    if not isinstance(out,str):out=out.decode("utf-8","replace")
    # Nothing except signal-lock status is returned or persisted.
    return {"exit":proc.returncode,"output":out[:5000],
            "start_streams_called":False,"pixels_captured":False}


def trial():
    g,q,s=original_preflight()
    result={"stage":"real_720p60_input_lock_trial",
            "hardware":"silver_hdmi_out_to_intensity_hdmi_in",
            "mode_changed":False,"original":ORIGINAL,
            "receiver_streams_started":False,"reboot":False}
    workspace=tempfile.mkdtemp(prefix="fc-hdmi-720p60-")
    lease=os.path.join(workspace,"lease.json")
    ready=os.path.join(workspace,"ready.json")
    audit=os.path.join(workspace,"audit.json")
    done=os.path.join(workspace,"done")
    log_handle=open(os.path.join(workspace,"watchdog.log"),"wb")
    guard_proc=None
    try:
        now=time.time()
        dump_json(lease,{
            "contract":"FC_HDMI_720P60_BOUNDED_TRIAL_V1",
            "controller_pid":os.getpid(),"started":now,
            "deadline":now+WATCHDOG_DEADLINE_S,
            "done_file":done})
        guard_proc=subprocess.Popen(
            [sys.executable,"-u",os.path.abspath(__file__),
             "--watchdog",lease,ready,audit],
            stdin=open(os.devnull,"rb"),stdout=log_handle,
            stderr=subprocess.STDOUT,close_fds=True,
            preexec_fn=os.setsid)
        for i in range(40):
            if os.path.exists(ready):break
            if guard_proc.poll() is not None:break
            time.sleep(0.1)
        if not os.path.exists(ready) or guard_proc.poll() is not None:
            raise RuntimeError("independent_watchdog_not_armed")
        result["watchdog_armed"]=True
        print("stage=physical_720p60 watchdog=armed"
              " original=1080p30 candidate=720p60")
        sys.stdout.flush()
        # Recheck the source after the guard is armed.
        g,q,s=original_preflight()
        modes=q.CGDisplayCopyAllDisplayModes(s["display"],None)
        candidates=[m for m in modes if g.equal_modes(g.mode_tuple(q,m),TRIAL)
                    and q.CGDisplayModeIsUsableForDesktopGUI(m)]
        if not candidates:raise RuntimeError("720p60_not_usable_on_recheck")
        rc=q.CGDisplaySetDisplayMode(s["display"],candidates[0],None)
        result["source_mode_set_return"]=int(rc)
        result["mode_changed"]=True
        print("stage=physical_720p60 mode_change_rc=%s" % rc)
        sys.stdout.flush()
        if rc!=0:raise RuntimeError("mode_change_rejected")
        matched=False
        for i in range(18):
            time.sleep(0.15)
            live=g.inspect_quartz(q)
            if live.get("ok") and g.equal_modes(live["current"],TRIAL):
                matched=True
                break
        result["mode_confirmed"]=matched
        print("stage=physical_720p60 active_720p60_confirmed=%s"%matched)
        sys.stdout.flush()
        if not matched:raise RuntimeError("720p60_not_selected_or_identified")
        result["actual_source_clock"]=source_clock()
        print("stage=physical_720p60 clock=%s" % result["actual_source_clock"])
        sys.stdout.flush()
        result["receiver"]=run_bounded_probe()
        print("stage=physical_720p60 receiver_exit=%s" %
              result["receiver"]["exit"])
        print(result["receiver"]["output"])
        sys.stdout.flush()
    except Exception as ex:
        result["trial_error"]=str(ex)[:250]
        print("stage=physical_720p60 error=%s" %result["trial_error"])
        sys.stdout.flush()
    finally:
        try:
            result["controller_restoration"]=restore_original()
        except Exception as ex:
            result["controller_restore_error"]=str(ex)[:250]
        # Don't signal finished until controller's own restoration completed.
        if result.get("controller_restoration",{}).get("verified"):
            with open(done,"wb") as f:f.write(b"restored\n")
        if guard_proc is not None:
            for i in range(75):
                if guard_proc.poll() is not None:break
                time.sleep(0.10)
            # If the guard is still restoring, don't kill it. It is the fallback.
        result["watchdog_record"]=safe_read(audit)
        try:
            _,_,after=quartz_guarded()
            result["original_after_trial"]=bool(
                after.get("ok") and g.equal_modes(after["current"],ORIGINAL))
        except Exception:
            result["original_after_trial"]=False
        result["accepted"]=bool(
            result.get("original_after_trial")
            and result.get("receiver",{}).get("exit")==0
            and "LOCK_CANDIDATE" in result.get("receiver",{}).get("output",""))
        dump_json(RESULTS,result)
        print("stage=physical_720p60 final_original_restored=%s"
              " receiver_lock_candidate=%s results=%s" %
              (result.get("original_after_trial"),result["accepted"],RESULTS))
        sys.stdout.flush()
        if guard_proc is not None and guard_proc.poll() is not None:
            shutil.rmtree(workspace)
        else:
            print("stage=physical_720p60 watchdog_still_running=1"
                  " preserve_temp_until_watchdog_finishes")


def main(args):
    if args==["--preflight"]:
        g,q,s=original_preflight()
        print("stage=physical_720p60 preflight=PASS"
              " source_original=1080p30 target=720p60"
              " status_probe_available=1 display_writes=0")
        return 0
    if args==["--restore-original"]:
        result=restore_original()
        print("stage=physical_720p60 external_restore=%s" %result)
        return 0 if result["verified"] else 31
    if args==["--trial-720p60"]:
        trial()
        result=safe_read(RESULTS)
        return 0 if result.get("original_after_trial") else 70
    if len(args)==4 and args[0]=="--watchdog":
        return watchdog(args[1],args[2],args[3])
    print("usage: --preflight|--restore-original|--trial-720p60")
    return 64


if __name__=="__main__":
    sys.exit(main(sys.argv[1:]))
