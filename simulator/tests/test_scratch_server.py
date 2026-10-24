"""Software-only browser scratch ring and authenticated ingress regressions."""
import base64
from http.server import ThreadingHTTPServer
from threading import Thread
import json
from urllib.request import Request, urlopen
from urllib.error import HTTPError
import unittest

from simulator.scratch import ScratchHistory, InvalidFrame
from simulator.scratch_server import ScratchServer

# Tiny complete JPEG fixture, distinct bytes can be supplied without imaging SDK.
JPEG = base64.b64decode(
    "/9j/4AAQSkZJRgABAQAASABIAAD/4QBMRXhpZgAATU0AKgAAAAgAAYdpAAQAAAABAAAAGgAAAAAAA6ABAAMAAAABAAEAAKACAAQAAAABAAAABKADAAQAAAABAAAAAgAAAAD/wAARCAACAAQDASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9sAQwACAgICAgIDAgIDBQMDAwUGBQUFBQYIBgYGBgYICggICAgICAoKCgoKCgoKDAwMDAwMDg4ODg4PDw8PDw8PDw8P/9sAQwECAgIEBAQHBAQHEAsJCxAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQ/90ABAAB/9oADAMBAAIRAxEAPwD8eJ1XEZ2j7gqDYn90VZn6Rf7gqCuxHIz/2Q==")

class ScratchRingTests(unittest.TestCase):
    def test_empty_state(self):
        ring=ScratchHistory(3, 1000,clock=lambda:50)
        s=ring.snapshot()
        self.assertEqual(s["count"],0)
        self.assertFalse(s["source_online"])
        self.assertFalse(s["hardware_hdmi_capture_accepted"])

    def test_monotonic_sequence_and_eviction(self):
        ring=ScratchHistory(3, 10000,clock=lambda:100)
        for i in range(6):
            ring.push(JPEG, i*100000000, "synthetic", 1, 1)
        s=ring.snapshot()
        self.assertEqual([x["seq"] for x in s["history"]],[3,4,5])
        self.assertEqual(s["evicted_frames"],3)
        self.assertIsNone(ring.get(2))
        self.assertEqual(ring.get(5).data,JPEG)

    def test_byte_budget_enforced(self):
        ring=ScratchHistory(10, len(JPEG)*2,clock=lambda:20)
        for i in range(3):
            ring.push(JPEG,i,"synthetic",1,1)
        self.assertEqual(ring.snapshot()["count"],2)
        self.assertEqual(ring.snapshot()["retained_bytes"],2*len(JPEG))

    def test_missing_or_untrusted_media_rejected(self):
        ring=ScratchHistory(clock=lambda:20)
        for frame,src,width,height,pts in [
            (b"anything","synthetic",1,1,0),
            (JPEG,"some-not-authorized-source",1,1,0),
            (JPEG,"synthetic",0,1,0),
            (JPEG,"synthetic",1,30000,0),
            (JPEG,"synthetic",1,1,-5),
        ]:
            with self.assertRaises(InvalidFrame):
                ring.push(frame,pts,src,width,height)

    def test_hardware_provenance_never_implies_hdmi_success(self):
        ring=ScratchHistory(clock=lambda:2)
        f=ring.push(JPEG,23,"decklink-observed",1280,720)
        self.assertFalse(f.metadata["simulated"])
        self.assertFalse(f.metadata["physical_hdmi_verified"])
        self.assertFalse(ring.snapshot()["hardware_hdmi_capture_accepted"])

    def test_app_restart_creates_new_media_epoch(self):
        ring=ScratchHistory(clock=lambda:200)
        ring.push(JPEG, 4_000_000_000, "synthetic", 4, 2)
        ring.push(JPEG, 4_200_000_000, "synthetic", 4, 2)
        restarted=ring.push(JPEG, 0, "synthetic", 4, 2)
        state=ring.snapshot()
        self.assertEqual(restarted.seq, 2)
        self.assertEqual(state["stream_epoch"],1)
        self.assertEqual(state["epoch_reason"],"timestamp_rollback")
        self.assertEqual(state["count"],1)
        self.assertEqual(state["oldest_seq"],2)
        self.assertEqual(state["latest_seq"],2)

    def test_synthetic_to_hardware_switch_resets_history(self):
        ring=ScratchHistory(clock=lambda:200)
        ring.push(JPEG,0,"synthetic",4,2)
        ring.push(JPEG,0,"decklink-observed",4,2)
        state=ring.snapshot()
        self.assertEqual(state["stream_epoch"],1)
        self.assertEqual(state["epoch_reason"],"source_changed")
        self.assertEqual(state["count"],1)
        self.assertEqual(state["latest"]["origin"],"decklink-observed")
        self.assertFalse(state["hardware_hdmi_capture_accepted"])

    def test_online_timeout_is_explicit(self):
        current=[0]
        ring=ScratchHistory(clock=lambda:current[0])
        ring.push(JPEG,0,"synthetic",1,1)
        self.assertTrue(ring.snapshot()["source_online"])
        current[0]=3
        self.assertFalse(ring.snapshot()["source_online"])


class ScratchHTTPTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server=ScratchServer(("127.0.0.1",0),"strictly-local-synthetic-test-token-12345")
        cls.base="http://127.0.0.1:%d"%cls.server.server_port
        cls.thread=Thread(target=cls.server.serve_forever,daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join(timeout=3)

    def setUp(self):
        self.server.history=ScratchHistory(3,10000)

    def do(self,path,body=None,token=None,meta=None,mimetype="image/jpeg"):
        headers={}
        if body is not None:
            headers.update({
                "Content-Type":mimetype,
                "Authorization":"Bearer "+(token or ""),
                "X-Fountain-PTS-Ns":"200000000",
                "X-Fountain-Origin":"synthetic",
                "X-Fountain-Width":"1",
                "X-Fountain-Height":"1"
            })
            headers.update(meta or {})
        req=Request(self.base+path,data=body,headers=headers)
        try:
            with urlopen(req,timeout=4) as res:
                return res.status,res.read(),res.headers
        except HTTPError as e:
            return e.code,e.read(),e.headers

    def post(self,**kw):
        return self.do("/api/scratch/ingest", body=JPEG, token=
                       "strictly-local-synthetic-test-token-12345", **kw)

    def test_authorization_required(self):
        code,body,_=self.do("/api/scratch/ingest",body=JPEG)
        self.assertEqual(code,401)
        self.assertEqual(self.server.history.snapshot()["count"],0)

    def test_good_jpeg_ingest_and_indexed_lookup(self):
        code,body,_=self.post()
        self.assertEqual(code,201)
        seq=json.loads(body)["seq"]
        self.assertEqual(seq,0)
        code,body,h=self.do("/api/scratch/frame/0.jpg")
        self.assertEqual(code,200)
        self.assertEqual(body,JPEG)
        self.assertEqual(h["Content-Type"],"image/jpeg")
        self.assertEqual(h["X-Fountain-Frame-Origin"],"synthetic")
        self.assertEqual(h["X-Fountain-HDMI-Verified"],"false")
        state=json.loads(self.do("/api/scratch/state")[1])
        self.assertTrue(state["source_online"])
        self.assertEqual(state["history"][0]["seq"],0)

    def test_evicted_frames_return_not_found(self):
        for _ in range(5):
            self.post()
        self.assertEqual(self.do("/api/scratch/frame/0.jpg")[0],404)
        self.assertEqual(self.do("/api/scratch/frame/4.jpg")[0],200)
        self.assertEqual(json.loads(self.do("/api/scratch/state")[1])["count"],3)

    def test_invalid_jpeg_rejected(self):
        code,body,_=self.do("/api/scratch/ingest",body=b"WRONG",
          token="strictly-local-synthetic-test-token-12345")
        self.assertEqual(code,422)

    def test_missing_timing_rejected(self):
        code,body,_=self.post(meta={"X-Fountain-PTS-Ns":"-1"})
        self.assertEqual(code,422)

    def test_unsupported_mime_rejected(self):
        self.assertEqual(self.post(mimetype="text/plain")[0],415)

    def test_browser_document_exists(self):
        code,body,_=self.do("/scratch")
        self.assertEqual(code,200)
        self.assertIn(b"Scratch the living image",body)
        self.assertIn(b"reverse",body.lower())
        self.assertIn(b"input type=\"range\"",body)

    def test_token_guard_on_startup(self):
        with self.assertRaises(ValueError):
            ScratchServer(("127.0.0.1",0),"")


if __name__=="__main__":
    unittest.main()
