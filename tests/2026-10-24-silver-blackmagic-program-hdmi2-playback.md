# 2026-10-24 — Silver Intensity Extreme HDMI OUT to Samsung HDMI 2

## Physically user-selected circuit

The operator confirmed original **Intensity Extreme HDMI OUT now connects to Samsung HDMI 2**. The silver Mac's working desktop remains via **i-tec THUNDERDOCK2 HDMI** on the Samsung's other HDMI input. Original Blackmagic HDMI IN from silver Mac native HDMI has been disconnected on operator instruction; the Blackmagic is used **as playback**, not HDMI pass-through or desktop capture. The original Macmini5,3 remains on Sierra 10.12.6 and its original Desktop Video 10.4 Thunderbolt/PCI driver. No driver, firmware, OS, Thunderbolt connection or host display mode was changed by this test.

**Device confirmed live:** PCI `BDBD:A124`, Thunderbolt@199,0,0, original driver 10.4 loaded; silver's ordinary Samsung i-tec video display remained detected. No conflicting Fountain Video / Black Syphon / Media Express capture application process was running before output.

## New software output probe

Source-owned `src/decklink-silver-program-output-hdmi2.cpp` builds on the existing `src/decklink-hdmi-steady-output.cpp` but **does not require or touch the previously fixed Mac HDMI IN source/EDID fixture**. It requires the original silver model and an active 1920x1080@30 display, then uses original DeckLinkAPI 10.4 `IDeckLinkOutput` to enable exact **1080p30 UYVY**, create and paint an original vendor-owned frame, and submit repeated color-bar frames with a moving white marker. Vendor current video output mode `cvom` is polled. The program disables video output before exit or after receiving SIGTERM. A distinct generated test pattern is used; no desktop, application, protected content, real HDMI input or network pixels are accessed.

The test **cannot independently measure the Samsung's physical image** and must not claim display optical acceptance without the operator's specific observation.

## Test run 1 — completed original hardware digital output

Executed directly on original silver Sierra host at ~19:41 CEST:

```text
stage=program_hdmi2 preflight=PASS
stage=steady_output_mode rc=0x00000000 supported=1
stage=steady_output_enable rc=0x00000000 target=1080p30
stage=steady_output_status elapsed_s=59 ... mode_is_requested=1
stage=steady_output_disable rc=0x00000000
stage=steady_output_complete result=API_STABLE vendor_frames=1425
    api_errors=0 consecutive_matching_mode_seconds=59
    output_disabled_after_test=1 monitor_optical_pixels_unmeasured=1
stage=program_hdmi2 exit=0
```

**Interpretation:** genuine device/vendor API playback at the requested 1080p30 mode is stable over one minute; the software signal producer and Blackmagic driver accepted 1,425 submissions. This **does not establish that Samsung HDMI 2 was visibly displaying those pixels**.

## Test run 2 — extended visible test started

The same source gained bounded `--program-hdmi2-300s` (5-minute) mode. Recompiled on original silver and started via `nohup` with owner-only log. Verified the process was running and `EnableVideoOutput(1080p30)` returned success with `cvom=Hp30`. Source-owned binary:

- `/Volumes/Fresh/FountainVideoDockerLab/bin/fc-decklink-program-hdmi2`
- Owner-only runtime log: `/Volumes/Fresh/FountainVideoDockerLab/private-roundtrip/hdmi2-visible-bars-20261024-5min.log`
- PID file: `/Volumes/Fresh/FountainVideoDockerLab/private-roundtrip/hdmi2-visible-bars-20261024.pid`
- Process observed running under PID **2760** at start.

**As of this evidence entry the extended run was started, not yet certified completed**; its digital result should be read from the private log after it terminates. Output disables automatically at end. Actual monitor optical result awaits the operator switching Samsung source to HDMI 2 and reporting the display.

## Acceptance boundary

**ACCEPTED** one-minute real Blackmagic driver HDMI OUT play/disable and new five-minute live test activation. **OPEN:** visual pixels on Samsung HDMI 2, independent wire/pixel capture, unattended application/Syphon program feed. The original HDMI input source loss/no-lock is irrelevant to testing an HDMI output-only path. PR #10 remains **DRAFT**.
