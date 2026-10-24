# Virtual Video Instrument — software-first test bench

> **Superseded by the full software video laboratory (24 October 2026).** This page describes the original one-device demonstration and its historical acceptance tests. The [new full laboratory](full-software-video-laboratory.md) runs two independent simulated devices, moves actual generated YUV/PCM packets across their virtual cable, supplies format-change and failure-recovery testing, and exports raw synthetic media. The Nikon and Blackmagic hardware are no longer required for this software development lane.

**Status (21 October 2026): implemented and regression-tested.** This is a *software simulation*, not an emulated Blackmagic driver or an electrical HDMI source. It is deliberately useful without owning additional test equipment.

## Purpose

The physical Blackmagic Intensity Extreme is half-duplex: it can capture **or** play back, not run input and output simultaneously. Tests of the actual hardware recovered device enumeration, 16 declared video modes in each direction, usable input/output activation, and successful submission of a generated output frame. Attempts to capture a real HDMI source reported no signal lock. HDMI OUT → HDMI IN loopback cannot validate capture because the hardware is half-duplex.

Instead of blocking all further Fountain Coach/Reframe video development on a physical test source, we implement the **video-instrument contract** in a replaceable virtual backend. The same contract can eventually be implemented by an actual capture host, subject to separate physical acceptance tests.

## Architecture

```text
Synthetic video source (explicit mode, connected/protected/drop state)
                        |
                        v
Virtual Intensity Extreme backend (state machine)
    capture OR playback (strict half-duplex)
    signal lock and source-mode negotiation
    YUV 4:2:2 frame buffers, timing and provenance
                        |
                        v
Portable frame metadata + frame bytes (Python standard library)
                        |
                 Preview HTTP API
                        |
                     iPad browser
```

No Blackmagic SDK, Syphon, PCI access, third-party binary, or Linux kernel module is involved in this simulation.

## Run and test

Python 3.10 or newer, **standard library only**:

```sh
python3 -m unittest discover -s simulator/tests -v
python3 -m simulator.server --host 127.0.0.1 --port 8172
```

Open `http://127.0.0.1:8172/` in the host browser.

To preview from an iPad on a **trusted local network**, intentionally bind the host's WLAN address:

```sh
python3 -m simulator.server --host YOUR_LAN_ADDRESS --port 8172
```

The preview service is intentionally unauthenticated and plain HTTP on the LAN, operates on **simulation only**, and should **not** be published to the Internet or connected to a privileged host API without adding authentication and transport security.

## Browser scenarios

- **Capture:** virtual source and selected mode match; generate YUV 4:2:2 frames with explicit nanosecond presentation timestamps.
- **Playback:** generate identical frames to the virtual output, independent of input source.
- **Disconnected cable:** source unlocks and yields zero captured frames.
- **Format mismatch:** explicit source mode differs from the selected capture format; no frames.
- **Protected input:** signal is treated as non-capturable; zero captured frames and no bypass.
- **Dropped frames:** introduce deterministic loss every third frame, preserving timestamp gaps.
- **Half-duplex conflict:** while capture is active, playback activation fails with the same observed legacy SDK code `0x80000009` (and vice versa).

All these are controlled *virtual scenario behaviors*, not claims of what a physical receiver is currently doing.

## API contract

| Route | Method | Purpose |
| --- | --- | --- |
| `/health` | GET | Health and `physical_device_access: false` |
| `/api/capabilities` | GET | 16 historical video modes, supported connector names, simulation declaration |
| `/api/state` | GET | Mode, virtual signal lock, frames delivered/dropped, latest timestamp, event log |
| `/frame.png` | GET | PNG derived from latest synthetic YUV frame (with frame counter in the first scanline) |
| `/api/configure` | POST | Select direction, video format, and fault scenario; starts a new virtual session |
| `/api/step` | POST | Advance 1–30 logical video-frame intervals |
| `/api/half-duplex` | POST | Safely attempt forbidden second direction; reports expected conflict |
| `/api/stop` | POST | Release virtual input/output |

Example: `POST /api/configure` with
```json
{"direction":"capture","mode":"720p50","source_mode":"720p50","connected":true,"protected":false,"drop_every":0}
```

The frame metadata includes `provenance: "synthetic; not captured from physical HDMI"`. All HTTP responses include `X-Fountain-Simulation: true`; the browser preview labels the image **SYNTHETIC VIDEO · NOT CAPTURED**.

## What is derived from the real device, and what is modeled

| Capability | Origin / evidence |
| --- | --- |
| 16 named input/output formats | Real device/legacy runtime enumeration |
| Half-duplex mutually exclusive modes | Confirmed in both physical activation orders |
| 8-bit YUV (2vuy) frame layout | Real vendor SDK mode tested; synthetic bytes generated here |
| Timestamps | Exact rational simulation timebase, **not** physical clock synchronization |
| Signal lock | Software state of virtual source, not the actual HDMI receiver |
| HDMI/component/composite/S-Video | Reported model capabilities; not virtually emulated at electrical level |
| 2-channel audio | Reported device attribute; **no audio samples generated yet** |
| Firmware, DMA, PCI interrupts, electrical HDCP | **Not simulated** and not validated by these tests |

The 16 formats are registered in the simulator, but only the defined 8-bit YUV generator is implemented. This prototype does not imply real interlacing, analog conversion, audio I/O, actual HDMI signaling, or physical performance certification.

## Acceptance results

On 21 October 2026, the simulator's unit and HTTP contract tests passed locally, including deterministic frame CRCs, exact timestamp arithmetic for fractional rates, source mismatch, protected/disconnected cases, deliberate frame loss, virtual playback, invalid mode/pixel rejection, and half-duplex enforcement.

Maintain a **separate physical acceptance matrix** at [hardware capability inventory](hardware-capabilities.md). Do not mark hardware capture as successful because a virtual video frame exists.

## Next milestone

Create a **typed Video Instrument Backend** interface consumed by Reframe/Teatro on iPadOS, macOS and Linux. Keep the virtual backend as the deterministic reference fixture. Later, adapt the legacy host's independently written native SDK client to the same interface; this is an application-level backend using the original Blackmagic driver, not an independent kernel driver.

No GitHub CI: tests are run locally and results recorded in Git, consistent with Fountain Coach's development policy.
