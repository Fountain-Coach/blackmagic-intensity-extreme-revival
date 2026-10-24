#!/usr/bin/env python2.7
# -*- coding: utf-8 -*-
"""Independently audit original 30-frame silver HDMI IN -> HDMI OUT replay.

This must run AFTER, never instead of, a successful true hardware recording.
Reads full private raw UYVY frames on the same silver Mac and compares exact
manifest hashes to the 60 Blackmagic HDMI OUT API-submitted buffer identities.
A successful digital comparison never claims optical monitor pixels measured.
"""
from __future__ import print_function
import hashlib
import json
import os
import re
import stat
import sys
import tempfile

BASE = "/Volumes/Fresh/FountainVideoDockerLab/private-roundtrip"
FRAME_BYTES = 1920*1080*2


def reject(reason):
    print("stage=roundtrip_independent_verifier result=REJECTED reason=%s" % reason)
    return 1


def safe_file(path, kind, uid, permissions=0o077):
    try:
        s=os.lstat(path)
    except OSError:
        raise ValueError(kind+"_missing")
    expected=stat.S_ISDIR if kind=="recording_dir" else stat.S_ISREG
    if not expected(s.st_mode) or s.st_uid!=uid or s.st_mode & permissions or \
       (kind!="recording_dir" and s.st_nlink!=1):
        raise ValueError(kind+"_not_private")
    return s


def verify(directory, output_log):
    uid=os.getuid()
    full=os.path.realpath(directory)
    if not full.startswith(BASE+"/recording-") or \
       os.path.dirname(full)!=BASE or not os.path.basename(full).startswith("recording-"):
        raise ValueError("outside_source_private_recording_root")
    safe_file(BASE,"recording_dir",uid)
    safe_file(full,"recording_dir",uid)
    manifest=os.path.join(full,"manifest.tsv")
    safe_file(manifest,"manifest",uid)
    safe_file(output_log,"output_log",uid)
    with open(manifest,"rb") as f:
        rows=[line.decode("ascii") if not isinstance(line,str) else line
              for line in f.read().splitlines()]
    if len(rows)!=31 or rows[0]!="index\twidth\theight\trow_bytes\thost_monotonic_ns\tbytes\tsha256":
        raise ValueError("incorrect_manifest_length_or_schema")
    hashes=[]
    previous_ns=0
    for i,line in enumerate(rows[1:]):
        pieces=line.split("\t")
        if len(pieces)!=7:
            raise ValueError("bad_frame_metadata")
        idx,w,h,stride,ns,count,sha=pieces
        if (int(idx),int(w),int(h),int(stride),int(count)) != \
           (i,1920,1080,3840,FRAME_BYTES):
            raise ValueError("wrong_frame_geometry_length_or_order")
        if not re.match(r"^[0-9a-f]{64}$",sha):
            raise ValueError("bad_frame_digest")
        timestamp=int(ns)
        if timestamp<=previous_ns:
            raise ValueError("nonmonotonic_host_frame_timestamp")
        previous_ns=timestamp
        path=os.path.join(full,"frame-%02d.uyvy"%i)
        fileinfo=safe_file(path,"raw_frame",uid)
        if fileinfo.st_size!=FRAME_BYTES:
            raise ValueError("short_or_long_raw_frame")
        digest=hashlib.sha256()
        with open(path,"rb") as f:
            while True:
                b=f.read(1024*1024)
                if not b:break
                digest.update(b)
        if digest.hexdigest()!=sha:
            raise ValueError("raw_frame_hash_mismatch")
        hashes.append(sha)
    with open(output_log,"rb") as f:
        text=f.read(20000)
        if not isinstance(text,str):
            text=text.decode("ascii")
    if "stage=replay result=api_accepted" not in text or \
       "recorded_frames=30 submitted=60" not in text or \
       "byte_identical_output_buffers=60" not in text or \
       "visual_monitor_witness=NOT_MEASURED" not in text or \
       "stage=roundtrip result=DIGITAL_ACCEPTED" not in text:
        raise ValueError("missing_actual_60_frame_vendor_replay_ack")
    report={
        "contract":"original-silver-HDMI-30-frame-raw-recording-v1",
        "input_source":"silver-Macmini5,3-HDMI-OUT-to-original-Intensity-HDMI-IN",
        "hardware_driver":"original-Sierra-DesktopVideo-10.4",
        "capture_format":"1920x1080@30-UYVY-8bit",
        "captured_frames":30,
        "capture_raw_bytes_verified":30*FRAME_BYTES,
        "capture_frame_checksums":hashes,
        "distinct_frame_payloads":len(set(hashes)),
        "output_submissions_api_accepted":60,
        "output_frame_buffers_byte_identical_to_recording":60,
        "monitor_hdmi_wire_pixels_recorded":False,
        "monitor_optical_pixels_recorded":False,
        "physical_monitor_E2E_accepted":False,
        "result":"DIGITAL_RECORDING_AND_OUTPUT_API_ACCEPTED_ONLY",
    }
    dest=os.path.join(full,"independent-digital-verification.json")
    fd,tmp=tempfile.mkstemp(prefix=".verify-",suffix=".tmp",dir=full)
    try:
        os.fchmod(fd,0o600)
        payload=json.dumps(report,sort_keys=True,indent=2).encode("ascii")
        os.write(fd,payload)
        os.fsync(fd)
        os.close(fd)
        os.rename(tmp,dest)
    except Exception:
        try:os.close(fd)
        except Exception:pass
        try:os.unlink(tmp)
        except Exception:pass
        raise
    print("stage=roundtrip_independent_verifier result=DIGITAL_ACCEPTED "
          "captured_frames=30 raw_verified_bytes=%d "
          "distinct_raw_frames=%d output_submitted=60 "
          "optical_monitor_witness=NOT_MEASURED "
          "private_report=%s" %
          (30*FRAME_BYTES,len(set(hashes)),dest))
    return 0


if __name__=="__main__":
    if len(sys.argv)!=3:
        print("usage: verify_hdmi_roundtrip_recording.py PRIVATE_RECORDING_DIR PRIVATE_OUTPUT_LOG",
              file=sys.stderr)
        sys.exit(64)
    try:
        sys.exit(verify(sys.argv[1],sys.argv[2]))
    except (ValueError,IOError,OSError,OverflowError) as err:
        sys.exit(reject(type(err).__name__+"_"+str(err)))
