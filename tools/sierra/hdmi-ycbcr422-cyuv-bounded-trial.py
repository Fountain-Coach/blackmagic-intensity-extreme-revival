#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Bounded Sierra HDMI cyuv connection-control YCbCr422 test with independent restore guard.

Source hardware: silver Macmini5,3 HDMI OUT -> original Intensity Extreme HDMI IN.
Original device HDMI OUT unconnected, original Sierra 10.4 driver unchanged.

A separate watchdog checks selected colour every 100ms after controller
termination/deadline, restores original cyuv=automatic via IODisplay (without Commit),
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

HELPER="/tmp/fc-sierra-hdmi-connection-color-control"
RECEIVER="/tmp/fc-decklink-input-lock-probe"
RESULT="/tmp/fc-bmd-ycbcr422-cyuv-witness.json"
ORIGINAL="0x00000100"
ORIGINAL_CONTROL="0x10000000"
TARGET="0x00000010"
LEASE_CONTRACT="FC_BMD_CYUV_YCBCR422_TRIAL_V2"
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
        if line.startswith("stage=color_selected"):
            return line.split("cmod=",1)[-1].split()[0]
    return "UNKNOWN"


def current_control(output):
    for line in output.splitlines():
        if line.startswith("stage=color_selected"):
            return line.split("cyuv=",1)[-1].split()[0]
    return "UNKNOWN"


def verify_original():
    out=bounded([HELPER,"--inspect"],3)
    return (out["rc"]==0 and
            current_color(out["text"])==ORIGINAL and
            current_control(out["text"])==ORIGINAL_CONTROL and
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
    if r["rc"] or current_color(r["text"])!=ORIGINAL or current_control(r["text"])!=ORIGINAL_CONTROL or "supports_422=1" not in r["text"]:
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
    """Retired: IOGraphics 515.x maps nonautomatic cyuv to RGB, not 422."""
    raise RuntimeError("IOGraphics_RGB_rewrite_blocks_422_trials")


def main(argv):
    if argv==["--preflight"]:
        preflight()
        print("stage=color_trial preflight=PASS original=YCbCr444"
              " cyuv=automatic supports_422=1 source=1080p30 physical_change=0")
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
