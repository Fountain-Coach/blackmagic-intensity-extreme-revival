"""Fountain serial media wire primitives; compatible with Sierra Python 2.7 and Python 3.

xhyve com2,autopty <-> Linux /dev/ttyS1. This is *not* PCI passthrough.
Stop-and-wait 256-byte chunks, checksum per chunk and whole JPEG hash.
No screen pixels, credentials, or token are written to diagnostics.
"""
from __future__ import print_function
import binascii
import hashlib
import os
import re
import select
import time

MAGIC = b"FCSV1"
CHUNK = 256
MAX_FRAME = 262144
MAX_LINE = 1600
SERIAL_TIMEOUT = 6.0
STREAM_RE = re.compile(br"^[A-Za-z0-9-]{1,64}$")


class WireError(Exception):
    pass


def crc32hex(data):
    return ("%08x" % (binascii.crc32(data) & 0xffffffff)).encode("ascii")


def digest(data):
    return hashlib.sha256(data).hexdigest().encode("ascii")


def send_line(fd, line, timeout=SERIAL_TIMEOUT, pace_seconds=0):
    if not isinstance(line, bytes):
        line = line.encode("ascii")
    if b"\n" in line or b"\r" in line or len(line) > MAX_LINE - 1:
        raise WireError("invalid wire line")
    data = line + b"\n"
    until = time.time() + timeout
    while data:
        wait = until - time.time()
        if wait <= 0 or not select.select([], [fd], [], wait)[1]:
            raise WireError("serial write timeout")
        # xhyve emulates an old 16550 UART with a 16-byte receive FIFO.
        # Writing a whole line to the host PTY can silently overrun it.
        # Pace host->guest bytes only, until a correctly acknowledged
        # hardware-host acceptance establishes a faster safe rate.
        window = data[:1] if pace_seconds else data
        n = os.write(fd, window)
        if n <= 0:
            raise WireError("serial write closed")
        data = data[n:]
        if pace_seconds:
            time.sleep(pace_seconds)


def read_line(fd, pending, timeout=SERIAL_TIMEOUT):
    until = time.time() + timeout
    while True:
        n = pending.find(b"\n")
        if n >= 0:
            line = bytes(pending[:n])
            del pending[:n + 1]
            if len(line) > MAX_LINE or b"\r" in line:
                raise WireError("invalid received line")
            return line
        if len(pending) > MAX_LINE:
            raise WireError("received line too large")
        wait = until - time.time()
        if wait <= 0 or not select.select([fd], [], [], wait)[0]:
            raise WireError("serial read timeout")
        received = os.read(fd, 512)
        if not received:
            raise WireError("serial disconnected")
        pending.extend(bytearray(received))


def header(fid, pts, width, height, data, stream):
    if not isinstance(stream, bytes):
        stream = stream.encode("ascii")
    if not 4 <= len(data) <= MAX_FRAME:
        raise WireError("frame exceeds bounded wire capacity")
    if not (0 <= int(pts) < 2 ** 63 and 1 <= int(width) <= 4096 and
            1 <= int(height) <= 2160):
        raise WireError("invalid timestamp or geometry")
    if not STREAM_RE.match(stream):
        raise WireError("invalid stream identifier")
    if not (data.startswith(b"\xff\xd8") and data.endswith(b"\xff\xd9")):
        raise WireError("not a complete JPEG")
    return b" ".join([MAGIC, str(fid).encode("ascii"),
                      str(int(pts)).encode("ascii"), str(int(width)).encode("ascii"),
                      str(int(height)).encode("ascii"),
                      str(len(data)).encode("ascii"), digest(data), stream])


def chunks(data):
    for offset in range(0, len(data), CHUNK):
        yield data[offset:offset + CHUNK]


def valid_id(value):
    return re.match(br"^[0-9]{1,12}$", value) is not None
