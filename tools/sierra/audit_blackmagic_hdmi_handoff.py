#!/usr/bin/env python2.7
# -*- coding: utf-8 -*-
"""Independent, *digital-boundary* audit of Intensity Extreme output replay.

This audit DOES NOT copy proprietary video pixels into Git, assume a second
HDMI capture input, or pretend that DisplayVideoFrameSync proves a monitor
received pixels. Verifies recorded input JPEG hashes against logged output
submission references and the device's actual S_OK output returns.

Use: python2.7 tools/sierra/audit_blackmagic_hdmi_handoff.py
Writes only a private small metadata JSON file into the local lab, never
the screenshot content. Does not open DeckLink / switch source / use PCI.
"""
from __future__ import print_function
import hashlib
import json
import os
import re
import stat
import sys
import tempfile

CAPTURES = [
    ("/tmp/fc-intensity-extreme-physical-frame0.jpg",
     "ea1f51836311a496d91f493f82bb9e380f41efff76992ef8438cea6511c562d3",
     30578),
    ("/tmp/fc-intensity-extreme-physical-frame1.jpg",
     "ab1abfddb80eb4a7d1404272aefed88d55373fd344dbb1debdc63a19c0c32634",
     30532)
]
LAB = "/Volumes/Fresh/FountainVideoDockerLab"
OUTPUT_LOG = "/tmp/fc-hdmi-archived-playback-20261024.log"
CUE_LOG = "/tmp/fc-hdmi-monitor-cue-20261024.log"

def fail(reason):
    print("stage=independent_digital_audit result=FAIL reason=%s" % reason)
    sys.exit(1)

def private_file(path):
    st=os.lstat(path)
    if not stat.S_ISREG(st.st_mode) or (st.st_mode & 0o077)!=0 or \
       st.st_uid!=os.getuid() or st.st_nlink!=1:
        fail("not_private_regular_file")
    return st

def require_log(path):
    st=os.stat(path)
    if not stat.S_ISREG(st.st_mode) or st.st_size>100000:
        fail("bad_log_metadata")
    with open(path,"rb") as f:
        return f.read()

def main():
    source=[]
    for name,expected,n in CAPTURES:
        st=private_file(name)
        if st.st_size!=n:fail("capture_length_mismatch")
        with open(name,"rb") as f:
            data=f.read()
        if not data.startswith("\xff\xd8") or not data.endswith("\xff\xd9"):
            fail("not_a_complete_jpeg")
        sha=hashlib.sha256(data).hexdigest()
        if sha!=expected:fail("capture_checksum_mismatch")
        source.append(dict(bytes=n,sha256=sha,recorded_physical_source=True))
    data=require_log(OUTPUT_LOG)
    lines=data.splitlines()
    frames=[x for x in lines if x.startswith("stage=archived_frame_playback ")]
    if len(frames)!=4:fail("expected_four_logged_hardware_output_submissions")
    records=[]
    for line in frames:
        def get(key):
            result=re.search(r"(?<!\S)%s=([^ ]+)"%key,line)
            return result.group(1) if result else None
        index=int(get("recording_index") or -1)
        repeat=int(get("repeat") or -1)
        if index not in (0,1) or repeat not in (0,1):
            fail("unexpected_frame_identity")
        if get("source_jpeg_sha256")!=CAPTURES[index][1]:
            fail("output_frame_origin_mismatch")
        if get("output_rc")!="0x00000000":
            fail("output_driver_rejected")
        if get("full_output_buffer_matches_decoded_recording")!="1":
            fail("output_buffer_difference")
        records.append(dict(source_frame=index,playback_cycle=repeat,
                            original_jpeg_sha256=CAPTURES[index][1],
                            output_api_rc=0, full_decoded_uyvy_buffer_equal=True))
    if [(x["source_frame"],x["playback_cycle"]) for x in records] != \
       [(0,0),(1,0),(0,1),(1,1)]:
        fail("incorrect_playback_order")
    if not any(x.startswith("stage=archived_playback result=API_ACCEPTED ") and
               "output_submissions=4" in x and
               "byte_identical_converted_buffers=4" in x and
               "optical_monitor_witness=NOT_MEASURED" in x and
               "new_recording_accepted=0" in x for x in lines):
        fail("missing_qualified_hardware_conclusion")
    cue=require_log(CUE_LOG)
    if not "stage=monitor_cue result=API_ACCEPTED" in cue:
        fail("hardware_cue_missing")
    doc=dict(version="fountain-hdmi-output-digital-boundary-v1",
             recorded_physical_capture_jpegs=source,
             output_submission_count=4,
             all_decompressed_source_to_output_buffers_match=True,
             hardware_decklink_output_api_calls_succeeded=True,
             cue_api_succeeded=True,
             original_input_capture_timestamp="earlier_2026_10_24",
             new_input_capture_accepted=False,
             output_hdmi_wire_frame_recorded=False,
             external_monitor_display_pixels_recorded=False,
             independent_2nd_hdmi_receiver_available=False,
             accepted_result="DIGITAL_API_HANDOFF_ONLY",
             next_gate="Independent physical HDMI sink receiver or optical monitor sensor required")
    folder=os.path.join(LAB,"private-roundtrip")
    if not os.path.isdir(folder):
        os.mkdir(folder,0o700)
    if os.stat(folder).st_mode & 0o077:
        fail("private_manifest_directory_not_private")
    path=os.path.join(folder,"digital-handoff-audit.json")
    fd,tmp=tempfile.mkstemp(prefix=".audit-",suffix=".tmp",dir=folder)
    try:
        os.fchmod(fd,0o600)
        os.write(fd,json.dumps(doc,sort_keys=True,indent=2).encode("ascii"))
        os.fsync(fd)
        os.close(fd)
        os.rename(tmp,path)
    except Exception:
        try:os.close(fd)
        except Exception:pass
        try:os.unlink(tmp)
        except Exception:pass
        raise
    print("stage=independent_digital_audit result=PASS "
          "source_real_captures=2 hardware_output_accepts=4 "
          "source_hash_refs_matched=4 handoff_buffers_byte_identical=4 "
          "monitor_pixels_recorded=0 full_HDMI_wire_E2E=NOT_ACCEPTED "
          "report=%s"%path)
if __name__=="__main__":
    main()
