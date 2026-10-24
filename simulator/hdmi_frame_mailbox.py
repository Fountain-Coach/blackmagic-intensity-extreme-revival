# -*- coding: utf-8 -*-
"""Fountain HDMI Frame Mailbox v1 — Sierra writer / Linux virtio-block reader.

The silver Mac still owns actual Thunderbolt / Blackmagic PCI hardware.
The Linux guest sees ONLY a dedicated, disposable, 4 MiB file-backed virtual
block device; NOT the Mac's boot disk, not a mounted ext4 filesystem, and not
the original PCI capture card.

Each of eight 512 KiB slots carries a complete, private JPEG and a 4 KiB
commit header. Sierra writes payload, fsyncs, then commits the header. The
Linux reader MUST use direct I/O and verify digest and reread the header to
reject stale, torn and superseded records. Never mount this disk as a filesystem.

Python 2.7/3 compatible for the host writer. Reader is Linux Python 3 only.
"""
from __future__ import print_function

import hashlib
import os
import re
import stat
import struct
import threading

MAGIC = b"FCHDMI01"
SLOT_COUNT = 8
SLOT_BYTES = 512 * 1024
HEADER_BYTES = 4096
DISK_BYTES = SLOT_COUNT * SLOT_BYTES
MAX_FRAME_BYTES = 262144
HEADER = struct.Struct("<8sQQIII32s64s")
STREAM_PATTERN = re.compile(br"^[A-Za-z0-9-]{1,64}$")


class MailboxError(ValueError):
    pass


def _wire_stream(stream):
    if not isinstance(stream, bytes):
        stream = stream.encode("ascii")
    if not STREAM_PATTERN.match(stream):
        raise MailboxError("Invalid stream id")
    return stream


def validated(seq, pts_ns, width, height, jpeg, stream):
    stream = _wire_stream(stream)
    if type(seq) not in (int, long if bytes is str else int) or not 1 <= seq < 2**63:
        raise MailboxError("Invalid sequence")
    if not (isinstance(pts_ns, (int, long if bytes is str else int)) and
            0 <= pts_ns < 2**63):
        raise MailboxError("Invalid timestamp")
    if not (1 <= width <= 4096 and 1 <= height <= 2160):
        raise MailboxError("Invalid dimensions")
    if not (isinstance(jpeg, bytes) and
            4 <= len(jpeg) <= MAX_FRAME_BYTES and
            jpeg.startswith(b"\xff\xd8") and jpeg.endswith(b"\xff\xd9")):
        raise MailboxError("Expected complete bounded JPEG")
    return stream


def encode_header(seq, pts_ns, width, height, jpeg, stream):
    stream = validated(seq, pts_ns, width, height, jpeg, stream)
    hdr = HEADER.pack(MAGIC, seq, pts_ns, width, height, len(jpeg),
                      hashlib.sha256(jpeg).digest(), stream.ljust(64, b"\x00"))
    return hdr.ljust(HEADER_BYTES, b"\x00")


def decode_header(block):
    if len(block) < HEADER_BYTES:
        return None
    magic, seq, pts, width, height, count, checksum, padded = HEADER.unpack_from(block)
    if magic != MAGIC or seq < 1 or seq >= 2**63 or pts >= 2**63 or \
       not 1 <= width <= 4096 or not 1 <= height <= 2160 or \
       not 4 <= count <= MAX_FRAME_BYTES:
        return None
    stream = padded.split(b"\x00", 1)[0]
    if not STREAM_PATTERN.match(stream):
        return None
    return {"seq":seq, "pts_ns":pts, "width":width, "height":height,
            "size":count, "sha256":checksum, "stream":stream}


def verify_payload(hdr, jpeg):
    return bool(hdr is not None and len(jpeg) == hdr["size"] and
                jpeg.startswith(b"\xff\xd8") and jpeg.endswith(b"\xff\xd9") and
                hashlib.sha256(jpeg).digest() == hdr["sha256"])


def write_all(fd, data, offset):
    os.lseek(fd, offset, os.SEEK_SET)
    remain = data
    while remain:
        sent = os.write(fd, remain)
        if sent <= 0:
            raise MailboxError("Short write")
        remain = remain[sent:]


class Writer(object):
    def __init__(self, path):
        info = os.stat(path)
        if not stat.S_ISREG(info.st_mode) or info.st_size != DISK_BYTES or \
           (info.st_mode & 0o077) or info.st_nlink != 1:
            raise MailboxError("Private dedicated 4 MiB regular file required")
        self.fd = os.open(path, os.O_RDWR)
        self.lock = threading.Lock()
        self.seq = 0
        for slot in range(SLOT_COUNT):
            raw = os.pread(self.fd, HEADER_BYTES, slot * SLOT_BYTES) \
                  if hasattr(os, "pread") else self._seek_read(slot * SLOT_BYTES)
            hdr = decode_header(raw)
            if hdr is not None:
                self.seq = max(self.seq, hdr["seq"])

    def _seek_read(self, offset):
        os.lseek(self.fd, offset, os.SEEK_SET)
        return os.read(self.fd, HEADER_BYTES)

    def submit(self, pts_ns, width, height, jpeg, stream):
        with self.lock:
            seq = self.seq + 1
            header = encode_header(seq, pts_ns, width, height, jpeg, stream)
            offset = ((seq - 1) % SLOT_COUNT) * SLOT_BYTES
            # Release only complete frames; header is the commit marker.
            write_all(self.fd, jpeg, offset + HEADER_BYTES)
            os.fsync(self.fd)
            write_all(self.fd, header, offset)
            os.fsync(self.fd)
            self.seq = seq
            return seq

    def close(self):
        os.close(self.fd)


def initialize(path):
    """Create only a NEW, disposable mailbox on Sierra; never overwrite one."""
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        os.ftruncate(fd, DISK_BYTES)
        os.fsync(fd)
    finally:
        os.close(fd)
    return path
