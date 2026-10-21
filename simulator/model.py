"""Deterministic Fountain video-device simulator; no vendor SDK and no hardware access.

Contract: capture OR playback; explicit mode and input source; generated YUV 4:2:2 frames.
Simulation evidence must never be presented as a physical HDMI signal test.
"""
from __future__ import annotations

from dataclasses import dataclass
from zlib import crc32


class DeviceError(RuntimeError):
    def __init__(self, code: str, message: str):
        self.code = code
        super().__init__(message)


@dataclass(frozen=True)
class VideoMode:
    name: str
    width: int
    height: int
    rate_num: int
    rate_den: int

    @property
    def fps(self) -> float:
        return self.rate_num / self.rate_den

    def as_dict(self) -> dict:
        return {"name": self.name, "width": self.width, "height": self.height,
                "rate": f"{self.rate_num}/{self.rate_den}", "fps": self.fps}


MODES = (
    VideoMode("NTSC", 720, 486, 30000, 1001),
    VideoMode("NTSC 23.98", 720, 486, 24000, 1001),
    VideoMode("PAL", 720, 576, 25, 1),
    VideoMode("NTSC Progressive", 720, 486, 60000, 1001),
    VideoMode("PAL Progressive", 720, 576, 50, 1),
    VideoMode("1080p23.98", 1920, 1080, 24000, 1001),
    VideoMode("1080p24", 1920, 1080, 24, 1),
    VideoMode("1080p25", 1920, 1080, 25, 1),
    VideoMode("1080p29.97", 1920, 1080, 30000, 1001),
    VideoMode("1080p30", 1920, 1080, 30, 1),
    VideoMode("1080i50", 1920, 1080, 25, 1),
    VideoMode("1080i59.94", 1920, 1080, 30000, 1001),
    VideoMode("1080i60", 1920, 1080, 30, 1),
    VideoMode("720p50", 1280, 720, 50, 1),
    VideoMode("720p59.94", 1280, 720, 60000, 1001),
    VideoMode("720p60", 1280, 720, 60, 1),
)
MODE_BY_NAME = {m.name: m for m in MODES}

# Chroma, luminance, chroma (Cb, Y, Cr). Deliberately synthetic color bars.
YUV_BARS = (
    (128, 235, 128), (16, 210, 146), (166, 170, 16), (54, 145, 34),
    (202, 106, 222), (90, 81, 240), (240, 41, 110), (128, 16, 128),
)


@dataclass
class VirtualSource:
    mode: str = "720p50"
    connected: bool = True
    protected: bool = False
    drop_every: int = 0

    def as_dict(self) -> dict:
        return {"mode": self.mode, "connected": self.connected,
                "protected": self.protected, "drop_every": self.drop_every}


def make_yuv422_bars(mode: VideoMode, number: int = 0) -> bytes:
    """Generate YUV 4:2:2 (2vuy: Cb, Y0, Cr, Y1), no real capture."""
    pairs = mode.width // 2
    parts = []
    for index, (cb, y, cr) in enumerate(YUV_BARS):
        first = (pairs * index) // len(YUV_BARS)
        last = (pairs * (index + 1)) // len(YUV_BARS)
        parts.append(bytes((cb, y, cr, y)) * (last - first))
    row = b"".join(parts)
    assert len(row) == mode.width * 2
    # First scanline carries a deterministic 32-bit logical frame counter.
    marker = bytearray(row)
    for bit in range(32):
        offset = bit * 4
        level = 235 if (number >> bit) & 1 else 16
        marker[offset + 1] = level
        marker[offset + 3] = level
    return bytes(marker) + row * (mode.height - 1)


class VirtualIntensityExtreme:
    """A replaceable simulator backend. Never imports Blackmagic software."""

    def __init__(self, source: VirtualSource | None = None):
        self.source = source or VirtualSource()
        self.activity = "idle"
        self.mode: VideoMode | None = None
        self.streaming = False
        self.frame_number = 0
        self.delivered = 0
        self.dropped = 0
        self.last_frame: bytes | None = None
        self.last_metadata: dict | None = None
        self.events: list[dict] = []

    def _event(self, kind: str, **details):
        self.events.append({"event": kind, **details})
        self.events = self.events[-40:]

    def capabilities(self) -> dict:
        return {"backend": "virtual", "simulated": True, "device": "Intensity Extreme (model)",
                "half_duplex": True, "video_input": True, "video_output": True,
                "connectors": ["HDMI", "Component", "Composite", "S-Video"],
                "max_audio_channels": 2,
                "pixel_format": "2vuy (8-bit YUV 4:2:2)",
                "modes": [mode.as_dict() for mode in MODES],
                "proprietary_driver_required": False,
                "physical_signal_verified": False}

    def _enable(self, activity: str, mode: str, pixel_format: str):
        if self.activity != "idle":
            raise DeviceError("0x80000009", "Half-duplex conflict: stop current activity first")
        if mode not in MODE_BY_NAME:
            raise DeviceError("MODE_UNSUPPORTED", "Unsupported video mode")
        if pixel_format != "2vuy":
            raise DeviceError("PIXEL_UNSUPPORTED", "This simulation implements only 8-bit 2vuy")
        self.activity = activity
        self.mode = MODE_BY_NAME[mode]
        self.streaming = False
        self.frame_number = self.delivered = self.dropped = 0
        self.last_frame = None
        self.last_metadata = None
        self._event("enable", direction=activity, mode=mode)

    def enable_capture(self, mode: str = "720p50", pixel_format: str = "2vuy"):
        self._enable("capture", mode, pixel_format)

    def enable_playback(self, mode: str = "720p50", pixel_format: str = "2vuy"):
        self._enable("playback", mode, pixel_format)

    def start(self):
        if self.activity == "idle":
            raise DeviceError("NOT_ENABLED", "Enable capture or playback first")
        self.streaming = True
        self._event("start", direction=self.activity)

    def stop(self):
        self._event("stop", direction=self.activity)
        self.streaming = False
        self.activity = "idle"
        self.mode = None

    @property
    def signal_locked(self) -> bool:
        return (self.activity == "capture" and self.streaming and
                self.source.connected and not self.source.protected and
                self.mode is not None and self.source.mode == self.mode.name)

    def advance(self, count: int = 1) -> list[dict]:
        if not isinstance(count, int) or not (1 <= count <= 30):
            raise DeviceError("INVALID_COUNT", "Advance 1–30 frames at a time")
        if not self.streaming or self.activity == "idle":
            raise DeviceError("NOT_STREAMING", "Start capture or playback")
        assert self.mode is not None
        produced = []
        for _ in range(count):
            number = self.frame_number
            self.frame_number += 1
            if self.activity == "capture" and not self.signal_locked:
                self.dropped += 1
                self._event("no_signal", frame=number)
                continue
            if self.activity == "capture" and self.source.drop_every > 0 and (
                    (number + 1) % self.source.drop_every == 0):
                self.dropped += 1
                self._event("dropped", frame=number)
                continue
            frame = make_yuv422_bars(self.mode, number)
            ns = number * self.mode.rate_den * 1_000_000_000 // self.mode.rate_num
            meta = {"frame": number, "pts_ns": ns, "width": self.mode.width,
                    "height": self.mode.height, "pixel_format": "2vuy",
                    "crc32": f"{crc32(frame):08x}", "bytes": len(frame),
                    "provenance": "synthetic; not captured from physical HDMI",
                    "direction": self.activity}
            self.last_frame = frame
            self.last_metadata = meta
            self.delivered += 1
            produced.append(meta)
        return produced

    def snapshot(self) -> dict:
        return {"simulated": True, "physical_signal_verified": False,
                "device": "Virtual Intensity Extreme", "activity": self.activity,
                "streaming": self.streaming,
                "mode": self.mode.name if self.mode else None,
                "source": self.source.as_dict(), "signal_locked": self.signal_locked,
                "frame_counter": self.frame_number, "frames_delivered": self.delivered,
                "frames_dropped": self.dropped, "last_frame": self.last_metadata,
                "events": self.events[-8:]}
