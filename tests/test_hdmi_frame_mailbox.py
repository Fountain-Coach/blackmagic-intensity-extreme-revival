"""Mailbox correctness tests: no real capture device or virtual disk required."""
import os
import sys
import tempfile
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, ROOT)
from simulator import hdmi_frame_mailbox as m

JPEG = b"\xff\xd8" + b"Fountain-HDMI-Linux-Docker" * 10 + b"\xff\xd9"


class FrameMailboxTests(unittest.TestCase):
    def setUp(self):
        self.folder=tempfile.mkdtemp(prefix="fc-hdmi-mailbox-")
        self.path=os.path.join(self.folder,"mailbox.raw")
        m.initialize(self.path)
        self.writer=m.Writer(self.path)

    def tearDown(self):
        self.writer.close()
        os.unlink(self.path)
        os.rmdir(self.folder)

    def read(self, seq):
        slot=(seq-1)%m.SLOT_COUNT
        with open(self.path,"rb") as fh:
            fh.seek(slot*m.SLOT_BYTES)
            header=m.decode_header(fh.read(m.HEADER_BYTES))
            fh.seek(slot*m.SLOT_BYTES+m.HEADER_BYTES)
            payload=fh.read(header["size"])
        return header,payload

    def test_private_file_and_first_frame(self):
        self.assertEqual(os.stat(self.path).st_size,m.DISK_BYTES)
        self.assertEqual(os.stat(self.path).st_mode & 0o077,0)
        seq=self.writer.submit(133700000000,640,360,JPEG,"silver-hdmi")
        self.assertEqual(seq,1)
        h,p=self.read(1)
        self.assertEqual(h["stream"],b"silver-hdmi")
        self.assertEqual(h["pts_ns"],133700000000)
        self.assertTrue(m.verify_payload(h,p))

    def test_ring_wrap_and_writer_restart_monotonic(self):
        for i in range(m.SLOT_COUNT+3):
            seq=self.writer.submit(1000000+i*33000000,320,180,
                                   JPEG+bytes(bytearray([i])), "physical-test")
            self.assertEqual(seq,i+1)
        self.assertTrue(m.verify_payload(*self.read(m.SLOT_COUNT+3)))
        self.writer.close()
        self.writer=m.Writer(self.path)
        self.assertEqual(self.writer.seq,m.SLOT_COUNT+3)
        self.assertEqual(self.writer.submit(987654321,320,180,JPEG,"fixed-silver"),
                         m.SLOT_COUNT+4)

    def test_bad_frames_rejected(self):
        bad=[b"not-jpeg", JPEG[:-2], b"\xff\xd8"+(b"x"*(m.MAX_FRAME_BYTES))+b"\xff\xd9"]
        for frame in bad:
            with self.assertRaises(m.MailboxError):
                self.writer.submit(1,320,180,frame,"silver")
        with self.assertRaises(m.MailboxError):
            self.writer.submit(-1,640,360,JPEG,"silver")
        with self.assertRaises(m.MailboxError):
            self.writer.submit(1,640,360,JPEG,"../../unsafe")
        self.assertEqual(self.writer.seq,0)

    def test_torn_payload_is_detected(self):
        seq=self.writer.submit(888,640,360,JPEG,"silver-hdmi")
        h,p=self.read(seq)
        self.assertTrue(m.verify_payload(h,p))
        self.assertFalse(m.verify_payload(h, p[:3]+b"z"+p[4:]))
        self.assertIsNone(m.decode_header(b"x"*m.HEADER_BYTES))

    def test_refuse_overwrite_existing_mailbox(self):
        with self.assertRaises(OSError):
            m.initialize(self.path)


if __name__=="__main__":
    unittest.main()
