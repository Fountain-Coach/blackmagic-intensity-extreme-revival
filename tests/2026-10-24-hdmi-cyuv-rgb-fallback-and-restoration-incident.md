# Sierra HDMI cyuv control: accepted request, RGB fallback, restored original

**Date:** 24 October 2026. Exact hardware: silver Macmini5,3 HDMI OUT -> original Blackmagic Intensity Extreme HDMI IN, HDMI OUT unconnected, Thunderbolt and original Sierra 10.4 driver unchanged.

**Physical outcome:** Setting the proper HDMI *connection* colour control (cyuv) to request YCbCr 4:2:2 **succeeded as an IOKit API call**, but Intel HD3000 selected RGB rather than YCbCr 4:2:2. An initial restore watchdog defect was exposed and fixed. The original automatic mode, which selects YCbCr 4:4:4, was physically restored and verified. **No receiver-lock test ran at YCbCr 4:2:2; no video frame was captured.**

## Why the previous test failed

Sierra IOGraphicsTypes.h distinguishes selected colour mode **cmod** from connection colour-mode control **cyuv** (kConnectionColorMode = 'cyuv'). The two earlier guarded cmod writes through AppleDisplay and CGDisplay each returned generic kIOReturnError 0xe00002bc, leaving cmod unchanged.

The corrected source-owned helper, [sierra-hdmi-connection-color-control.c](../src/sierra-hdmi-connection-color-control.c), uses IODisplaySetIntegerParameter on **cyuv**, not cmod. The original state before the test was:

- cmod = **0x100**, selected YCbCr 4:4:4;
- cyuv = **0x10000000**, automatic colour-mode control;
- colr = **0x111**, supported RGB + YCbCr 4:2:2 + YCbCr 4:4:4;
- BMD HDMI 1920x1080 at 30 Hz (unchanged physical topology).

## Controlled real change

The [bounded cyuv controller](../tools/sierra/hdmi-ycbcr422-cyuv-bounded-trial.py) armed a separate Sierra watchdog and a delayed independent modern-Mac SSH restore before the experiment. Both control and selected state were measured by AppleDisplay and CGDisplay service paths.

On the actual physical Mac:

    stage=color_write attr=cyuv old_control=0x10000000 target_control=0x00000010 old_cmod=0x00000100 commit_parameters=0
    stage=color_write path=AppleDisplay rc=0x00000000
    stage=io_display_param path=after_write name=cmod rc=0x00000000 value=0x00000001
    stage=io_display_param path=after_write name=cyuv rc=0x00000000 value=0x00000010
    stage=color_write result=NOT_VERIFIED control_after=0x00000010 selected_after=0x00000001

**Interpretation:** Sierra accepted the 4:2:2 *control request*, but the driver-selected result cmod=0x1 was **RGB**, not cmod=0x10 YCbCr422. The result must not be called an output signal in YCbCr422 or a Blackmagic 4:2:2 lock experiment. It is also not an independently measured HDMI AVI InfoFrame.

The controller deliberately rejected the result and never called the DeckLink receiver status probe.

## Recovery-guard defect and physical repair

The first version of the new source-owned restoration helper incorrectly whitelisted only cmod=0x100 (444) and cmod=0x10 (422). It rejected the unforeseen **RGB cmod=0x1** state and therefore failed to perform the automatic return to cyuv=auto.

That initial failure affected all three paths: controller-initiated restore, local Sierra watchdog and delayed modern-Mac SSH fallback, because all called the same helper implementation. **This is a genuine restoration implementation defect, not a pass.** The controller returned exit 70 and reported original colour NOT restored. It did not affect SSH or the ordinary 1080p30 source display.

The recovery guard was repaired in the actual Sierra source file to allow **RGB as a valid intermediate state for restoration only**. We then recompiled and invoked the correct connection-control restore from a separate SSH process. This physical restoration succeeded:

    stage=color_selected cmod=0x00000001 cyuv=0x00000010
    stage=color_write attr=cyuv old_control=0x00000010 target_control=0x10000000 old_cmod=0x00000001 commit_parameters=0
    stage=color_write path=AppleDisplay rc=0x00000000
    stage=io_display_param path=after_write name=cmod rc=0x00000000 value=0x00000100
    stage=io_display_param path=after_write name=cyuv rc=0x00000000 value=0x10000000
    stage=color_write result=APPLIED control_after=0x10000000 selected_after=0x00000100

**Actual post-recovery state:** IODisplay independently reported cmod=0x100 and cyuv=0x10000000 again, and macOS System Profiler reported online **BMD HDMI 1920 x 1080 at 30 Hz**. The original Desktop Video 10.4 driver remained intact.

The repaired version is source-owned as [commit 747142c](https://github.com/Fountain-Coach/blackmagic-intensity-extreme-revival/commit/747142cae9104231c6b3526fdabaf4e90274567b). The [five RGB-fallback guard regressions](test_hdmi_cyuv_rgb_recovery.py) now pass **5/5 on Sierra Python2, modern macOS Python3 and Ubuntu Python3**, including a synthetic RGB to auto-to-444 restoration. These regression tests do not substitute for a physical guard fault-injection test.

## Physical acceptance and next experiment boundary

- Correct connection-control API identified: **PASS**.
- YCbCr422 connection-control request accepted by IOKit: **PASS**.
- Actual selected YCbCr422: **FAIL / selected RGB**.
- Actual 4:2:2 HDMI wire output: **NOT WITNESSED**.
- Blackmagic signal lock during 4:2:2 source output: **NOT TESTED**.
- Automatic restoration during unexpected RGB selection: **FAILED IN FIRST RUN**.
- Subsequent manually invoked physical RGB -> automatic -> YCbCr444 restoration: **PASS**.
- Original BMD HDMI 1080p30 source restored: **PASS**.
- Real captured input pixels/timestamps: **NOT ACCEPTED**.

**Do not rerun the live 4:2:2 selection blindly.** The Intel graphics driver changed to RGB despite the 4:2:2 connection request. Investigate why it selects RGB for cyuv=422, and only run further physical changes under a fully validated restoration and recovery guard that recognizes *all observed* states. We should avoid persistent EDID overrides, OS driver replacement or firmware changes without independently boot-tested rollback.

The separate source pixel-clock discrepancy remains unchanged: requested 74.25 MHz, I/O Registry operating 73.97953 MHz. Neither the source clock nor the selected colour mode has been proven to cause absent Blackmagic input lock.

**D4 remains OPEN.**
