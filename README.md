# Blackmagic Intensity Extreme Revival

**Software repair is hardware sustainability.**

A video-capture device does not stop being useful simply because its manufacturer stops supporting the software around it. This independent Fountain Coach project investigates whether a legacy **Blackmagic Intensity Extreme** can be brought back into service with documented diagnostics, reproducible reverse engineering, and eventually an openly implemented hardware interface.

Inspired by the practical spirit of a *Repair Café*: understand what is broken, share what we learn, and make functioning equipment useful for longer.

> **Current status — research, not a working replacement driver.** The original capture hardware is visible over Thunderbolt/PCI Express and its legacy macOS kernel driver attaches. Blackmagic Media Express nevertheless reports that it cannot find usable hardware. We have completed the first verified REA + Ghidra analysis of the driver, but we have **not** demonstrated successful video capture or an independent driver.

## What we want to repair

The tested setup includes:
- **Blackmagic Intensity Extreme**, PCI vendor **0xBDBD**, device **0xA124**.
- **Comdis/i-tec THUNDERDOCK2** between the capture device and host.
- **2011 Mac mini**, macOS **10.12.6 Sierra**, with **Desktop Video 10.4** installed.

The device enumerates over Thunderbolt and PCI Express. macOS attaches the Blackmagic driver, including audio-related components, yet Media Express cannot enumerate a usable capture device. This is the software boundary we're investigating, not proof of broken hardware.

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
| Video capture | **Not yet demonstrated** |
| Independent driver | **Not implemented** |

Selected analytical findings are in [the investigation log](docs/investigation.md). The complete copyrighted vendor binaries, firmware and their decompiler output are **not distributed here**.

## Why this matters

Keeping functional equipment in use may reduce electronic waste and the embodied impacts of purchasing replacement devices. But *carbon savings are not assumed*. An old computer also consumes power, and reverse-engineering activity has a cost. Our [sustainability ledger](docs/sustainability.md) explains which data would justify a real claim.

We want this to be both an engineering project **and a practical public example of software-led repair**—a place where a repair volunteer, artist, researcher or systems architect can follow the evidence and try the next experiment.

## Contribute

Owners of similar devices can contribute sanitized hardware inventories, driver-version observations, error messages, independent technical notes and reproducible tests. See [CONTRIBUTING.md](CONTRIBUTING.md). Please **do not upload proprietary drivers, firmware images, private user data or copied vendor source**.

This is an independent community repair initiative, **not affiliated with or endorsed by Blackmagic Design**. All respective trademarks belong to their owners.

The original materials in this repository are MIT licensed (see [LICENSE](LICENSE)). Proprietary third-party software and hardware are not relicensed by this project.

**Fountain Coach** — *We make meaning executable.* Here, that includes making the meaning of repair demonstrable.
