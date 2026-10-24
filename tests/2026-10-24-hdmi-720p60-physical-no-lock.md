# Physical 720p60 HDMI input-lock trial — no lock, safe return

**Date:** 24 October 2026, Berlin time. **Host:** silver Apple Macmini5,3, macOS Sierra 10.12.6, original Blackmagic Desktop Video 10.4. **Device:** original Thunderbolt Intensity Extreme, identified via DeckLink as `Intensity Extreme`, PCI `0xBDBD:0xA124`.

**Result:** physical **720p60 source selection PASS**; **Blackmagic input lock FAIL (0/8)**; return to original **1080p30 PASS**. This is **not** a physical capture acceptance. No recording was attempted.

## Physical setup

Silver Mac HDMI OUT → original Intensity Extreme HDMI IN; original Intensity Extreme HDMI OUT empty. Original Thunderbolt remains connected. Nothing was unplugged, replugged or rerouted, and no second HDMI source was substituted.

## Supported 60 Hz mode — corroborated twice

The Sierra CoreGraphics BMD HDMI sink advertised `1280×720 @ 60.0 Hz`. Separately, the original 10.4 **DeckLink INPUT mode iterator on the actual device** reported:

```text
model=Intensity Extreme
stage=mode interface=input code=0x68703630 width=1280 height=720
rate=60000/1000 rate_status=0x00000000 name=720p60
```

The same device enumerated `720p50`, `720p59.94`, and `720p60`. Its original-model capabilities do **not** include progressive 1080p60; the host's ability to select 1080p60 as an HDMI *display mode* must not be confused with supported *capture* input.

## Source-owned diagnostics and independent restoration

- [No-stream input lock probe](../src/decklink-input-lock-probe.cpp) now accepts `--preflight-720p60` and `--probe-active-720p60`, both verifying `1280×720@60` against the actual BMD HDMI display. Before the physical switch, it correctly rejected a 720p60 capture-mode request while the selected source was still 1080p30: `BLOCKED_wrong_current_rate vendor_calls=0`.
- [Physical bounded controller](../tools/sierra/hdmi-720p60-bounded-trial.py) starts an **independent Sierra process watchdog** (13-second upper-bound lease), waits until armed, uses public CoreGraphics to select 720p60, samples source timing, runs the existing no-stream DeckLink status probe, then restores the original 1080p30. The watchdog independently verifies restoration and is capable of attempting restoration if the controller dies/exceeds its lease.
- A **second, external fallback** was armed on the separate authorized modern Mac before the trial: after 18 seconds it connected over SSH to the silver Mac and executed the bounded restore-only command, verifying the original 1080p30 display. This was an independent host/SSH recovery path, not a firmware/driver change.
- The guard was **not** deliberately fault-injected while the physical screen was at 720p60. This trial proves a normal real source mode transition back to 1080p30 and the guard's final verification; it does not prove every watchdog failure path under a GPU/kernel hang.
- The original Mac has not gained an independent boot-tested rescue drive or local physical display. Preserve existing no-go for driver/firmware swaps and crash-prone live video streaming.

## Observed physical source mode

```text
stage=physical_720p60 watchdog=armed original=1080p30 candidate=720p60
stage=physical_720p60 mode_change_rc=0
stage=physical_720p60 active_720p60_confirmed=True
```

The physically connected silver Mac's active BMD HDMI display became **1280 × 720 @ 60.0 Hz** according to Quartz. Its active Intel framebuffer registry at that moment reported:

| Source-side property | Reported physical value |
| --- | ---: |
| `IOFBCurrentPixelClock` | **73,979,530 Hz** |
| `IOFBCurrentPixelCount` | **1,237,500 pixels** |
| Derived macOS source timing | **59.781438 Hz** |
| Mac's selected UI mode | **60.000000 Hz** |

The standard 720p60 timing is 1650 × 750 = **1,237,500 total pixels** at a nominal **74.25 MHz** pixel clock. The GPU's reported 73.97953 MHz is **-0.364269%** below 74.25 MHz.

Crucially, this is **the same 73,979,530 Hz registry pixel clock** previously observed for 1080p30, when the framebuffer used **2,475,000 total pixels**, yielding 29.890719 Hz. The two source-mode calculations are consistent with one anomalous macOS graphics clock. This is **not** a direct measurement of the HDMI wire clock, source YCbCr/RGB colour encoding or receiver PLL state, and cannot independently establish causality.

## Receiver input-lock test under real 720p60 source

The new `--probe-active-720p60` path confirmed the **source was already in 720p60** before touching the Blackmagic vendor runtime:

```text
stage=source result=BMD_HDMI active_width=1280 active_height=720
selected_rate_hz=60.000000 required_width=1280
required_height=720 required_rate_hz=60 mode_advertised=1 active_mode_match=1
stage=input_connector rc=0x00000000 value=0x2 expected=0x2
stage=enable_input mode=720p60 pixel_buffer=2vuy rc=0x00000000
stage=receiver sample=0 lock_rc=0x00000000 locked=NO detected_rc=0x80000001 detected_mode=0x0
stage=receiver sample=1 lock_rc=0x00000000 locked=NO detected_rc=0x80000001 detected_mode=0x0
stage=receiver sample=2 lock_rc=0x00000000 locked=NO detected_rc=0x80000001 detected_mode=0x0
stage=receiver sample=3 lock_rc=0x00000000 locked=NO detected_rc=0x80000001 detected_mode=0x0
stage=receiver sample=4 lock_rc=0x00000000 locked=NO detected_rc=0x80000001 detected_mode=0x0
stage=receiver sample=5 lock_rc=0x00000000 locked=NO detected_rc=0x80000001 detected_mode=0x0
stage=receiver sample=6 lock_rc=0x00000000 locked=NO detected_rc=0x80000001 detected_mode=0x0
stage=receiver sample=7 lock_rc=0x00000000 locked=NO detected_rc=0x80000001 detected_mode=0x0
stage=receiver result=NO_LOCK locked_samples=0 correct_detected_mode_samples=0 real_frame_acceptance=NOT_TESTED
stage=disable_input rc=0x00000000
stage=complete exit=32 callback_installed=0 stream_started=0 captured_frames=0
```

The Blackmagic device **accepted input-mode configuration**, but **never reported signal lock or detected the video format** over eight sequential samples. Exit 32 is the source-owned `NO_LOCK` result, not a hardware crash. The `2vuy` argument is the requested **capture buffer format**, not evidence of the source's actual HDMI wire encoding.

## Return-to-original verification

The controller's return reported `VERIFIED_ORIGINAL`, the independent local watchdog wrote `result=PASS trigger=controller_done restoration=VERIFIED_ORIGINAL`, and the external delayed restore fallback independently returned `VERIFIED_ORIGINAL` with the current mode **1920×1080@30**. System Profiler after the physical trial again showed `BMD HDMI: Resolution: 1920 x 1080 @ 30 Hz, Online: Yes`.

No `StartStreams`, video callbacks, captured pixels, driver/firmware installation, PCI register reads or reboot occurred.

The machine-readable [physical 720p60 result](../fixtures/silver-hdmi-720p60-no-lock-2026-10-24.json) is committed for future comparisons.

## Verdict and next diagnostic

1. **The user's 60 Hz hypothesis was exercised physically.** Changing the silver Mac's HDMI output to the supported 720p60 format **did not recover Blackmagic input lock**. Neither a nominal 30 Hz nor a nominal 60 Hz source mode has passed the receiver.
2. Source timing is a now independently repeated problem candidate: **both** macOS 1080p30 and 720p60 modes use the same reported 73.97953 MHz source pixel clock rather than the nominal 74.25 MHz.
3. We still cannot infer that this clock discrepancy **caused** loss of lock; other unresolved candidates include actual RGB vs YCbCr wire encoding, HDMI signal/protection, receiver/driver readiness or an electrical fault.
4. **Next investigation should be source-clock/colour-encoding evidence through read-only graphics driver research**, then design a nonpersistent reference-timing intervention. Do not blindly install a graphics driver, force EDID overrides, substitute hardware sources or restart the crash-prone DeckLink capture stream.
5. D4 remains **OPEN**: hardware frame capture requires true lock, detected input mode, valid non-no-source frame bytes and timestamps, and crash-free teardown.

**Physical result: 720p60 NO LOCK; source returned to 1080p30.**
