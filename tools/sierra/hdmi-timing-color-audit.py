#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Read-only source-mode timing + colour audit for the original Sierra Mac mini.

Reconstructs exact original IOGraphics V2 detailed timing entries, maps the
active BMD HDMI source framebuffer, and reads IODisplay color-mode parameters.
Reports the operating-system registry clock versus requested timing; neither
is an independent electrical HDMI wire/AVI InfoFrame measurement.

No HDMI mode, color, DeckLink, firmware, kernel, or pixel writes.
"""
from __future__ import print_function
import ctypes
import plistlib
import struct
import subprocess
import sys

EXPECTED=(8961,0)
EXPECTED_HOST="Macmini5,3"
COLORS={
    0x1:"RGB",
    0x10:"YCbCr422",
    0x100:"YCbCr444",
    0x1000:"RGB_limited",
    0x10000000:"automatic"
}


def raw_bytes(value):
    if hasattr(value,"data"):
        return value.data
    if isinstance(value,(bytes,bytearray)):
        return value
    raise TypeError("timing_bytes_required")


def parse_timing(blob):
    data=raw_bytes(blob)
    if len(data)<96:
        raise ValueError("short_IOG_DetailedTimingV2")
    identifier=struct.unpack_from("<I",data,0)[0]
    signal=struct.unpack_from("<I",data,32)[0]
    clock,clock_min,clock_max=struct.unpack_from("<QQQ",data,40)
    ha,hb,hs,hw,va,vb,vs,vw=struct.unpack_from("<8I",data,64)
    total=(ha+hb)*(va+vb)
    if not total or ha>16384 or va>16384 or clock>3000000000:
        raise ValueError("implausible_timing")
    return {"id":identifier,"signal_flags":signal,"clock_hz":clock,
            "clock_min_hz":clock_min,"clock_max_hz":clock_max,
            "width":ha,"height":va,"h_total":ha+hb,"v_total":va+vb,
            "rate_hz":float(clock)/total}


def get_plist_for_class(name):
    data=subprocess.check_output(["/usr/sbin/ioreg","-r","-a","-c",name])
    return plistlib.loads(data) if hasattr(plistlib,"loads") else plistlib.readPlistFromString(data)


def get_source_display_id():
    core=ctypes.CDLL("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")
    U=ctypes.c_uint32
    core.CGGetOnlineDisplayList.argtypes=[U,ctypes.POINTER(U),ctypes.POINTER(U)]
    core.CGGetOnlineDisplayList.restype=ctypes.c_int
    core.CGDisplayVendorNumber.argtypes=[U]
    core.CGDisplayVendorNumber.restype=U
    core.CGDisplayModelNumber.argtypes=[U]
    core.CGDisplayModelNumber.restype=U
    core.CGOpenGLDisplayMaskToDisplayID.argtypes=[U]
    core.CGOpenGLDisplayMaskToDisplayID.restype=U
    displays=(U*16)(); count=U()
    rc=core.CGGetOnlineDisplayList(16,displays,ctypes.byref(count))
    if rc or count.value!=1:
        raise ValueError("unexpected_online_displays")
    identity=(core.CGDisplayVendorNumber(displays[0]),
              core.CGDisplayModelNumber(displays[0]))
    if identity!=EXPECTED:
        raise ValueError("not_original_BMD_HDMI")
    return core,displays[0]


def audit():
    if sys.platform!="darwin":
        raise RuntimeError("not_macOS")
    model=subprocess.check_output(["sysctl","-n","hw.model"]).strip()
    if model != EXPECTED_HOST.encode("ascii"):
        raise ValueError("wrong_silver_mac")
    core,display_id=get_source_display_id()
    bmd_nodes=[v for v in get_plist_for_class("AppleDisplay")
               if v.get("DisplayVendorID")==EXPECTED[0] and
               v.get("DisplayProductID")==EXPECTED[1]]
    if len(bmd_nodes)!=1:
        raise ValueError("ambiguous_BMD_sink")
    params=bmd_nodes[0].get("IODisplayParameters",{})
    color=params.get("cmod",{}).get("value")
    supported=params.get("colr",{}).get("value")
    preference=params.get("cyuv",{}).get("value")
    if color is None or supported is None:
        raise ValueError("missing_display_color_registry")
    print("stage=hdmi_colour selected=0x%x selected_name=%s supported_mask=0x%x"
          " supports_RGB=%d supports_422=%d supports_444=%d auto_value=0x%x"
          % (color,COLORS.get(color,"unknown"),supported,
             int(bool(supported&0x1)),int(bool(supported&0x10)),
             int(bool(supported&0x100)),preference if preference is not None else 0))

    framebuffer=None
    for fb in get_plist_for_class("AppleIntelFramebuffer"):
        idx=fb.get("IOFramebufferOpenGLIndex")
        if isinstance(idx,int) and idx>=0 and idx<32:
            if core.CGOpenGLDisplayMaskToDisplayID(1<<idx)==display_id:
                if framebuffer is not None:
                    raise ValueError("multiple_active_BMD_framebuffers")
                framebuffer=fb
    if framebuffer is None:
        raise ValueError("missing_active_BMD_framebuffer")
    clock=framebuffer.get("IOFBCurrentPixelClock")
    count=framebuffer.get("IOFBCurrentPixelCount")
    if not clock or not count:
        raise ValueError("no_live_registry_clock")
    print("stage=operating_clock frame_gl_index=%d reported_clock_hz=%d"
          " reported_pixel_count=%d registry_derived_refresh_hz=%.6f" %
          (framebuffer["IOFramebufferOpenGLIndex"],clock,count,
           float(clock)/count))
    entries=framebuffer.get("IOFBDetailedTimings",[])
    timings=[]
    for entry in entries:
        try:
            timings.append(parse_timing(entry))
        except (TypeError,ValueError,struct.error):
            continue
    for width,height,nominal in ((1920,1080,30),(1280,720,60)):
        cand=[t for t in timings if t["width"]==width and
              t["height"]==height and abs(t["rate_hz"]-nominal)<0.001
              and t["signal_flags"]==0]
        if not cand:
            raise ValueError("missing_standard_requested_timing_%dp%d"%
                             (height,nominal))
        # Consistent entries with the same exact clock are sufficient.
        clocks=sorted(set(t["clock_hz"] for t in cand))
        if len(clocks)!=1:
            raise ValueError("ambiguous_requested_clocks")
        t=cand[0]
        print("stage=detailed_timing format=%dx%d_p%d mode_id=0x%x"
              " requested_clock_hz=%d totals=%dx%d intended_refresh_hz=%.6f"
              " registry_vs_request_percent=%.6f"%
              (width,height,nominal,t["id"],t["clock_hz"],
               t["h_total"],t["v_total"],t["rate_hz"],
               100.0*(float(clock)/t["clock_hz"]-1.0)))
    print("stage=interpretation live_output_wire_clock=NOT_MEASURED"
          " active_AVI_InfoFrame=NOT_MEASURED"
          " source_color_selection_is_driver_report=1"
          " receiver_lock=NOT_TESTED")
    print("stage=safety display_writes=0 color_writes=0 vendor_calls=0"
          " frame_pixels=0 firmware_driver_changes=0")
    return 0


if __name__=="__main__":
    try:
        sys.exit(audit())
    except Exception as e:
        print("stage=hdmi_source_audit result=FAILED reason=%s"%str(e))
        sys.exit(21)
