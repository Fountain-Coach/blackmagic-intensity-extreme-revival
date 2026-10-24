# Sierra BMD HDMI source clock and colour-selection investigation

Physical witness: 24 October 2026, original silver Macmini5,3, Sierra 10.12.6, Intel HD Graphics 3000, original Blackmagic Desktop Video 10.4 / Intensity Extreme A124.

**Fixed topology:** silver Mac HDMI OUT to original Intensity Extreme HDMI IN; Blackmagic HDMI OUT disconnected; original Thunderbolt unchanged.

**Verdict:** Sierra requests standard 74.25 MHz for both tested modes but reports 73.979530 MHz operating framebuffer clock. It selects YCbCr 4:4:4 while supporting 4:2:2. Two independently guarded attempts to change the selected colour to 4:2:2 via the public IOKit setter returned generic error and did not apply. NO real 4:2:2 receiver-lock test occurred.

## Requested clock versus reported operating clock

The source-owned [read-only timing and colour audit](../tools/sierra/hdmi-timing-color-audit.py) parses the physical IOGraphics detailed timing records:

| Mode | Timing ID | Active resolution | Pixel totals | Requested pixel clock | Requested rate |
| --- | --- | --- | --- | --- | --- |
| 1080p30 | 0x80001008 | 1920 x 1080 | 2200 x 1125 | 74,250,000 Hz | 30 Hz |
| 720p60 | 0x80001001 | 1280 x 720 | 1650 x 750 | 74,250,000 Hz | 60 Hz |

On the active BMD HDMI framebuffer index 1, I/O Registry reports IOFBCurrentPixelClock = 73,979,530 Hz. Current 1080p30 pixel count 2,475,000 gives **29.890719 Hz**. During the separately documented actual 720p60 trial the count was 1,237,500, giving **59.781438 Hz** with the same registry clock. Both correspond to **-0.364269%** relative to the requested pixel clock. CoreVideo independently reproduced the 1080p30 display-scheduler cadence.

These are operating-system display observations, NOT electrical HDMI pixel-clock or AVI InfoFrame measurements; their causal relationship to missing receiver lock remains unproven.

## Sink EDID and current colour format

The exact BMD EDID (vendor 8961, product 0) passes checksum and advertises compatible timing and YCbCr 4:2:2 / 4:4:4 capabilities. Sierra IODisplay reports:

| Key | Meaning | Value |
| --- | --- | --- |
| cmod | selected colour mode | 0x100 = YCbCr444 |
| colr | supported colour modes | 0x111 = RGB + YCbCr422 + YCbCr444 |
| cyuv | connection colour control | 0x10000000 = automatic |

The [read-only C witness](../src/sierra-hdmi-color-readonly.c) and the separate IODisplay inspection paths through AppleDisplay and CoreGraphics returned identical 0x100 values. The original Intensity Extreme documents 4:2:2 YUV over HDMI, making 4:4:4 a candidate compatibility mismatch. Nevertheless, the reported selected value is NOT a direct capture of the HDMI wire format.

## Bounded physical YCbCr422 attempt

The [Sierra colour trial](../tools/sierra/hdmi-ycbcr422-bounded-trial.py) armed an independent Sierra watchdog before each write and a separately scheduled SSH restore from the modern Mac. The controller preserved the original 1080p30 display mode, did not call IODisplayCommitParameters or write an EDID override, and never started Blackmagic input streams.

First attempt, CGDisplay-derived IODisplay service:
- Requested cmod = 0x10 (YCbCr422), from 0x100 (YCbCr444).
- IODisplaySetIntegerParameter returned **0xe00002bc**.
- Readback stayed 0x100. No capture test attempted.

Second attempt, direct AppleDisplay service, with CGDisplay fallback:
- AppleDisplay write returned **0xe00002bc**.
- CGDisplay write also returned **0xe00002bc**.
- Readback stayed 0x100. No capture test attempted.

The original Sierra IOReturn.h defines this value as **kIOReturnError, a generic error**, so it does not establish whether the selector is read-only, an option is unsupported, or a different API is required.

Both independent Sierra watchdogs, both controller restoration checks and the modern Mac's delayed external restore commands confirmed the original 0x100 selection. The physical desktop remained BMD HDMI 1920 x 1080 at UI 30 Hz. No cables, firmware, drivers, pixel streams or system configuration were changed.

**Important interpretation:** the source did NOT switch to 4:2:2, therefore this experiment is **not evidence that 4:2:2 input fails on Blackmagic**. The source-owned result field accepted_receiver_lock=false denotes no accepted 4:2:2 capture, not a measured receiver-no-lock state at 4:2:2. Earlier 1080p30 baseline is still receiver lock 0/8 at original source settings.

## Reproducibility and next action

Source-owned [timing/colour audit](../tools/sierra/hdmi-timing-color-audit.py) ran on the physical Sierra host. Its [nine synthetic decoding tests](test_hdmi_timing_color_audit.py) passed 9/9 on Sierra Python 2, modern macOS Python 3 and Ubuntu Python 3.

[Independent colour readout](../src/sierra-hdmi-color-readonly.c), [bounded colour helper](../src/sierra-hdmi-color-control.c) and [physical guarded colour trial](../tools/sierra/hdmi-ycbcr422-bounded-trial.py) preserve the exact behaviour for comparison.

**Next:** inspect offline how Sierra Intel HD3000 chooses the actual HDMI colour output. Do not repeat the same failed IODisplay cmod write, do not install persistent EDID overrides or modify global video driver preferences, and do not call the crash-prone DeckLink StartStreams path. Find a proven reversible mechanism before another live colour test. Separately investigate whether the reported 73.97953 MHz differs from the physical HDMI wire clock.

**D4 remains OPEN: no confirmed receiver lock or real non-no-source captured frame.**
