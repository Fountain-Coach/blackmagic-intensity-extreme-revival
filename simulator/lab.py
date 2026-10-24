"""Full software-only video instrument laboratory.

Two independent *virtual* half-duplex Intensity-style devices, one producer
and one receiver, connected by a typed *virtual* signal link. A Nikon-like
software camera can replace the producer. Video pixels, audio, timestamps,
capture archives, connector state and faults are generated in Python.

There is deliberately NO hardware/driver/CEC/USB/PCI/HDMI/firmware access.
This is a protocol-level simulation, not an electrical HDMI simulator.
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass
from io import BytesIO
import json
import struct
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED
from zlib import crc32

from .model import DeviceError, MODE_BY_NAME, VirtualIntensityExtreme, VirtualSource, make_yuv422_bars

CONNECTORS = ("HDMI", "Component", "Composite", "S-Video")
PATTERNS = ("bars", "checker", "moving")
SCENARIOS = ("camera", "camera-off", "disconnected", "mismatch",
             "protected", "drops", "loopback", "playback")
AUTO_DETECT = ("off", "once", "continuous")
AUDIO_SAMPLE_RATE = 48000
AUDIO_CHANNELS = 2
MAX_HISTORY = 6


@dataclass(frozen=True)
class MediaPacket:
    video: bytes
    audio: bytes
    metadata: dict


def validate_mode(mode: str, connector: str) -> None:
    if mode not in MODE_BY_NAME:
        raise DeviceError("MODE_UNSUPPORTED", "Unknown video format")
    if connector not in CONNECTORS:
        raise DeviceError("CONNECTOR_UNSUPPORTED", "Unknown input/output connector")
    if connector in ("Composite", "S-Video") and mode not in (
        "PAL", "NTSC", "NTSC 23.98", "PAL Progressive", "NTSC Progressive"):
        raise DeviceError("CONNECTOR_MODE_UNSUPPORTED",
                          "Virtual composite and S-Video fixtures require SD mode")


def make_pattern(mode_name: str, frame: int, pattern: str) -> bytes:
    """Full-size, correctly ordered, 8-bit packed YUV422 2vuy fixture."""
    mode = MODE_BY_NAME[mode_name]
    if pattern not in PATTERNS:
        raise DeviceError("PATTERN_UNSUPPORTED", "Unknown virtual source pattern")
    if pattern == "bars":
        return make_yuv422_bars(mode, frame)
    pairs = mode.width // 2
    if pattern == "checker":
        # Spatial checkerboard with deterministic moving tile phase.
        white = bytes((128, 218, 128, 218))
        dark = bytes((128, 40, 128, 40))
        top = b"".join(
            (white if ((x // 32 + frame // 3) % 2) == 0 else dark)
            for x in range(pairs))
        bottom = b"".join(
            (dark if ((x // 32 + frame // 3) % 2) == 0 else white)
            for x in range(pairs))
        return b"".join(top if (y // 40) % 2 == 0 else bottom
                        for y in range(mode.height))
    # A synthetic "lens view": a light rectangle moving on a dark field,
    # with its motion encoded into both the YUV payload and frame metadata.
    dark = bytes((128, 40, 128, 40)) * pairs
    white = bytes((128, 220, 128, 220))
    left_pair = (frame * 11) % max(1, pairs - min(pairs, 96))
    width_pairs = min(pairs, 96)
    middle = (dark[:left_pair*4] + white*width_pairs +
              dark[(left_pair+width_pairs)*4:])
    top = mode.height // 3
    bottom = min(mode.height, top + max(10, mode.height // 5))
    return dark*top + middle*(bottom-top) + dark*(mode.height-bottom)


def pcm_samples(first_sample: int, count: int, muted: bool = False) -> bytes:
    """Source-owned deterministic stereo PCM; no microphone access.

    Left: 1 kHz triangular wave. Right: 500 Hz triangular wave.
    Both stay within signed-16-bit range and use 48k sample rate.
    """
    if count < 0 or count > 48000 or first_sample < 0:
        raise DeviceError("AUDIO_RANGE", "Invalid audio sample interval")
    if muted:
        return b"\0" * (count * 4)
    result = bytearray(count * 4)
    for i in range(count):
        n = first_sample + i
        phase_left = n % 48
        phase_right = n % 96
        left = (phase_left if phase_left < 24 else 48-phase_left) * 1000 - 12000
        right = (phase_right if phase_right < 48 else 96-phase_right) * 500 - 12000
        struct.pack_into("<hh", result, i*4, left, right)
    return bytes(result)


def audio_interval(mode_name: str, frame: int) -> tuple[int, int]:
    mode = MODE_BY_NAME[mode_name]
    first = frame * AUDIO_SAMPLE_RATE * mode.rate_den // mode.rate_num
    last = (frame+1) * AUDIO_SAMPLE_RATE * mode.rate_den // mode.rate_num
    return first, last-first


def make_packet(mode_name: str, frame: int, pattern: str,
                origin: str, connector: str, muted: bool = False) -> MediaPacket:
    validate_mode(mode_name, connector)
    raw = make_pattern(mode_name, frame, pattern)
    mode = MODE_BY_NAME[mode_name]
    first, count = audio_interval(mode_name, frame)
    pcm = pcm_samples(first, count, muted)
    pts = frame * 1_000_000_000 * mode.rate_den // mode.rate_num
    meta = {
        "frame": frame, "pts_ns": pts,
        "duration_ns": ((frame+1)*1_000_000_000*mode.rate_den//mode.rate_num)-pts,
        "mode": mode.name, "width": mode.width, "height": mode.height,
        "interlaced": mode.name.startswith("1080i"),
        "pixel_format": "2vuy", "stride_bytes": mode.width*2,
        "video_bytes": len(raw), "video_crc32": "%08x" % crc32(raw),
        "source": origin, "connector": connector, "pattern": pattern,
        "audio": {
            "sample_rate": AUDIO_SAMPLE_RATE, "channels": AUDIO_CHANNELS,
            "format": "signed_pcm_16_le", "start_sample": first,
            "samples_per_channel": count, "bytes": len(pcm),
            "crc32": "%08x" % crc32(pcm), "muted": muted,
        },
        "simulated": True, "physical_signal_verified": False,
        "provenance": "SYNTHETIC: source-owned video and audio only",
    }
    return MediaPacket(raw, pcm, meta)


class VideoInstrumentLab:
    """Full-stack deterministic video/audio test bench, independent of hardware.

    The *receiver* and *transmitter* are separately simulated half-duplex
    devices. Their virtual signal link carries the SAME bytes, checksums and
    presentation time: they are not independent, coincidentally matching bars.
    """

    def __init__(self):
        self.receiver = VirtualIntensityExtreme()
        self.transmitter = VirtualIntensityExtreme()
        self.events: list[dict] = []
        self.captured: deque[MediaPacket] = deque(maxlen=MAX_HISTORY)
        self.emitted: deque[MediaPacket] = deque(maxlen=MAX_HISTORY)
        self.schedule: deque[str] = deque(maxlen=120)
        self.serial = 0
        self.reset()

    def _event(self, kind: str, **details) -> None:
        self.events.append({"event": kind, "tick": self.serial, **details})
        self.events = self.events[-60:]

    def reset(self, scenario: str = "camera", mode: str = "720p50",
              source_mode: str | None = None, connector: str = "HDMI",
              pattern: str = "moving", auto_detect: str = "off") -> dict:
        if scenario not in SCENARIOS or pattern not in PATTERNS or auto_detect not in AUTO_DETECT:
            raise DeviceError("BAD_SCENARIO", "Unknown scenario, pattern or detection policy")
        validate_mode(mode, connector)
        source_mode = source_mode or mode
        validate_mode(source_mode, connector)
        self.receiver.stop()
        self.transmitter.stop()
        self.source_mode = source_mode
        self.connector = connector
        self.pattern = pattern
        self.scenario = scenario
        self.auto_detect = auto_detect
        self.camera_power = scenario != "camera-off"
        self.live_view = True
        self.cable_connected = scenario not in ("disconnected",)
        self.source_protected = scenario == "protected"
        self.drop_every = 3 if scenario == "drops" else 0
        self.mute_audio = False
        self.output_auto_fill = True
        self.captured.clear()
        self.emitted.clear()
        self.schedule.clear()
        self.serial = 0
        self.input_locked_before = False
        self.last_crc_equal = None
        self.events.clear()
        if scenario in ("loopback", "playback"):
            self.transmitter.enable_playback(mode)
            self.transmitter.start()
        if scenario != "playback":
            capture_source_mode = self.source_mode
            if scenario == "loopback":
                capture_source_mode = mode
                self.source_mode = mode
            if scenario == "mismatch":
                capture_source_mode = self._different_mode(mode, connector)
                self.source_mode = capture_source_mode
            self.receiver.source = VirtualSource(mode=capture_source_mode)
            self.receiver.enable_capture(mode)
            self.receiver.start()
        self._event("reset", scenario=scenario, mode=mode, source_mode=self.source_mode,
                    connector=connector, detection=auto_detect)
        return self.snapshot()

    @staticmethod
    def _different_mode(mode: str, connector: str) -> str:
        for other in ("720p50", "1080p25", "PAL", "NTSC", "720p60"):
            try:
                validate_mode(other, connector)
            except DeviceError:
                continue
            if other != mode:
                return other
        raise DeviceError("NO_ALTERNATE_MODE", "No alternate mode for fixture")

    def change_source(self, **changes) -> dict:
        supported = {"mode", "connector", "pattern", "power", "live_view",
                     "connected", "protected", "drop_every", "mute_audio",
                     "auto_detect", "output_auto_fill"}
        if set(changes) - supported:
            raise DeviceError("UNKNOWN_CONTROL", "Unknown virtual signal control")
        for key in ("power", "live_view", "connected", "protected",
                    "mute_audio", "output_auto_fill"):
            if key in changes and type(changes[key]) is not bool:
                raise DeviceError("BAD_OPTIONS", key+" requires a Boolean")
        if "drop_every" in changes and (
            type(changes["drop_every"]) is not int or
            not 0 <= changes["drop_every"] <= 120):
            raise DeviceError("BAD_OPTIONS", "drop_every requires an integer in 0..120")
        target_mode = changes.get("mode", self.source_mode)
        target_connector = changes.get("connector", self.connector)
        validate_mode(target_mode, target_connector)
        if "pattern" in changes and changes["pattern"] not in PATTERNS:
            raise DeviceError("PATTERN_UNSUPPORTED", "Unknown pattern")
        if "auto_detect" in changes and changes["auto_detect"] not in AUTO_DETECT:
            raise DeviceError("BAD_OPTIONS", "Unknown detection policy")
        for key, value in changes.items():
            if key == "mode":
                self.source_mode = value
            elif key == "power":
                self.camera_power = value
            elif key == "connected":
                self.cable_connected = value
            elif key == "protected":
                self.source_protected = value
            elif key == "connector":
                self.connector = value
            elif key == "pattern":
                self.pattern = value
            elif key == "auto_detect":
                self.auto_detect = value
            else:
                setattr(self, key, value)
        self._event("source_changed", controls=sorted(changes))
        return self.snapshot()

    def queue_output(self, pattern: str = "moving", count: int = 1) -> dict:
        if self.transmitter.activity != "playback":
            raise DeviceError("OUTPUT_INACTIVE", "Playback is not active")
        if pattern not in PATTERNS:
            raise DeviceError("PATTERN_UNSUPPORTED", "Unknown pattern")
        if type(count) is not int or not 1 <= count <= 30:
            raise DeviceError("INVALID_COUNT", "Queue 1..30 frames")
        if len(self.schedule) + count > self.schedule.maxlen:
            raise DeviceError("OUTPUT_QUEUE_FULL", "Scheduled output buffer is full")
        self.schedule.extend([pattern]*count)
        self._event("output_scheduled", pattern=pattern, count=count)
        return self.snapshot()

    def _active_source(self) -> tuple[bool, str, bool]:
        if self.scenario == "loopback":
            active = self.cable_connected and self.transmitter.streaming
            return active, self.transmitter.mode.name, False
        if self.scenario == "playback":
            return False, self.source_mode, False
        return (self.camera_power and self.live_view and self.cable_connected,
                self.source_mode, self.source_protected)

    def _auto_detect_if_requested(self, active: bool, mode: str) -> None:
        if not active or not self.receiver.streaming or self.receiver.mode is None:
            return
        if mode == self.receiver.mode.name or self.auto_detect == "off":
            return
        validate_mode(mode, self.connector)
        old = self.receiver.mode.name
        self.receiver.stop()
        self.receiver.enable_capture(mode)
        self.receiver.start()
        self._event("format_changed", previous=old, detected=mode,
                    policy=self.auto_detect)
        if self.auto_detect == "once":
            self.auto_detect = "off"

    def _copy_to_device(self, device: VirtualIntensityExtreme, packet: MediaPacket,
                        direction: str) -> MediaPacket:
        # Called ONLY after the simulated device advanced successfully.
        metadata = dict(packet.metadata)
        metadata["direction"] = direction
        metadata["device"] = ("virtual-rx" if direction == "capture" else "virtual-tx")
        result = MediaPacket(packet.video, packet.audio, metadata)
        device.last_frame = packet.video
        device.last_metadata = metadata
        return result

    def step(self, count: int = 1) -> dict:
        if type(count) is not int or not 1 <= count <= 30:
            raise DeviceError("INVALID_COUNT", "Step 1..30 ticks")
        if not self.receiver.streaming and not self.transmitter.streaming:
            raise DeviceError("NOT_STREAMING", "No virtual device is running")
        for _ in range(count):
            output_packet = None
            if self.transmitter.streaming:
                if self.schedule:
                    output_pattern = self.schedule.popleft()
                elif self.output_auto_fill:
                    output_pattern = self.pattern
                else:
                    output_pattern = None
                if output_pattern is None:
                    # Deliberate producer underflow; no stale frame is replayed.
                    self.transmitter.dropped += 1
                    self.transmitter.frame_number += 1
                    self._event("output_underflow")
                else:
                    produced = self.transmitter.advance(1)
                    if len(produced) != 1:
                        raise AssertionError("Virtual playback failed unexpectedly")
                    out_mode = self.transmitter.mode.name
                    frame = self.transmitter.frame_number - 1
                    packet = make_packet(out_mode, frame, output_pattern,
                                         "virtual-playback", self.connector,
                                         self.mute_audio)
                    output_packet = self._copy_to_device(self.transmitter, packet,
                                                         "playback")
                    self.emitted.append(output_packet)
            if self.receiver.streaming:
                active, source_mode, protected = self._active_source()
                if self.scenario == "loopback" and output_packet is None:
                    active = False
                self._auto_detect_if_requested(active, source_mode)
                self.receiver.source = VirtualSource(
                    mode=source_mode, connected=active, protected=protected,
                    drop_every=self.drop_every)
                was_locked = self.receiver.signal_locked
                if was_locked != self.input_locked_before:
                    self._event("signal_lock" if was_locked else "signal_lost",
                                format=self.receiver.mode.name)
                self.input_locked_before = was_locked
                captured = self.receiver.advance(1)
                if captured:
                    frame = self.receiver.frame_number-1
                    if self.scenario == "loopback":
                        assert output_packet is not None
                        packet = output_packet
                        assert packet.metadata["mode"] == self.receiver.mode.name
                    else:
                        packet = make_packet(self.receiver.mode.name, frame,
                                             self.pattern, "virtual-camera",
                                             self.connector, self.mute_audio)
                    inbound = self._copy_to_device(self.receiver, packet, "capture")
                    self.captured.append(inbound)
                    if self.scenario == "loopback":
                        self.last_crc_equal = (
                            inbound.metadata["video_crc32"] ==
                            output_packet.metadata["video_crc32"] and
                            inbound.video == output_packet.video and
                            inbound.metadata["pts_ns"] ==
                            output_packet.metadata["pts_ns"] and
                            inbound.audio == output_packet.audio)
                        if not self.last_crc_equal:
                            raise AssertionError("Virtual cable altered media bytes")
                        self._event("loopback_received",
                                    video_crc32=inbound.metadata["video_crc32"])
            self.serial += 1
        return self.snapshot()

    def deliberate_half_duplex_conflict(self) -> dict:
        active = self.receiver if self.receiver.activity != "idle" else self.transmitter
        if active.activity == "idle":
            raise DeviceError("NOT_ENABLED", "Start an instrument first")
        try:
            if active.activity == "capture":
                active.enable_playback(active.mode.name)
            else:
                active.enable_capture(active.mode.name)
        except DeviceError as error:
            self._event("half_duplex_blocked", code=error.code)
            return {"blocked": True, "code": error.code,
                    "simulated": True, "state": self.snapshot()}
        raise AssertionError("Half-duplex simulation accepted an invalid operation")

    def stop(self) -> dict:
        self.receiver.stop()
        self.transmitter.stop()
        self._event("stop")
        return self.snapshot()

    def snapshot(self) -> dict:
        active, source_mode, protected = self._active_source()
        rx = self.receiver.snapshot()
        tx = self.transmitter.snapshot()
        return {
            "backend": "full-software-video-lab", "simulated": True,
            "physical_device_access": False, "physical_signal_verified": False,
            "device_instances": 2, "half_duplex_per_device": True,
            "scenario": self.scenario, "tick": self.serial,
            "source": {
                "type": "virtual-camera" if self.scenario not in ("loopback", "playback")
                        else ("virtual-playback" if self.scenario == "loopback" else "none"),
                "power": self.camera_power, "live_view": self.live_view,
                "connected": self.cable_connected,
                "active": active, "protected": protected, "mode": source_mode,
                "connector": self.connector, "pattern": self.pattern,
                "drop_every": self.drop_every, "mute_audio": self.mute_audio,
            },
            "auto_detect": self.auto_detect, "output_auto_fill": self.output_auto_fill,
            "input": {"activity": rx["activity"], "mode": rx["mode"],
                      "streaming": rx["streaming"], "signal_locked": rx["signal_locked"],
                      "delivered": rx["frames_delivered"], "dropped": rx["frames_dropped"],
                      "last_frame": rx["last_frame"]},
            "output": {"activity": tx["activity"], "mode": tx["mode"],
                       "streaming": tx["streaming"], "delivered": tx["frames_delivered"],
                       "dropped": tx["frames_dropped"], "queue_depth": len(self.schedule),
                       "last_frame": tx["last_frame"]},
            "loopback": {"link": "virtual-memory-not-hdmi",
                         "last_video_audio_pts_identical": self.last_crc_equal},
            "archive": {"retained_capture_frames": len(self.captured),
                        "max_frames": MAX_HISTORY, "format": "raw-2vuy+pcm+json+zip"},
            "events": self.events[-18:],
        }

    def capabilities(self) -> dict:
        c = self.receiver.capabilities()
        return {"backend": "full-software-video-lab", "simulated": True,
                "physical_device_access": False, "physical_signal_verified": False,
                "device_count": 2, "half_duplex_per_device": True,
                "connectors": list(CONNECTORS), "modes": c["modes"],
                "patterns": list(PATTERNS), "scenarios": list(SCENARIOS),
                "auto_detection": list(AUTO_DETECT),
                "video": {"format": "2vuy", "bits_per_component": 8,
                          "interlace_metadata": True, "actual_interlace_fields": False},
                "audio": {"sample_rate": 48000, "channels": 2,
                          "sample_format": "signed_pcm_16_le",
                          "fractional_frame_sample_accumulator": True},
                "behaviors": ["capture", "playback", "two-device-loopback",
                              "half-duplex", "frame-preview", "audio-packets",
                              "signal-loss", "content-protection-block",
                              "mode-mismatch", "mode-auto-detect",
                              "drop-injection", "output-underflow",
                              "frame-scheduling", "bounded-archive"],
                "not_emulated": ["physical-HDMI-electrical-signaling",
                                 "actual-firmware-and-DMA",
                                 "HDCP-decryption", "physical-analog-conversion",
                                 "GPU-Syphon-or-NDI-server"]}

    def frame(self, direction: str) -> MediaPacket | None:
        if direction == "input":
            return self.captured[-1] if self.captured and self.receiver.signal_locked else None
        if direction == "output":
            return self.emitted[-1] if self.emitted else None
        raise DeviceError("BAD_DIRECTION", "Use input or output")

    def archive_zip(self) -> bytes:
        """Bounded source-owned YUV/PCM archive, not a physical recording."""
        if not self.captured:
            raise DeviceError("NO_FRAMES", "No synthetic capture frames are retained")
        dest = BytesIO()
        packets = list(self.captured)
        manifest = {
            "format": "fountain-video-simulation-v1", "simulated": True,
            "physical_device_access": False, "captured_from_hardware": False,
            "provenance": "SYNTHETIC MEDIA ONLY: DO NOT PRESENT AS REAL CAPTURE",
            "frames": [p.metadata for p in packets],
        }
        with ZipFile(dest, mode="w", compression=ZIP_DEFLATED, compresslevel=6) as archive:
            def write(name: str, data: bytes):
                info = ZipInfo(name, date_time=(1980,1,1,0,0,0))
                info.compress_type = ZIP_DEFLATED
                info.external_attr = 0o644 << 16
                archive.writestr(info, data)
            write("manifest.json", json.dumps(manifest, sort_keys=True,
                                              separators=(",", ":")).encode("utf-8"))
            for packet in packets:
                frame_id = packet.metadata["frame"]
                root = "synthetic-%06d" % frame_id
                write(root + ".2vuy", packet.video)
                write(root + ".pcm", packet.audio)
            write("README.txt", b"Fountain synthetic video instrument archive.\n"
                  b"Frames: packed 8-bit YUV422 2vuy, width/height/stride in manifest.\n"
                  b"Audio: signed little-endian 16-bit stereo at 48 kHz; "
                  b"per-packet sample indexes and PTS in manifest.\n"
                  b"Generated entirely in software. NO actual HDMI data.\n")
        return dest.getvalue()
