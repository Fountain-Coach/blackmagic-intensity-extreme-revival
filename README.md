# Blackmagic Intensity Extreme Revival

**Software repair is hardware sustainability.**

A video-capture device does not stop being useful simply because its manufacturer stops supporting the software around it. This independent Fountain Coach project investigates whether a legacy **Blackmagic Intensity Extreme** can be brought back into service with documented diagnostics, reproducible reverse engineering, and eventually an openly implemented hardware interface.

Inspired by the practical spirit of a *Repair Café*: understand what is broken, share what we learn, and make functioning equipment useful for longer.

> **Current status — native capture/playback APIs recovered; HDMI receiver not locked.** Our self-authored programs enumerate the device, negotiate input and output modes, and submit a generated playback frame through the installed legacy driver. With a Mac mini or MacBook HDMI source connected, capture can start, but the driver reports **no input signal lock** at 1080p30 and delivers no frames. A temporary 720p60 source-mode test likewise produced no frames. With Blackmagic HDMI OUT physically looped into HDMI IN, native tests established **half-duplex behavior**: whichever capture/playback mode was enabled second was rejected. Physical signal transfer and an independent replacement driver remain unverified.

## Try the software simulation — no additional hardware required

The [Fountain Virtual Video Instrument](docs/virtual-video-instrument.md) models the original device's video modes, deterministic YUV 4:2:2 frames, frame timing, source loss, format mismatch, protected input and strict half-duplex operation. It is a **replaceable software backend**, not a claim of a repaired HDMI receiver. The iPad-friendly browser preview is served by a small Python HTTP process on a trusted local host.

```sh
python3 -m unittest discover -s simulator/tests -v
python3 -m simulator.server --host 127.0.0.1 --port 8172
```

Open `http://127.0.0.1:8172/` on the host. See the [simulator design and API](docs/virtual-video-instrument.md) for trusted LAN access. **No Syphon, Blackmagic SDK or third-party Python dependency** is required. Synthetic frames are visibly marked, and real hardware validation remains separate.

## Legacy driver failure affecting other macOS applications

Our tests exposed a reproducible legacy-Desktop-Video integration defect: opening **App Store 2.2.1 on Sierra** crashes inside Blackmagic's system-wide CoreMediaIO device-discovery plugin and DeckLinkAPI. Blackmagic's hardware helper also crashes at nearly the same time. We have [documented the four matching crash stacks](tests/2026-10-21-app-store-blackmagic-crash.md), the limits of current evidence, and a reversible plugin-isolation experiment. **The App Store startup crash is now mitigated by an administrator-approved, reversible isolation of the old CoreMediaIO plugin; the proprietary bug itself has not been patched.**

## What we want to repair

The tested setup includes:
- **Blackmagic Intensity Extreme**, PCI vendor **0xBDBD**, device **0xA124**.
- **Comdis/i-tec THUNDERDOCK2** between the capture device and host.
- **2011 Mac mini**, macOS **10.12.6 Sierra**, with **Desktop Video 10.4** installed.

The device enumerates over Thunderbolt and PCI Express. macOS attaches the Blackmagic driver, including audio-related components. **Our independently written native test can now enumerate the device, discover video formats, and temporarily enable/disable input and output modes.** Media Express's contradictory failure is an application-level symptom to investigate, not proof of broken hardware.

## The repair approach

1. **Observe before changing anything.** Record model IDs, software versions, connection topology, failures, and exact artifact hashes.
2. **Understand the working boundaries.** Separate PCI enumeration, driver attachment, firmware readiness, device usability, video DMA, and actual capture.
3. **Reconstruct behavior from evidence.** Analyze an authorized local copy of the legacy kernel extension using [Reverse Engineer Anything (REA)](https://github.com/morluto/rea) and [Ghidra](https://github.com/NationalSecurityAgency/ghidra).
4. **Implement a minimal diagnostic.** First determine whether a supported user client can report device, firmware and readiness state without modifying hardware.
5. **Reclaim the useful interface.** Build a portable video instrument with replaceable host backends. Examine independent driver implementation as a separate, testable objective.
6. **Measure what works.** Reproduce successful frames, timestamps, audio synchronization, and controlled output—rather than promising compatibility by assertion.

**No Syphon dependency.** The forward architecture is intended for current macOS, Linux, and iPadOS consumers, although a low-level Thunderbolt/PCI host is still needed for this particular device.

## Evidence so far

| Layer | Observed result |
| --- | --- |
| Thunderbolt and PCI Express | Enumerated; vendor `0xBDBD`, device `0xA124` |
| macOS kernel driver | DeckLink driver attached and audio engines registered |
| Vendor capture application | Media Express reports no usable Blackmagic hardware |
| Binary inspection | 64-bit Intel Mach-O kernel extension with readable C++ symbols |
| REA + Ghidra | First analysis successful, including readiness and firmware callbacks |
| Native DeckLink program | Device enumerated as Intensity Extreme; both video interfaces and 16 modes each discovered |
| Physical connectors | HDMI, component, composite, S-Video masks returned for input and output |
| Video mode activation | 1080p25, 8-bit YUV input and 1080p25 output enabled and disabled successfully |
| Generated output frame submission | 1920×1080 8-bit YUV frame created and accepted by synchronous display API; physical sink not verified |
| HDMI self-loop signal | Mac mini display identifies BMD HDMI at 1080p30; legacy configuration selects HDMI input (`0x2`) |
| HDMI receiver signal lock | **False during active 1080p30 capture** with an external computer source |
| Simultaneous capture and playback | **Rejected** whichever direction was enabled second (half-duplex); generated HDMI self-loop cannot function |
| Input stream callback | 1080p25, 1080p30 and temporary 720p60 capture tests: no frames received |
| Input frame capture / physical output verification | **Not yet demonstrated** |
| Independent driver | **Not implemented** |

The [confirmed half-duplex loopback result](tests/2026-10-21-half-duplex-loopback.md) supersedes the [earlier proposed MacBook HDMI self-loop](tests/2026-10-21-macbook-hdmi-source.md). The earlier report documents the MacBook HDMI source and the now-disproven self-loop hypothesis. The [hardware capability inventory](docs/hardware-capabilities.md) tracks every documented video, audio and conversion function. The [native enumeration probe](src/decklink-enumerate.cpp), [single-frame output experiment](src/decklink-output-frame.cpp), and [bounded input observer](src/decklink-input-observer.cpp) demonstrate the progressively recovered interface. The [dated enumeration](tests/2026-10-21-native-enumeration.md), [output](tests/2026-10-21-single-frame-output.md), [input](tests/2026-10-21-input-callback.md), and [HDMI loopback diagnostics](tests/2026-10-21-hdmi-self-loop.md) logs separate successful API calls from unverified physical signals. The [investigation log](docs/investigation.md) records the reverse-engineering evidence. The complete copyrighted vendor binaries, firmware and their decompiler output are **not distributed here**.

## Why this matters

Keeping functional equipment in use may reduce electronic waste and the embodied impacts of purchasing replacement devices. But *carbon savings are not assumed*. An old computer also consumes power, and reverse-engineering activity has a cost. Our [sustainability ledger](docs/sustainability.md) explains which data would justify a real claim.

We want this to be both an engineering project **and a practical public example of software-led repair**—a place where a repair volunteer, artist, researcher or systems architect can follow the evidence and try the next experiment.

## Contribute

Owners of similar devices can contribute sanitized hardware inventories, driver-version observations, error messages, independent technical notes and reproducible tests. See [CONTRIBUTING.md](CONTRIBUTING.md). Please **do not upload proprietary drivers, firmware images, private user data or copied vendor source**.

This is an independent community repair initiative, **not affiliated with or endorsed by Blackmagic Design**. All respective trademarks belong to their owners.

The original materials in this repository are MIT licensed (see [LICENSE](LICENSE)). Proprietary third-party software and hardware are not relicensed by this project.

**Fountain Coach** — *We make meaning executable.* Here, that includes making the meaning of repair demonstrable.
