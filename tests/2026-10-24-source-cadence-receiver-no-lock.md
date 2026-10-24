# Silver Mac → Intensity Extreme: source cadence and receiver-lock investigation

**Witness date:** 24 October 2026 (Europe/Berlin), physical silver 2011 Mac mini (Macmini5,3), macOS Sierra 10.12.6, original Blackmagic Desktop Video 10.4.
**Physical verdict:** **NOT ACCEPTED** — no receiver input lock or genuine captured pixels. This report is a source-side timing observation and a safe, no-stream receiver cross-check; not an HDMI wire measurement.

## 1. Fixed physical topology — do not alter for these tests

Silver Mac mini HDMI OUT → original Intensity Extreme HDMI IN. Intensity Extreme HDMI OUT is **disconnected**, and the original Thunderbolt dock/device path is left attached. The same Sierra machine owns both the source display and Blackmagic host interface; this is **not** a capture-card HDMI-output self-loop. The physical owner confirmed the cabling.

The visible macOS display returns to **BMD HDMI** after the downstream monitor is disconnected:

- `system_profiler SPDisplaysDataType`: BMD HDMI; 1920 × 1080 @ 30 Hz; online; main display; television.
- I/O Registry sink: DisplayVendorID `8961`, DisplayProductID `0`.
- Original Blackmagic 10.4 kernel driver and framebuffer driver remain loaded.
- The 256-byte BMD EDID checksum passes in both 128-byte blocks. CTA revision 3 advertises video identification code **34 (1080p30)**, as well as 32/33 (1080p24/p25). CTA flags `0x77` advertise YCbCr 4:2:2 and 4:4:4 acceptance.
- **Advertising** 4:2:2 via EDID does not prove the Mac transmits 4:2:2; the 32-bit ARGB8888 OS framebuffer also does not establish the HDMI **wire** colour encoding.

## 2. Source-side timing evidence

The GUI-selected `CGDisplayModeGetRefreshRate` reports exactly **30.000000 Hz**. Our Git-owned [source clock witness](../tools/sierra/hdmi-output-clock-witness.py) independently maps `AppleIntelFramebuffer` OpenGL index **1** to this specific BMD display using `CGOpenGLDisplayMaskToDisplayID`. It reads, without kernel or register access:

| I/O Registry display property | Physical value |
| --- | ---: |
| `IOFBCurrentPixelClock` | 73,979,530 Hz |
| `IOFBCurrentPixelCount` | 2,475,000 |
| `IOFBCurrentPixelCountReal` | 2,475,000 |
| Derived refresh (`clock/count`) | **29.890719 Hz** |
| Difference from 30 Hz | **-0.364269%** |
| Difference from 30000/1001 Hz | **-0.264634%** |

For comparison, a nominal 1080p30 frame of 2200 × 1125 total pixels requires **74,250,000 Hz** pixel clock. The registry number differs by 270,470 Hz. A registry counter is not by itself an oscilloscope reading of an HDMI wire.

A separate Git-owned [CoreVideo display-link witness](../tools/sierra/hdmi-displaylink-clock-witness.c) was compiled under the real Sierra toolchain with `-std=c11 -Wall -Wextra -Werror` and linked only against CoreGraphics and CoreVideo. It registered a **timestamp-only** callback with the BMD display, not a video recorder. It did not read screen pixels or invoke the DeckLink runtime.

Three physical samples returned:

| Sample | `CVDisplayLinkGetNominalOutputVideoRefreshPeriod` | Actual system-time refresh estimate | Median callback cadence |
| --- | ---: | ---: | ---: |
| Initial | 29.890719 Hz | 29.890777 Hz | 29.890719 Hz |
| Repeat A | 29.890719 Hz | 29.891119 Hz | 29.890710 Hz |
| Repeat B | 29.890719 Hz | 29.890708 Hz | 29.890720 Hz |

Each sample used 120 display-link callbacks and 119 intervals, for a median period of about **33.4552 ms**. Apple's API describes the actual estimate as computed relative to host system time: [CoreVideo documentation](https://developer.apple.com/documentation/corevideo/cvdisplaylinkgetactualoutputvideorefreshperiod%28_%3A%29).

**Interpretation:** the Mac's display scheduler consistently operates near 29.8907 Hz while the selected macOS mode is named 30 Hz. The mismatch is observable through more than one operating-system interface. These observations do **not** independently measure the HDMI transmitted electrical pixel clock, RGB/YCbCr encoding, HDCP, or cable integrity.

## 3. Receiver status on this exact setup

Immediately after the repeated display-link runs, we invoked the existing bounded `/tmp/fc-input-armed-post-reboot --armed-1080p30-no-stream` on the Sierra host.

```text
stage=input rc=0x00000000 present=1
stage=status rc=0x00000000 present=1
stage=enable_video_input rc=0x00000000 requested=1080p30 pixel_format=2vuy
stage=mode_armed_status step=0 lock_rc=0x00000000 lock=no detected_rc=0x80000001 detected_mode=0x0
stage=mode_armed_status step=1 lock_rc=0x00000000 lock=no detected_rc=0x80000001 detected_mode=0x0
stage=mode_armed_status step=2 lock_rc=0x00000000 lock=no detected_rc=0x80000001 detected_mode=0x0
stage=mode_armed_status step=3 lock_rc=0x00000000 lock=no detected_rc=0x80000001 detected_mode=0x0
stage=disable_video_input rc=0x00000000
stage=complete exit=0 stream_started=0 physical_frame_captured=0
```

Post-test macOS display inventory still identified **BMD HDMI at 1920 × 1080 @ 30 Hz**. The `2vuy` argument is the Blackmagic capture **buffer** format requested from the driver, not proof the HDMI source is emitting YCbCr 4:2:2.

## 4. Reproducible source-owned checks

```sh
# On the physical Sierra Mac:
python tools/sierra/hdmi-output-clock-witness.py
clang -std=c11 -Wall -Wextra -Werror tools/sierra/hdmi-displaylink-clock-witness.c \
  -framework CoreGraphics -framework CoreVideo -o /tmp/fc-hdmi-displaylink-clock
/tmp/fc-hdmi-displaylink-clock

# Pure calculation tests; passed 7/7 on Sierra Python 2.7 and modern macOS Python 3:
python tests/test_hdmi_output_clock_witness_math.py
```

Safety contracts: no monitor display-mode changes, external cable changes, screenshot pixels, screen recordings, DeckLink frame callbacks or stream start, firmware update, PCI memory mapping, independent KEXT load or modification of Sierra. Only the bounded receiver status probe momentarily enabled and disabled 1080p30 capture **mode**.

The owner's earlier report that the original diagnostic once obtained stable input lock is a legitimate historical datum, but the published source repository has not yet yielded an independently preserved, positive **physical** lock log. Do not misclassify synthetic fixture cases or a success return from `StartStreams` as physical lock.

## 5. Technical hypotheses, deliberately unproven

1. **Source clock mismatch:** 29.8907 Hz may be rejected by the capture receiver or indicate incorrect PLL programming/EDID interpretation by the Sierra display driver. This is the most actionable newly measured discrepancy. No hardware timing tolerance has been established.
2. **Source HDMI colour encoding:** the original Intensity Extreme lists YUV 4:2:2 video. The macOS GUI's ARGB framebuffer and the sink's 4:2:2 EDID capability reveal neither the actual RGB/YCbCr HDMI format nor whether the source selected an unsupported format. [Original model technical specifications](https://www.bhphotovideo.com/c/product/824118-REG/Blackmagic_Design_BINTSTBEXT_Intensity_Extreme_HDMI_and.html).
3. **Driver/firmware/receiver:** the original 10.4 driver can enumerate and configure the card but has a separately documented frame-lifetime crash; the receiver and actual firmware readiness are unverified. No contrary physical frame data exists.

## 6. Next experiment and acceptance barriers

**NOW:** preserve this exact wiring and successful software-first Fountain Video/Browser Scratch service. Keep the physical input crash guard enabled. Continue offline inspection of the original Sierra display PLL/mode-selection behaviour and old vendor receiver requirements. Compare existing source timing with the standard 74.25 MHz 1080p30 model. No reason to reconnect HDMI OUT.

**NEXT RECOVERABLE EXPERIMENT:** only with a verified independent screen-mode restoration/watchdog path and local console available, temporarily try a supported source timing that macOS can actually generate accurately; sample the CoreVideo clock **first**, then the driver lock status with **no `StartStreams`**. Verify restoration to the original display state after either success or failure. A past CoreGraphics 1080p24 → 1080p30 restoration encountered a stale display identifier; do not assume a mode-change API call is automatically safe on the only active display. Do not apply custom display overrides blindly or permanently alter Sierra.

**PHYSICAL ACCEPTANCE STILL REQUIRES:** receiver `lock=yes`, detected supported mode, actual non-no-source frame pixels + timestamp, crash-free teardown and finally the browser scratch feed explicitly marked as genuine hardware source. Since Desktop Video 10.4 previously crashed during frame-object release, do not re-enable legacy streaming on this non-recovery-backed boot merely because the source clock appears corrected.

**Status:** valid BMD sink EDID **PASS**; changing Mac desktop **PASS**; reproducible **nonstandard host display cadence DETECTED**; capture receiver lock **FAIL**; genuine HDMI frames **NOT ACCEPTED**.
