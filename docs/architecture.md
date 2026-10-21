# The Fountain Video Instrument: proposed architecture

**Status: virtual capture/playback backend implemented and tested; physical and cross-platform native adapters remain in design.**

The Intensity Extreme is only one candidate backend. We separate the physical hardware from the semantics of capture, recording, transformation and presentation.

```text
Intensity Extreme (Thunderbolt / tunneled PCI Express)
    |
[hardware backend: legacy IOKit today, independent backend researched]
    |
[typed video frames + audio samples + timestamps + capabilities]
    |
[host capture service: diagnostics, capture, archive, playback]
    |
[standard transport and archival formats]
    |
Reframe / Teatro on macOS, iPadOS and Linux
```

**Boundaries**
- The observed original Intensity Extreme is **half-duplex**: capture mode and playback mode cannot be enabled simultaneously. This was measured in both activation orders and agrees with Blackmagic SDK guidance. Do not promise a simultaneous same-device capture/processing/playback pipeline; use separate devices or capture and playback in separate sessions. [Evidence](../tests/2026-10-21-half-duplex-loopback.md).
- A host physically connected to the device handles its platform-specific controller and drivers. An iPad on WLAN is a client, not a direct Thunderbolt-1 PCI host.
- Independent, documented transport (potentially WebRTC for interactive preview and conventional video files for archives) should not dictate proprietary application semantics.
- MIDI 2.0 may orchestrate scenarios and timing where appropriate; it is **not** a substitute for a high-bandwidth video transport.
- Original recordings remain separate from derived compositions and transformations.
- Every backend must report capabilities, capture errors, timestamp provenance, and an honest readiness state.

**Milestones**
1. Read-only device, firmware, and readiness probe against the installed legacy driver.
2. Deterministic test-frame acquisition with an unprotected source.
3. Recorded and network-accessible frames with timestamps.
4. One host-independent visual transformation and playback scenario.
5. Investigation of a replacement hardware backend after low-level behavior is sufficiently known.

**Implemented reference:** the [Virtual Video Instrument](virtual-video-instrument.md) supplies deterministic YUV frames, rational timestamps, half-duplex behavior and a browser preview for development without physical video equipment. The real HDMI receiver remains unverified.

**Explicit exclusion:** Syphon is not part of the target stack.
