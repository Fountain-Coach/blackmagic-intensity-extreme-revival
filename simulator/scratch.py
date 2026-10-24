"""Bounded, source-neutral compressed video frame history.

A scratch instrument needs independently addressable past frames. An MJPEG
stream alone is live-only and cannot rewind. Frames below are immutable,
identified by monotonically increasing receiver sequence IDs and dropped from
the oldest end on count/byte bounds.

Source provenance is explicit: 'synthetic' or 'decklink-observed'.
'decklink-observed' means submitted by the native user's DeckLink callback,
NOT independent proof that the HDMI receiver or Nikon is working.
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass
from threading import RLock
import time

MAX_FRAME_BYTES = 1_048_576
VALID_ORIGINS = ("synthetic", "decklink-observed")


class InvalidFrame(ValueError):
    pass


@dataclass(frozen=True)
class ScratchFrame:
    seq: int
    data: bytes
    pts_ns: int
    origin: str
    width: int
    height: int
    created_at: float

    @property
    def metadata(self):
        return {"seq": self.seq, "pts_ns": self.pts_ns,
                "origin": self.origin, "simulated": self.origin == "synthetic",
                "physical_hdmi_verified": False,
                "width": self.width, "height": self.height,
                "jpeg_bytes": len(self.data)}


class ScratchHistory:
    def __init__(self, max_frames=144, max_bytes=24_000_000, clock=None):
        if max_frames < 1 or max_bytes < 100:
            raise ValueError("Invalid history limits")
        self.max_frames = max_frames
        self.max_bytes = max_bytes
        self._clock = clock or time.time
        self._lock = RLock()
        self._items = deque()
        self._index = {}
        self._next_seq = 0
        self._byte_count = 0
        self._evicted = 0
        self._epoch = 0
        self._last_epoch_reason = "initial"

    def push(self, image, pts_ns, origin, width, height):
        if not isinstance(image, bytes) or not image.startswith(b"\xff\xd8") or \
           not image.endswith(b"\xff\xd9"):
            raise InvalidFrame("Expected a complete JPEG frame")
        if len(image) < 4 or len(image) > MAX_FRAME_BYTES:
            raise InvalidFrame("JPEG frame exceeds supported size")
        if type(pts_ns) is not int or pts_ns < 0 or pts_ns > 2**63 - 1:
            raise InvalidFrame("Invalid frame timestamp")
        if origin not in VALID_ORIGINS:
            raise InvalidFrame("Unrecognized frame provenance")
        if type(width) is not int or type(height) is not int or \
           not 1 <= width <= 4096 or not 1 <= height <= 2160:
            raise InvalidFrame("Invalid frame geometry")
        if len(image) > self.max_bytes:
            raise InvalidFrame("Frame larger than total history capacity")
        with self._lock:
            if self._items:
                prior=self._items[-1]
                if origin != prior.origin or pts_ns < prior.pts_ns:
                    self._epoch += 1
                    self._last_epoch_reason = ("source_changed" if origin != prior.origin
                                               else "timestamp_rollback")
                    self._evicted += len(self._items)
                    self._items.clear()
                    self._index.clear()
                    self._byte_count = 0
            seq = self._next_seq
            self._next_seq += 1
            item = ScratchFrame(seq, image, pts_ns, origin,
                                width, height, self._clock())
            self._items.append(item)
            self._index[seq] = item
            self._byte_count += len(image)
            while len(self._items) > self.max_frames or \
                  self._byte_count > self.max_bytes:
                gone = self._items.popleft()
                del self._index[gone.seq]
                self._byte_count -= len(gone.data)
                self._evicted += 1
            return item

    def get(self, seq):
        with self._lock:
            return self._index.get(seq)

    def snapshot(self):
        with self._lock:
            entries = list(self._items)
            now = self._clock()
            latest = entries[-1] if entries else None
            return {
                "instrument": "fountain-video-browser-scratch",
                "history": [frame.metadata for frame in entries],
                "count": len(entries),
                "oldest_seq": entries[0].seq if entries else None,
                "latest_seq": latest.seq if latest else None,
                "latest": latest.metadata if latest else None,
                "age_seconds": round(max(0, now-latest.created_at), 3)
                               if latest else None,
                "source_online": bool(latest and now-latest.created_at < 2.5),
                "max_frames": self.max_frames,
                "max_bytes": self.max_bytes,
                "retained_bytes": self._byte_count,
                "evicted_frames": self._evicted,
                "stream_epoch": self._epoch,
                "epoch_reason": self._last_epoch_reason,
                "scratching": {
                    "random_access": True,
                    "forward": True,
                    "reverse": True,
                    "freeze": True,
                    "jog": True,
                    "bounded_history": True
                },
                "hardware_hdmi_capture_accepted": False,
            }
