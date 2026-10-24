"""Offline xhyve COM2 -> authenticated local Docker scratch ingress.

No guest NIC or Sierra kernel/network entitlements. The host must send
authenticated HTTP to its *loopback-only* proxy; this process never treats
serial data as independently verified hardware. Origin is forced synthetic.
"""
import argparse
import base64
import binascii
import hashlib
import hmac
import json
import os
import re
import termios
import tty
from pathlib import Path
from urllib.request import Request, urlopen

from .serial_wire import (MAGIC, CHUNK, MAX_FRAME, SERIAL_TIMEOUT,
                          STREAM_RE, WireError, crc32hex, digest,
                          read_line, send_line, valid_id)

def receive_one(fd, pending, token, endpoint):
    line = read_line(fd, pending, timeout=3600)
    parts = line.split(b" ")
    if len(parts) != 8 or parts[0] != MAGIC or not valid_id(parts[1]):
        # Missing or malformed frame headers are discarded, never media.
        return "ignored"
    fid = parts[1]
    try:
        pts = int(parts[2])
        width = int(parts[3])
        height = int(parts[4])
        size = int(parts[5])
        expected_sha = parts[6]
        stream = parts[7]
        if not (0 <= pts < 2**63 and 1 <= width <= 4096 and
                1 <= height <= 2160 and 4 <= size <= MAX_FRAME and
                len(expected_sha) == 64 and
                re.fullmatch(rb"[a-f0-9]{64}", expected_sha) and
                STREAM_RE.match(stream)):
            raise ValueError("invalid header fields")
    except ValueError:
        send_line(fd, b"F " + fid + b" HEADER")
        return "rejected_header"
    send_line(fd, b"H " + fid)
    frame = bytearray()
    for index in range((size + CHUNK - 1) // CHUNK):
        retries = 0
        while retries < 4:
            try:
                part = read_line(fd, pending)
                fields = part.split(b" ")
                if (len(fields) != 4 or fields[0] != b"C" or
                    fields[1] != fid or fields[2] != str(index).encode("ascii")):
                    raise WireError("unexpected sequence")
                block = base64.b64decode(fields[3].split(b":")[0], validate=True)
                claimed_crc = fields[3].split(b":")[-1]
                if (len(fields[3].split(b":")) != 2 or
                    len(block) != min(CHUNK, size - len(frame)) or
                    not hmac.compare_digest(crc32hex(block), claimed_crc)):
                    raise WireError("chunk mismatch")
                frame.extend(block)
                send_line(fd, b"A " + fid + b" " + str(index).encode("ascii"))
                break
            except (WireError, ValueError, binascii.Error):
                retries += 1
                send_line(fd, b"R " + fid + b" " + str(index).encode("ascii"))
        if retries == 4:
            send_line(fd, b"F " + fid + b" CHUNK")
            return "rejected_chunk"
    try:
        if len(frame) != size or not hmac.compare_digest(digest(frame), expected_sha):
            raise WireError("frame checksum")
        final = read_line(fd, pending)
        if final != b"E " + fid:
            raise WireError("final delimiter")
        if not (frame[:2] == b"\xff\xd8" and frame[-2:] == b"\xff\xd9"):
            raise WireError("not JPEG")
    except WireError:
        send_line(fd, b"F " + fid + b" SHA")
        return "rejected_checksum"
    headers = {
        "Content-Type": "image/jpeg",
        "Authorization": "Bearer " + token,
        "X-Fountain-PTS-Ns": str(pts),
        "X-Fountain-Origin": "synthetic",
        "X-Fountain-Width": str(width),
        "X-Fountain-Height": str(height),
        "X-Fountain-Stream-ID": stream.decode("ascii"),
    }
    try:
        request = Request(endpoint, data=bytes(frame), headers=headers, method="POST")
        with urlopen(request, timeout=5) as response:
            body = json.loads(response.read())
            if response.status != 201 or not body.get("accepted"):
                raise WireError("ingress denied")
            seq = int(body["seq"])
    except Exception:
        send_line(fd, b"F " + fid + b" INGRESS")
        return "ingress_rejected"
    send_line(fd, b"D " + fid + b" " + str(seq).encode("ascii"))
    return "accepted"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", default="/dev/ttyS1")
    parser.add_argument("--token-file", default="/run/secrets/fountain_scratch_token")
    parser.add_argument("--endpoint",
                        default="http://127.0.0.1:8273/api/scratch/ingest")
    args = parser.parse_args()
    if args.serial != "/dev/ttyS1" or args.endpoint != \
       "http://127.0.0.1:8273/api/scratch/ingest":
        parser.error("only isolated guest ttyS1 and same-network-namespace loopback accepted")
    token = Path(args.token_file).read_text(encoding="ascii").strip()
    if len(token) < 32:
        parser.error("private receiver token required")
    fd = os.open(args.serial, os.O_RDWR | os.O_NOCTTY)
    try:
        tty.setraw(fd)
        termios.tcflush(fd, termios.TCIFLUSH)
        pending = bytearray()
        print("stage=serial_guest result=listening physical_hdmi_accepted=0 guest_nic=none",flush=True)
        while True:
            try:
                state = receive_one(fd, pending, token, args.endpoint)
                if state != "ignored":
                    print("stage=serial_guest frame_result=%s physical_hdmi_accepted=0" %
                          state,flush=True)
            except WireError:
                pending.clear()
                # A partial/aborted frame never becomes receiver history.
                print("stage=serial_guest result=bounded_transport_reset",flush=True)
    finally:
        os.close(fd)


if __name__ == "__main__":
    main()
