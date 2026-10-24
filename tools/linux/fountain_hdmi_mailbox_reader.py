#!/usr/bin/env python3
"""Read-only direct-I/O virtual HDMI frame mailbox -> private Docker Scratch.

Run inside a capability-dropped container, sharing only the existing Scratch
container's network namespace, with /dev/vdb mounted as *READ-ONLY* virtual
block device. No PCI passthrough, no direct Mac disks, no guest LAN required.

The device and Linux Scratch token remain separate from Sierra. JPEG integrity
is checked across the shared file, and the commit header is read again to
reject overwritten/torn frames. HTTP POST stays on container localhost.
"""
import argparse
import ctypes
import hashlib
import json
import os
import time
import urllib.request
import urllib.error

from hdmi_frame_mailbox import (
    SLOT_COUNT, SLOT_BYTES, HEADER_BYTES, MAX_FRAME_BYTES, decode_header,
    verify_payload,
)

ALIGN = 4096
libc = ctypes.CDLL(None, use_errno=True)
libc.posix_memalign.argtypes = [ctypes.POINTER(ctypes.c_void_p),
                                ctypes.c_size_t, ctypes.c_size_t]
libc.posix_memalign.restype = ctypes.c_int
libc.pread.argtypes = [ctypes.c_int, ctypes.c_void_p,
                       ctypes.c_size_t, ctypes.c_longlong]
libc.pread.restype = ctypes.c_ssize_t
libc.free.argtypes = [ctypes.c_void_p]


def direct_read(fd, off, size):
    if off % ALIGN or size % ALIGN or size < ALIGN:
        raise ValueError("Unaligned direct read")
    ptr = ctypes.c_void_p()
    err = libc.posix_memalign(ctypes.byref(ptr), ALIGN, size)
    if err != 0:
        raise OSError(err, "posix_memalign")
    try:
        done = libc.pread(fd, ptr, size, off)
        if done != size:
            raise OSError(ctypes.get_errno(), "Direct block read failed")
        return ctypes.string_at(ptr, size)
    finally:
        libc.free(ptr)


def slot_header(fd, slot):
    return decode_header(direct_read(fd, slot * SLOT_BYTES, HEADER_BYTES))


def slot_frame(fd, slot, header):
    count = header["size"]
    length = (count + ALIGN - 1) // ALIGN * ALIGN
    payload = direct_read(fd, slot * SLOT_BYTES + HEADER_BYTES, length)[:count]
    # Reject if writer wrapped the ring during our read.
    again = slot_header(fd, slot)
    if (again is None or again["seq"] != header["seq"] or
        again["sha256"] != header["sha256"] or
        not verify_payload(header, payload)):
        return None
    return payload


def push(frame, header, token, endpoint):
    meta = {"Content-Type":"image/jpeg",
            "Authorization":"Bearer " + token,
            "X-Fountain-Origin":"decklink-observed",
            "X-Fountain-PTS-Ns":str(header["pts_ns"]),
            "X-Fountain-Width":str(header["width"]),
            "X-Fountain-Height":str(header["height"]),
            "X-Fountain-Stream-ID":header["stream"].decode("ascii")}
    req = urllib.request.Request(endpoint, data=frame, headers=meta, method="POST")
    with urllib.request.urlopen(req, timeout=3) as response:
        body = json.load(response)
        if response.status != 201 or not body.get("accepted") or \
           body.get("origin") != "decklink-observed":
            raise RuntimeError("Scratch receiver rejected frame")
        return body["seq"]


def run(device, token_file, poll, max_seconds):
    if device != "/dev/fc-hdmi-mailbox":
        raise ValueError("Dedicated read-only virtual block device only")
    if os.stat(token_file).st_mode & 0o077:
        raise ValueError("Receiver token must be private")
    with open(token_file, "r", encoding="utf8") as fh:
        token = fh.read().strip()
    if len(token) < 32:
        raise ValueError("Missing receiver-local token")
    if not hasattr(os, "O_DIRECT"):
        raise RuntimeError("Linux direct I/O support is mandatory")
    fd = os.open(device, os.O_RDONLY | os.O_DIRECT)
    endpoint = "http://127.0.0.1:8273/api/scratch/ingest"
    deadline = time.monotonic() + max_seconds if max_seconds else None
    last_seq = 0
    delivered = 0
    missed = 0
    started = time.monotonic()
    try:
        # Do not replay stale, private desktop material on daemon restart.
        starting = [slot_header(fd, s) for s in range(SLOT_COUNT)]
        last_seq = max([0]+[h["seq"] for h in starting if h])
        print("stage=guest_hdmi_mailbox status=ready last_existing_seq=%d "
              "device=readonly_direct authenticated_receiver=localhost" %
              last_seq, flush=True)
        while deadline is None or time.monotonic() < deadline:
            candidates = []
            for slot in range(SLOT_COUNT):
                h=slot_header(fd,slot)
                if h and h["seq"]>last_seq:
                    candidates.append((h["seq"],slot,h))
            candidates.sort()
            for seq,slot,h in candidates:
                if seq <= last_seq: continue
                payload = slot_frame(fd,slot,h)
                if payload is None:
                    missed += 1
                    continue
                try:
                    ack = push(payload,h,token,endpoint)
                except (OSError, ValueError, urllib.error.URLError,
                        RuntimeError) as exc:
                    print("stage=guest_hdmi_mailbox error=downstream_rejected",
                          flush=True)
                    # Back pressure: retry next poll; do NOT lie about receipt.
                    break
                last_seq = seq
                delivered += 1
                if delivered <= 4 or delivered % 15 == 0:
                    print("stage=guest_hdmi_mailbox accepted=%d host_seq=%d "
                          "scratch_seq=%d jpeg_bytes=%d sha256=%s" %
                          (delivered,seq,ack,len(payload),
                           hashlib.sha256(payload).hexdigest()),flush=True)
            time.sleep(poll)
        print("stage=guest_hdmi_mailbox result=stopped delivered=%d "
              "latest_host_seq=%d stale_or_torn=%d elapsed=%.2f" %
              (delivered,last_seq,missed,time.monotonic()-started),flush=True)
        return delivered
    finally:
        os.close(fd)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--device",default="/dev/fc-hdmi-mailbox")
    p.add_argument("--token-file",default="/run/secrets/fountain_scratch_token")
    p.add_argument("--poll",type=float,default=0.08)
    p.add_argument("--max-seconds",type=int,default=0)
    args=p.parse_args()
    if not 0.02 <= args.poll <= 2 or args.max_seconds < 0:
        p.error("Invalid bounded polling arguments")
    run(args.device,args.token_file,args.poll,args.max_seconds)


if __name__ == "__main__":
    main()
