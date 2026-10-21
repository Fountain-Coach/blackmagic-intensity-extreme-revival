# The Fountain Video Instrument: proposed architecture

**Status: design direction, not implemented software.**

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

**Explicit exclusion:** Syphon is not part of the target stack.
