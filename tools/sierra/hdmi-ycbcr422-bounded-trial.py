#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Bounded Sierra HDMI selected YCbCr422 test with independent restore guard.

Source hardware: silver Macmini5,3 HDMI OUT -> original Intensity Extreme HDMI IN.
Original device HDMI OUT unconnected, original Sierra 10.4 driver unchanged.

A separate watchdog checks selected colour every 100ms after controller
termination/deadline, restores original YCbCr444 via IODisplay (without Commit),
and logs the attempt. An external host can call --restore-only independently.

No frame streaming, screenshots, colour-profile files, driver or firmware
installation, or display resolution/refresh changes.

This guard cannot recover a completely wedged operating-system kernel or
graphics card; physical mode changes are not a proven replacement for rescue.
"""
from __future__ import print_function
import json
import os
import signal
import subprocess
import sys
import tempfile
import time

HELPER="/tmp/fc-sierra-hdmi-color-control"
RECEIVER="/tmp/fc-decklink-input-lock-probe"
RESULT="/tmp/fc-bmd-ycbcr422-physical-witness.json"
ORIGINAL="0x00000100"
TARGET="0x00000010"
LEASE_CONTRACT="FC_BMD_YCBCR422_TRIAL_V1"
MAX_LEASE_S=12.0


def atomic_json(path,data):
    tmp=path+".tmp.%d"%os.getpid()
    with open(tmp,"wb") as f:
        f.write((json.dumps(data,sort_keys=True)+"\n").encode("utf-8"))
    os.rename(tmp,path)


def read_json(path):
    with open(path,"rb") as f:
        return json.load(f)


def bounded(cmd, seconds=5.0):
    p=subprocess.Popen(cmd, stdin=open(os.devnull,"rb"),
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    deadline=time.time()+seconds
    while p.poll() is None and time.time()<deadline:
        time.sleep(.06)
    if p.poll() is None:
        p.terminate()
        for _ in range(8):
            if p.poll() is not None: break
            time.sleep(.07)
        if p.poll() is None: p.kill()
    content=p.communicate()[0]
    if not isinstance(content,str):
        content=content.decode("utf-8","replace")
    return {"rc":p.returncode,"text":content[:4500],"timed_out":time.time()>=deadline}


def current_color(output):
    for line in output.splitlines():
        if line.startswith("stage=color_read path=CGDisplay"):
            return line.rsplit("value=",1)[-1].strip()
    for line in output.splitlines():
        if line.startswith("stage=color_read path=AppleDisplay"):
            return line.rsplit("value=",1)[-1].strip()
    return "UNKNOWN"


def verify_original():
    out=bounded([HELPER,"--inspect"],3)
    return (out["rc"]==0 and
            current_color(out["text"])==ORIGINAL and
            "bmd_quartz_online=1" in out["text"])


def do_restore():
    attempts=[]
    for i in range(4):
        r=bounded([HELPER,"--restore444"],3)
        attempts.append({"rc":r["rc"],"result":r["text"][-750:]})
        if verify_original():
            return {"verified":True,"attempts":attempts}
        time.sleep(.2)
    return {"verified":False,"attempts":attempts}


def preflight():
    if not os.path.isfile(HELPER) or not os.access(HELPER,os.X_OK):
        raise RuntimeError("missing_color_helper")
    if not os.path.isfile(RECEIVER) or not os.access(RECEIVER,os.X_OK):
        raise RuntimeError("missing_no_stream_receiver")
    r=bounded([HELPER,"--inspect"],3)
    if r["rc"] or current_color(r["text"])!=ORIGINAL or "ycbcr422=1" not in r["text"]:
        raise RuntimeError("wrong_original_color_or_422_unavailable")
    if "bmd_quartz_online=1" not in r["text"]:
        raise RuntimeError("direct_bmd_display_not_online")
    s=bounded([RECEIVER,"--preflight-1080p30"],3)
    if s["rc"]!=0 or "active_mode_match=1" not in s["text"]:
        raise RuntimeError("wrong_original_source_mode")
    return {"color":r["text"],"source":s["text"]}


def watchdog(lease,ready,audit):
    state={"stage":"hdmi_color_watchdog"}
    try:
        value=read_json(lease)
        if value.get("contract")!=LEASE_CONTRACT:
            raise RuntimeError("wrong_lease")
        if value["deadline"]-value["started"]>MAX_LEASE_S+0.1:
            raise RuntimeError("overlong_lease")
        if time.time()-value["started"]>5:
            raise RuntimeError("stale_lease")
        # Verify the restore path is independently executable now.
        if not verify_original():
            raise RuntimeError("original_color_not_independently_readable")
        atomic_json(ready,{"armed":True,"pid":os.getpid()})
        trigger="deadline"
        while time.time()<value["deadline"]:
            if os.path.exists(value["done_file"]):
                trigger="controller_done"
                break
            try:
                os.kill(value["controller_pid"],0)
            except OSError:
                trigger="controller_died"
                break
            time.sleep(.10)
        state["trigger"]=trigger
        state["restoration"]=do_restore()
        state["result"]="PASS" if state["restoration"]["verified"] else "FAILED"
        atomic_json(audit,state)
        return 0 if state["restoration"]["verified"] else 41
    except Exception as e:
        state["result"]="FAILED"
        state["reason"]=str(e)[:150]
        state["restoration"]=do_restore()
        atomic_json(audit,state)
        return 42


def run_trial():
    # Even direct module calls may not re-run the invalid experiment.
    raise RuntimeError("IOGraphics_RGB_rewrite_blocks_422_trials")
    baseline=preflight()
    root=tempfile.mkdtemp(prefix="fc-bmd-color-trial-")
    lease=os.path.join(root,"lease.json")
    ready=os.path.join(root,"ready.json")
    audit=os.path.join(root,"audit.json")
    done=os.path.join(root,"done")
    guard_log=open(os.path.join(root,"guard.log"),"wb")
    result={"stage":"physical_bmd_ycbcr422_lock_trial","original_color":ORIGINAL,
            "requested_color":TARGET, "original_mode":"1080p30",
            "source_color_mode_is_not_wire_measurement":True,
            "video_streams_started":False,"driver_modified":False,
            "display_mode_changed":False}
    guard=None
    try:
        now=time.time()
        atomic_json(lease,{"contract":LEASE_CONTRACT,
                           "started":now,"deadline":now+MAX_LEASE_S,
                           "controller_pid":os.getpid(),"done_file":done})
        guard=subprocess.Popen(
            [sys.executable,os.path.abspath(__file__),
             "--watchdog",lease,ready,audit],
            stdin=open(os.devnull,"rb"),stdout=guard_log,
            stderr=subprocess.STDOUT,close_fds=True,
            preexec_fn=os.setsid)
        for i in range(36):
            if os.path.exists(ready):break
            if guard.poll() is not None:break
            time.sleep(.10)
        if not os.path.exists(ready) or guard.poll() is not None:
            raise RuntimeError("independent_watchdog_not_ready")
        result["watchdog_armed"]=True
        print("stage=color_trial watchdog=armed original=YCbCr444 target=YCbCr422")
        sys.stdout.flush()
        # Confirm original has not drifted since guard armed.
        preflight()
        changed=bounded([HELPER,"--set422"],4)
        result["color_attempt"]=changed
        print("stage=color_trial change_rc=%s"%changed["rc"])
        print(changed["text"])
        sys.stdout.flush()
        readback=bounded([HELPER,"--inspect"],3)
        result["during_color"]=current_color(readback["text"])
        print("stage=color_trial actual_selected=%s"%result["during_color"])
        sys.stdout.flush()
        if result["during_color"]!=TARGET:
            raise RuntimeError("YCbCr422_not_confirmed_by_source_driver")
        result["receiver"]=bounded([RECEIVER,"--probe-active-1080p30"],5)
        print("stage=color_trial receiver_probe_rc=%s"%result["receiver"]["rc"])
        print(result["receiver"]["text"])
        sys.stdout.flush()
    except Exception as e:
        result["trial_error"]=str(e)[:180]
        print("stage=color_trial rejected_or_failed=%s"%result["trial_error"])
        sys.stdout.flush()
    finally:
        result["controller_restoration"]=do_restore()
        if result["controller_restoration"]["verified"]:
            with open(done,"wb") as f:f.write(b"restored\n")
        if guard is not None:
            for i in range(65):
                if guard.poll() is not None:break
                time.sleep(.08)
        if os.path.exists(audit):
            result["watchdog_record"]=read_json(audit)
        result["original_color_verified"]=verify_original()
        result["accepted_receiver_lock"]=bool(
            result.get("receiver",{}).get("rc")==0 and
            "LOCK_CANDIDATE" in result.get("receiver",{}).get("text",""))
        atomic_json(RESULT,result)
        print("stage=color_trial final_original_color_restored=%s lock_candidate=%s"
              " witness=%s"%(
                result["original_color_verified"],
                result["accepted_receiver_lock"],RESULT))
        sys.stdout.flush()
    return 0 if result["original_color_verified"] else 70


def main(argv):
    if argv==["--preflight"]:
        preflight()
        print("stage=color_trial preflight=PASS original=YCbCr444"
              " supports_422=1 source=1080p30 physical_change=0")
        return 0
    if argv==["--restore-only"]:
        v=do_restore()
        print("stage=color_trial external_restore=%s"%v["verified"])
        return 0 if v["verified"] else 32
    if argv==["--trial-422"]:
        print("stage=color_trial result=BLOCKED_IOGRAPHICS_RGB_REWRITE"
              " no_source_change=1 no_vendor_calls=1"
              " restore_only_remains_available=1")
        return 65
    if len(argv)==4 and argv[0]=="--watchdog":
        return watchdog(argv[1],argv[2],argv[3])
    print("usage: --preflight|--restore-only|--trial-422")
    return 64


if __name__=="__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except Exception as e:
        print("stage=color_trial unhandled=%s"%str(e))
        sys.exit(72)
