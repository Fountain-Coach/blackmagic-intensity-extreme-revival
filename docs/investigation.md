# Investigation record — 21 October 2026

This document distinguishes **direct observation**, **static reconstruction**, and **untested hypotheses**. Addresses below refer to the inspected Mach-O image, not live kernel addresses.

## Confirmed setup

| Item | Observation |
| --- | --- |
| Host | Intel Mac mini (2011), macOS Sierra 10.12.6 |
| Topology | Thunderbolt host → THUNDERDOCK2 → Intensity Extreme |
| Vendor / device ID | `0xBDBD:0xA124` |
| Desktop Video | 10.4 |
| Driver state | Blackmagic DeckLink kernel components loaded, attached in I/O Registry |
| Media Express | Launches, shows “Blackmagic hardware could not be found in your system” |
| Capture status | Unknown: no successful video frame verified |

No firmware was flashed; no kernel driver was changed during investigation.

## Reproducible static analysis

The original, locally obtained driver is:
- Bundle path on the historical installation: `/Library/Extensions/DeckLink_Driver.kext/Contents/MacOS/DeckLink_Driver`
- Format: 64-bit Intel Mach-O `KEXTBUNDLE`
- SHA-256: `c4ec945aa46d07123788e66c219203026d1dcddffad20d1a55b8b5b4943d3780`
- Analysis tools: **REA 6.7.0**, **Ghidra 12.1.4**, **Java 21**, **Node.js 24.11.1**
- REA/Ghidra inventory: 2,963 recognized procedures and 2,684 defined strings (not proof of complete program recovery).

Analyze only a copy of software you are authorized to inspect, following applicable law and licensing conditions. The source binary is not part of this repository.

### Identified exported functions

| Symbol (class namespace abbreviated) | Image-relative address | Observation |
| --- | --- | --- |
| `DeckLink_Driver::isHardwareUsableAction` | `0x285c` | Wrapper returns a Boolean hardware-readiness result through an I/O Kit user-client mechanism |
| `DeckLink_Driver::firmwareHasDownloaded` | `0x3388` | Firmware callback conditionally creates audio, video, DMA and other support components, with distinct failure messages |
| `HardwareAbstractionDeckLink::isHardwareUsable` | `0xf85e` | Returns a stored flag at object offset `0x28a` |
| `HardwareAbstractionDeckLink::InitialiseHardware` | `0xf702` | Sets that readiness flag and makes PCI configuration and register writes; some indirect dispatch could not be fully recovered |
| `DeckLink_Driver::getSharedDataMemoryDescriptor` | `0x2832` | Candidate shared-memory boundary |
| `DeckLink_Driver::setVideoInputModeGated` | `0x31c8` | Candidate video-input control |
| `DeckLink_Driver::interruptOccurred` | `0x230a` | Candidate interrupt path |

Symbols were extracted from the shipped binary; described *behavior* is a Ghidra-derived reconstruction and requires validation. Exact original source code has **not** been recovered.

### Why application failure is still unresolved

Thunderbolt and PCI visibility, kernel attachment, successful hardware initialization, firmware usability, application device enumeration and receipt of actual frames are distinct milestones. The first two are directly observed. The others require tests.

The firmware callback contains explicit allocation/initialization failure branches for video DMA and video-input support. That could explain partial attachment, but we have **not** established whether any such failure branch was executed on the tested host.

## Next reproducible experiments

1. Identify every writer to readiness flag offset `0x28a`, and its state transitions.
2. Trace the I/O Kit user-client query to an independently logged result on the legacy host.
3. Compare Desktop Video Utility / Media Express enumeration with the driver-reported usable state.
4. Inspect firmware/version compatibility and driver logs **read-only**.
5. Test short input acquisition with a known, unprotected HDMI signal.
6. If capture succeeds, define a replaceable video transport/processing API; if it fails, document the precise stage and reproduce it.
7. Evaluate a wholly independent backend only after register maps, safe DMA, interrupts, timing, and firmware behavior are evidenced.

**Safety:** Do not flash firmware, write EEPROM, disable checks, modify memory-mapped registers or replace kernel extensions as a discovery shortcut. Capture work must respect source copyrights and input protection.

## Provenance policy

Publish: self-authored test harnesses, sanitized error messages, structured hardware IDs, original analytical summaries, hashes, test descriptions, and observed results.

Do not publish: Blackmagic binaries, firmware images, full decompiler dumps, credentials, third-party copyrighted code, private file paths, hardware serial numbers or personal media.

See [project introduction](../README.md) and [contribution guidance](../CONTRIBUTING.md).
