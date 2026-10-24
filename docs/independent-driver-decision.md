# Decision: independent Intensity Extreme driver

**Current deployment (2026-10-24):** Keep Sierra as the physical Thunderbolt/PCI owner on the silver Mac; Linux/Docker is a media-service guest. Native Linux PCI drivers cannot own the Sierra-controlled A124 from a container. The independent driver work below is *research*, and the physical HDMI/driver repair is still unverified. [Silver Linux/Docker witness](../tests/2026-10-24-silver-sierra-docker-runtime.md).

**Historical optional physical-driver path, not the current deployment:** The original Blackmagic Intensity Extreme PCIe/Thunderbolt interface can be researched on current Ubuntu, with the silver 2011 Mac mini as the physical reference. Ubuntu presently has Thunderbolt host controllers, but the actual A124 card remains on silver; the Ubuntu register interface is **injected read-only replay**, not a full emulated PCI endpoint and not physical capture. Earlier Sierra driver-upgrade, reinstall and recovery investigation is an OPTIONAL side branch, not a prerequisite for Ubuntu hardware engineering. Media/browser simulation is a **downstream consumer** of the eventual source-owned hardware device, not its substitute. [Actual topology, source-owned Ubuntu hardware contract, physical acceptance](a124-hardware-fidelity-ubuntu.md).

Date: 2026-10-23. Status: design approved; an independent hardware driver is **not yet functional**.

## Why

The vendor CoreMediaIO/DAL plugin on macOS Sierra crashes other applications. Isolating it restored App Store stability. Its four-byte patch candidate did not restore video-device registration. The *separate* vendor kernel driver still exposes native DeckLink interfaces, but our 1080p25, 1080p30 and 720p60 capture experiments reported no HDMI signal lock or captured frames. The original capture/playback hardware is half-duplex.

**Decision:** build an independently owned IOKit hardware driver for the original Intensity Extreme (Thunderbolt-tunneled PCI Express, vendor 0xBDBD, device 0xA124), instead of treating vendor binary patching as the repair.

## Architecture boundaries

1. Original vendor kernel driver: preserved as a verified control; not removed or modified during discovery.
2. Independent kernel driver: native PCI initialization, firmware-readiness contract, verified register protocol, bounded DMA, interrupts and timestamps. **None of these functions has yet been implemented independently.**
3. Host userland interface: independent video/audio frames, capture status, clock and capability reporting.
4. Optional Sierra CoreMediaIO Device Abstraction Layer bridge: *after* hardware capture works; not a driver substitute.
5. Higher-level video transport and Reframe/Teatro presentation: separate replaceable consumers.

## Verifiable gates

- **D0 Preserve:** record hashes, I/O Registry inventory, original bootable configuration and rollback. Keep the defective vendor DAL plugin isolated.
- **D1 PCI inventory:** run a self-authored, read-only user-space IOKit enumerator for PCI identifier 0xBDBD:0xA124. This does not prove the device can capture.
- **D2 Kernel attachment:** write, build and load a deliberately non-autostarting IOKit IOService/IOPCIDevice probe, after establishing rollback and an authorized test boot. No competition with the vendor driver in the stable boot.
- **D3 Protocol specification:** characterize firmware, register ownership, safe DMA and interrupt protocol from authorized, locally retained binaries and measured test results. Publish observations and source-owned behavior only, not proprietary firmware or decompiler dumps.
- **D4 Real capture:** with the vendor kernel driver absent *only on a recoverable test configuration*, lock a known unprotected HDMI source and capture one real, timestamped frame under the independent driver.
- **D5 Hardening:** test repeated capture, formats, signal loss and re-acquisition, audio sync and analog interfaces.
- **D6 Integration:** optional Sierra CoreMediaIO DAL support, storage and Reframe/Teatro consumers.

## Release rule

Never declare the hardware repaired because PCI enumeration works, a virtual backend produces frames, a streaming call succeeds, or CoreMediaIO sees a device. **The acceptance signal is a reproducible, physical input frame captured by the independent driver with a documented rollback.**

No firmware flashing, EEPROM edits or speculative memory-mapped writes; no Syphon dependency; no GitHub CI. Keep original vendor assets private and unmodified.
