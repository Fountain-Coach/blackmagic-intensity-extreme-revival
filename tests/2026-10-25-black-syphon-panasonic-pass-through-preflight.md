# Black Syphon input publication + physical HDMI monitoring — fixed-rig witness

**Session:** 25 October 2026 (Europe/Berlin)  
**Host:** original silver Mac mini (Macmini5,3), macOS Sierra 10.12.6, Desktop Video 10.4, original Intensity Extreme.  
**Acceptance status:** **NOT YET ACCEPTED** — Black Syphon opened, source enumerated, but input not activated; no Syphon server; receiver lock 0/8; Panasonic optical output not yet witnessed.

## Physical circuit — preserve the input while testing the output

- Keep **silver Mac HDMI OUT → original Intensity Extreme HDMI IN**.
- Keep **silver Mac Thunderbolt → original Intensity Extreme** unchanged.
- Connect **original Intensity Extreme HDMI OUT → Panasonic HDMI IN**, select that input on the Panasonic. This physical connection and the Panasonic's displayed picture have **not** been remotely witnessed yet.
- Do not confuse the Blackmagic HDMI OUT with the silver Mac's HDMI OUT or another dock. No extra cable swap is needed.
- Do not select Black Syphon's separate **Potential Outputs** for this first capture/monitor test.

## Why Black Syphon input and monitoring may coexist

Per [the VIDVOX Black Syphon r4 guide](https://docs.vidvox.net/blacksyphon/), an activated Blackmagic input can publish the received image to Syphon, and certain Blackmagic input/output pairs allow a **hardware monitoring/pass-through** to their physical output while capturing. This pass-through is *not* simultaneous `IDeckLinkInput` and separately commanded `IDeckLinkOutput` software modes. It is device-dependent; the Intensity Extreme physical pass-through has not yet been independently accepted in this exact rig.

The existing [half-duplex test](../tests/2026-10-21-half-duplex-loopback.md) found that enabling two independent DeckLink input/output API modes concurrently fails with `0x80000009` in either order. Do **not** repeat that conflicting two-process test. A physical pass-through mode is a separate capability to verify rather than assume.

## What was actually executed on Sierra

1. Inspected the physical silver Mac: macOS still reports **BMD HDMI 1920×1080 @ 30 Hz**, online, `cyuv=automatic` and `cmod=YCbCr444`. The original driver remains intact.
2. Verified `/Applications/Black Syphon.app` (bundle ID `com.Vidvox.Black-Syphon`, historical VIDVOX r4) and `/Users/bene/Desktop/ReceiveImages.vuo` still exist. No other Fountain Video/Media Express capture client was running.
3. Launched Black Syphon through the existing user Aqua session using ordinary `open -a`; process was running without startup error.
4. Inspected the actual Black Syphon **window** through CoreGraphics, screenshot of only that window (temporary local diagnostic, **not published**): **Intensity Extreme** appears under **Potential Inputs** and separately **Potential Outputs**, but **no item is selected/activated**; both options areas are empty and both previews are black. The user session has an actual application window. In particular, merely drawing the Black Syphon window does not start capture.
5. Compiled and executed [read-only Syphon server-directory witness](../tools/sierra/syphon-server-witness.m) under original Sierra Clang. Result:
```text
stage=legacy_syphon result=enumerated server_count=0 frame_pixels_verified=0 hardware_control=0
```
6. After Black Syphon opened but before its input was activated, ran the source-owned bounded, **no-StartStreams** receiver query:
```text
stage=source result=BMD_HDMI active_width=1920 active_height=1080 selected_rate_hz=30.000000 active_mode_match=1
stage=input_connector rc=0x00000000 value=0x2 expected=0x2
stage=capability input_format_detection rc=0x00000000 supported=0
stage=enable_input mode=1080p30 pixel_buffer=2vuy rc=0x00000000
stage=receiver result=NO_LOCK locked_samples=0 successful_lock_queries=8 input_format_detection_supported=0 detected_mode_not_required=1 real_frame_acceptance=NOT_TESTED
stage=disable_input rc=0x00000000
stage=complete exit=32 callback_installed=0 stream_started=0 captured_frames=0 display_mode_changes=0 driver_firmware_changes=0
```
The no-stream probe does **not** prove whether physical hardware pass-through would be enabled by a fully running capture stream.

## Remaining single user-interface handoff

The authorized remote shell is **not** trusted for macOS Accessibility (`AXIsProcessTrusted=False`). An ordinary AppleScript request to inspect/operate the Black Syphon process's controls returns `-1728` (no Accessibility permission). We have **not** changed macOS security permissions or attempted an alternate injection route.

The first genuine capture/pass-through experiment needs the operator, in the actual **Black Syphon** GUI, to:
1. Select **Intensity Extreme** in the **top-left Potential Inputs** list only.
2. Select **Mode 1080p30** in **Input Options**. Do **not** enable unsupported auto-detection; this physical card returned `input_format_detection_supported=0`.
3. Inspect **Input Preview** for changing actual imagery.
4. Inspect the **Panasonic** on the Blackmagic HDMI OUT connection. Does it show the incoming Sierra image, remain black, report no signal, or switch resolution? Note **exact** status.
5. Keep **Potential Outputs** deselected, since it represents a separate Blackmagic output stream, not hardware pass-through.

Once that input is actually activated, rerun the **read-only** `/tmp/fc-syphon-server-witness` in a separate terminal to check for a Syphon server named `Intensity Extreme`. A server listing alone does not establish real image pixels; a changing valid Syphon frame, an Input Preview and an observed physical Panasonic picture remain separate acceptance gates.

**Do not run** a separate DeckLink input-status client while Black Syphon owns the device, re-enable unsafe legacy frame lifetime capture, or simultaneously enable a separate Blackmagic output stream.

## Explicit acceptance matrix

| Boundary | Result |
| --- | --- |
| Silver Mac → Blackmagic HDMI IN cable, BMD EDID 1080p30 | PASS |
| Black Syphon r4 installed and launched | PASS |
| Original Intensity Extreme enumerated as potential input/output | PASS |
| Black Syphon input selected and active | **NOT YET** |
| Syphon `Intensity Extreme` server published | **NO — 0 servers** |
| Native no-stream receiver input lock | **FAIL — 0/8** |
| Device-specific HDMI IN → HDMI OUT physical pass-through | **NOT TESTED** |
| Panasonic HDMI image visually observed | **NOT TESTED** |
| Changing valid Syphon images passed downstream | **NOT TESTED** |
| Original Vuo 2.4.1 composition executed on Sierra | **NOT TESTED** |

**Next:** physical Panasonic cable/input observation and a single authorised Black Syphon GUI selection. The current app is already open. No new driver, firmware, operating system or hardware bridge is needed for this preliminary test. If native signal lock remains false once input is active, investigate source-to-receiver format/physical compatibility; do not infer functioning capture from server publication or software status alone.

**Hardware acceptance D4 remains OPEN.**
