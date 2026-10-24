"""Offline two-way PTY integration tests: no Blackmagic, no NIC, no image leak."""
import base64
import binascii
import hashlib
import os
import pty
import threading
import tty
import unittest

from simulator.serial_frame_guest import receive_one
from simulator.serial_wire import (
    CHUNK, WireError, chunks, crc32hex, header, read_line, send_line)
from simulator.scratch_server import ScratchServer
from simulator.tests.test_scratch_server import JPEG
from tools.sierra.fountain_serial_proxy import SerialSender


class SerialMediaTests(unittest.TestCase):
    def setUp(self):
        self.server = ScratchServer(("127.0.0.1", 0),
                                    "serial-local-mock-guest-token-1234567890")
        self.guest_url = "http://127.0.0.1:%d/api/scratch/ingest" % \
                         self.server.server_port
        self.server_thread = threading.Thread(target=self.server.serve_forever,
                                               daemon=True)
        self.server_thread.start()
        self.master, self.slave = pty.openpty()
        tty.setraw(self.master)
        tty.setraw(self.slave)
        self.sender = SerialSender(self.slave)
        self.host_pending = bytearray()
        self.guest_pending = bytearray()
        self.results = []

    def tearDown(self):
        os.close(self.master)
        os.close(self.slave)
        self.server.shutdown()
        self.server.server_close()
        self.server_thread.join(timeout=3)

    def guest_once(self):
        def consume():
            self.results.append(receive_one(
                self.master, self.guest_pending, self.server.ingest_token,
                self.guest_url))
        t = threading.Thread(target=consume, daemon=True)
        t.start()
        return t

    def test_real_pty_end_to_end(self):
        guest = self.guest_once()
        seq = self.sender.submit(JPEG, 8900000, 4, 2, "pty-synthetic")
        guest.join(5)
        self.assertEqual(seq, 0)
        self.assertEqual(self.results, ["accepted"])
        snap = self.server.history.snapshot()
        self.assertEqual(snap["count"], 1)
        self.assertEqual(snap["latest"]["stream_id"], "pty-synthetic")
        self.assertEqual(snap["latest"]["origin"], "synthetic")
        self.assertFalse(snap["hardware_hdmi_capture_accepted"])
        self.assertEqual(self.server.history.get(seq).data, JPEG)

    def test_crc_nak_then_recovery(self):
        guest = self.guest_once()
        fid = b"8"
        send_line(self.slave, header(8, 55, 4, 2, JPEG, b"retry"))
        self.assertEqual(read_line(self.slave, self.host_pending), b"H 8")
        for i, block in enumerate(chunks(JPEG)):
            idx = str(i).encode("ascii")
            packet = b"C " + fid + b" " + idx + b" " + \
                     base64.b64encode(block) + b":" + crc32hex(block)
            if i == 0:
                send_line(self.slave, packet[:-8] + b"ffffffff")
                self.assertEqual(read_line(self.slave, self.host_pending), b"R 8 0")
            send_line(self.slave, packet)
            self.assertEqual(read_line(self.slave, self.host_pending),
                             b"A 8 " + idx)
        send_line(self.slave, b"E 8")
        self.assertEqual(read_line(self.slave, self.host_pending), b"D 8 0")
        guest.join(5)
        self.assertEqual(self.results, ["accepted"])

    def test_whole_frame_checksum_prevents_ingest(self):
        guest = self.guest_once()
        fid = b"9"
        send_line(self.slave, header(9, 55, 4, 2, JPEG, b"altered"))
        self.assertEqual(read_line(self.slave, self.host_pending), b"H 9")
        bad = bytearray(JPEG)
        bad[10] ^= 1
        for i, chunk in enumerate(chunks(bytes(bad))):
            idx = str(i).encode("ascii")
            send_line(self.slave, b"C 9 " + idx + b" " +
                      base64.b64encode(chunk) + b":" + crc32hex(chunk))
            self.assertEqual(read_line(self.slave, self.host_pending),
                             b"A 9 " + idx)
        send_line(self.slave, b"E 9")
        self.assertEqual(read_line(self.slave, self.host_pending), b"F 9 SHA")
        guest.join(5)
        self.assertEqual(self.results, ["rejected_checksum"])
        self.assertEqual(self.server.history.snapshot()["count"], 0)

    def test_oversized_frame_rejected_before_serial(self):
        with self.assertRaises(WireError):
            self.sender.submit(b"\xff\xd8" + b"x"*262143 + b"\xff\xd9",
                               0,4,2,"overlimit")

    def test_invalid_frame_headers_rejected_locally(self):
        for pts, width, height, stream in [
            (-1,4,2,"test"),(0,0,2,"test"),
            (0,4,0,"test"),(0,4,2,"bad/path")
        ]:
            with self.assertRaises(WireError):
                self.sender.submit(JPEG,pts,width,height,stream)


if __name__ == "__main__":
    unittest.main()
