# Fountain Video Instrument — software-first architecture

**2026-10-24 hardware-owner correction:** The route of record is **original Blackmagic Intensity Extreme PCIe/Thunderbolt interface on current Ubuntu**, with the silver 2011 Mac mini as an independently witnessed physical reference. Ubuntu presently has Thunderbolt host controllers, but the actual A124 card remains on silver; the Ubuntu register interface is **injected read-only replay**, not a full emulated PCI endpoint and not physical capture. Earlier Sierra driver-upgrade, reinstall and recovery investigation is an OPTIONAL side branch, not a prerequisite for Ubuntu hardware engineering. Media/browser simulation is a **downstream consumer** of the eventual source-owned hardware device, not its substitute. [Actual topology, source-owned Ubuntu hardware contract, physical acceptance](a124-hardware-fidelity-ubuntu.md).

**Implemented:** full simulated capture, playback, camera, cross-device virtual cable, stereo audio, frame clock, faults, recorder interchange and browser controls. **Optional:** the Sierra DeckLink/PCI hardware backend, which remains independently unverified for a real Nikon HDMI image.

[Launch and run the full software laboratory](full-software-video-laboratory.md) — private WLAN http://192.168.8.50:8272/lab.

## Design ownership

All downstream application semantics belong to the video instrument, not the Blackmagic card, Syphon, Nikon Live View, HDMI electrical ports or a named proprietary API. The physical host is an optional adapter, not a precondition of developing or running the instrument.

    Reframe / Teatro / browser / local recorder
                       |
    source-owned media and control contract:
      YUV 2vuy, stereo PCM, timestamps, capabilities,
      signal status, frame scheduling, errors and provenance
                       |
             Video Instrument backend
                  /             \
         FULL SOFTWARE       OPTIONAL HARDWARE
         deterministic lab      Sierra DeckLink
         two simulated          Intensity Extreme
         half-duplex units      independently witnessed
                  \             /
                 same application meaning

### Media contract

- Video: true generated (not captured) YUV 4:2:2 8-bit buffers with width, height, stride, format, rational PTS, source and CRC.
- Audio: 48 kHz, 16-bit signed stereo with exact samples-per-frame and sample index including 29.97 / 59.94 fractional rates.
- Frame source: virtual camera, virtual output, or optional independently verified physical device.
- Timing: a monotonic logical media clock; network or actual hardware timestamps require an explicit adapter provenance.
- Capabilities: declarative properties, not inferred from the existence of a UI element.
- Safety: no protected input bypass; absent source and dropped/underrun frames are explicit fault states.
- Archival: generated test bytes and metadata are preserved losslessly in a synthetic raw archive; real footage must never be conflated with this archive.

### Half-duplex ownership

The physical Intensity Extreme is **half-duplex**; input and output cannot run simultaneously. The full simulator enforces the same restriction **per simulated device**. Its working loopback is not a single-device self-loop: it uses two separate modeled units linked by a virtual source-owned signal. Video, PCM, and timestamps are transferred without regeneration in the receiving path. A same-unit capture/playback attempt fails with the observed legacy conflict code.

### Orthogonal contracts

Hardware/firmware/PCI/DMA, electrical format lock and real-camera settings are owned by the **optional physical adapter** and its independent acceptance tests. They are never prerequisites for testing media timing, recordings, publication clients, transformations, two-instrument routing, no-source UI, fault handling or downstream Reframe/Teatro software features.

Original older application flow — Black Syphon → Syphon → Vuo → NDI — is recovered in [historical evidence](historical-black-syphon-vuo-ndi-bridge.md). It is not a software-lab dependency. Syphon and a single historical creative-coding framework are not mandatory abstractions: the needed semantics are owned by the instrument, and any adapter is replaceable.

## Verification boundary

- Virtual tests: Python 3.10+ standard library; current local suite **70 tests passed** on Ubuntu with the camera switched off.
- Native macOS GUI: Fountain Video on Sierra defaults to a generated, visibly labeled synthetic source, with a shortcut to the full lab; selecting real hardware is explicit.
- Real capture: **not accepted**. No actual HDMI image or live source frame has yet been obtained from the Nikon/Intensity Extreme.
- Real output sink, actual interlacing, analog electrical conversion, kernel-driver ownership and real Syphon/NDI publishing are not simulated and cannot be claimed as achieved based on virtual tests.

No GitHub CI. Git source and local witnessed tests are authoritative.
