# Intensity Extreme capability inventory

Updated 2026-10-21. **Documented capabilities are not the same as functions we have recovered or tested.**

## Identified hardware

- Product: Blackmagic Design Intensity Extreme (original Thunderbolt model; **not** Intensity Shuttle, Intensity Pro 4K, or a HyperDeck).
- Live PCI identifier: `0xBDBD:0xA124`.
- Host connection: first-generation Thunderbolt, with PCI Express tunneling; live test uses THUNDERDOCK2 dock and a 2011 Mac mini.
- Host: Macmini5,3 (2011 Server), 2.0 GHz quad-core Core i7, installed 16 GB RAM, **Intel HD Graphics 3000** (live inspection: dynamically shared graphics memory, max 512 MB).
- Live OS: macOS Sierra 10.12.6; Blackmagic Desktop Video driver 10.4.
- Driver is attached in the I/O Registry; system audio inventory shows Blackmagic audio interfaces at 48 kHz.
- Blackmagic Media Express currently reports **no usable Blackmagic hardware**. No verified successful capture or playback.

## Documented input/output inventory

| Subsystem | Documented function | Live validation |
| --- | --- | --- |
| HDMI input | Standard/HD digital video, audio embedded | Not yet working |
| HDMI output | Standard/HD digital video + embedded audio | Not yet working |
| Component analog video | Y/Pb/Pr (R-Y/Y/B-Y) input and output via breakout cable, including SD and HD | Not tested |
| Composite analog video | PAL/NTSC input and output using suitable breakout RCA connectors | Not tested |
| S-Video analog video | PAL/NTSC input and output with suitable dual-RCA adapter | Not tested |
| Analog audio input | Stereo left/right, via breakout cable | Kernel audio enumeration only |
| Analog audio output | Stereo left/right, via breakout cable | Kernel audio enumeration only |
| Digital audio output | AES/SPDIF output via breakout cable (output only on the documented pinout) | Not tested |
| Audio over HDMI | Embedded digital audio in/out | Not tested |
| Real-time conversion | Hardware color-space conversion; format-dependent SD/HD upconversion and software downconversion | Not tested |
| Compressed/uncompressed recording | Captured frames supplied to host codecs/storage (not evidence of an onboard H.264 encoding engine) | Not tested |
| PCI/Thunderbolt and firmware | Device enumeration, user-client calls, firmware loading, DMA and interrupts | Basic enumeration + driver attachment only |

No SDI input/output, no general-purpose CUDA/Metal/ROCm compute GPU, and no advertised native 4K capture in this model.

## Formats documented for the original model

- PAL: 625i50; NTSC: 525i59.94.
- 720p50, 720p59.94 and 720p60.
- 1080i50, 1080i59.94 and 1080i60.
- 1080p23.98, 24, 25, 29.97 and 30.
- HDMI: 4:2:2 YUV sampling, 10-bit processing capability; audio generally 48 kHz, 24-bit.
- No advertised 1080p60 progressive capture and no 4K.
- Protected HDMI/HDCP capture is not supported; tests must use authorized unprotected signals.

## Processing chip / FPGA / AI

The Intensity Extreme is a *video capture/playback peripheral*, not a trainable graphics device. It likely uses specialized digital video logic (potentially FPGA/ASIC), but the **precise chip model, FPGA resource budget, firmware architecture, and reprogrammability have not been confirmed on this physical unit**. Do not assert it can run a general-purpose GPU workload or train LLMs. Neighboring Blackmagic products having FPGAs is not proof of this unit's model.

The *host computer* has an Intel HD Graphics 3000 GPU, confirmed in live `SPDisplaysDataType` with device ID `0x0116`. It is not a practical accelerator for modern LLM training.

**More useful AI application:** capture self-owned audiovisual training or evaluation data on the old device; process it and train models on modern hardware. Capture hardware is a source of frames and timing information, not an LLM training card.

## Acceptance sequence

1. User-client readiness + firmware status from a read-only diagnostic.
2. HDMI test-pattern capture at 720p50/60 and 1080p25/30, plus digital audio; verify timestamps, integrity and output.
3. HDMI playback to a display.
4. Component analog capture + playback (HD and SD).
5. Composite PAL/NTSC capture + playback.
6. S-Video PAL/NTSC capture + playback using appropriate adapter.
7. Analog stereo capture + playback; digital audio output.
8. Document format negotiation, color fidelity, frame-drop rate, audio/video synchronization and sustained storage throughput.
9. Expose tested capabilities through a replaceable, platform-independent video instrument backend. **No Syphon dependency.**
10. Independently characterize the specialized processing silicon and evaluate hardware backend replacement feasibility. No flash or EEPROM writes as part of discovery.

## Source distinctions

- The device's historical connections are documented in the [Blackmagic Desktop Video Operation Manual (2012)](https://manualzz.com/doc/8803676/blackmagic-design-desktop-video-operation-manual), especially the **Intensity Extreme connection reference**. Some settings in the general manual apply to other Blackmagic products and must not be generalized to this model.
- Original-model technical specifications are preserved in [the legacy Intensity Extreme product listing](https://www.expandore.com/blackmagic/blackmagic-intensity-extreme.htm).
- Host graphics specifications: [Apple Mac mini Server (Mid 2011)](https://support.apple.com/en-mo/112006).
- The live observations and reverse-engineering results are in [investigation.md](investigation.md).

This is a public reproducibility inventory; it contains no proprietary driver binaries, private serial numbers or decompilation dumps.
