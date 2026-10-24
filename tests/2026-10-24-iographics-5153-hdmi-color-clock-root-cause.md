# Sierra HDMI colour and source-clock contract: Apple IOGraphics 515.x

**Date:** 24 October 2026. **Machine:** original silver Macmini5,3, Sierra 10.12.6, Intel HD Graphics 3000, installed IOGraphicsFamily **515.4**, AppleIntelSNBGraphicsFB **10.0.2**, original Blackmagic Desktop Video **10.4** and Intensity Extreme. Silver HDMI OUT stays connected to Blackmagic HDMI IN; Blackmagic HDMI OUT is empty; original Thunderbolt remains attached.

**Status:** The RGB fallback after a requested YCbCr 4:2:2 setting is explained by Apple's published IOGraphics source. The driver-reported pixel-clock discrepancy is a source timing-data observation, **not a physical HDMI wire-clock measurement**. No usable Blackmagic input lock or genuinely captured frame accepted.

## Authority and scope

[Apple's official IOGraphics 515.3 IOFramebuffer.cpp](https://github.com/apple-oss-distributions/IOGraphics/blob/IOGraphics-515.3/IOGraphicsFamily/IOFramebuffer.cpp) is the nearest observed published source version to the original Sierra machine's installed IOGraphicsFamily **515.4**. The installed binary itself was not disassembled; source/binary parity is therefore **supported by matching physical behaviour, not byte-for-byte verified**.

### Colour-mode selection

The source defines the preferred AUTO_COLOR_MODE as YCbCr 4:4:4 and implements its selection at IOFramebufferParameterHandler::doIntegerSet. On an **automatic** colour-control request with allowed YCbCr support, it selects 4:4:4; on an **explicit non-automatic** request, including 4:2:2, it substitutes RGB. The selected colour is published as cmod, while connection control is cyuv.

The actual physical Sierra observations match the source:

| Actual request | IOKit outcome | Sierra selected colour |
| --- | --- | --- |
| Initial cyuv=0x10000000 (auto) | Original state | cmod=0x100: YCbCr444 |
| Attempted direct cmod=0x10 write | kIOReturnError 0xe00002bc | Unchanged: YCbCr444 |
| Correct cyuv=0x10 request | Setter success | **cmod=0x1: RGB**, not 4:2:2 |
| Restored cyuv=0x10000000 | Setter success | **cmod=0x100: YCbCr444** |

The 4:2:2 request was accepted **as a control operation**, but the output was not selected as 4:2:2. No 4:2:2 wire signal was verified and no Blackmagic receiver lock test was run under a proven 4:2:2 source. This does NOT establish Intel HD3000 hardware is incapable of generating 4:2:2, only that the observed generic IODisplay route cannot force it.

The prior restoration defect is explicitly documented in [the actual incident report](2026-10-24-hdmi-cyuv-rgb-fallback-and-restoration-incident.md): the first guard rejected the unexpected RGB intermediate state and the automatic local/external restoration attempts failed. A separate SSH-triggered physical restoration was then performed successfully, and the guard was fixed and regression-tested. **Do not mislabel the initial watchdog failure as success.**

### What IOFBCurrentPixelClock means

The same Apple IOGraphics 515.3 source populates IOFBCurrentPixelClock in IOFramebuffer::doSetup using the graphics driver's detailed timing information returned for the active display mode; under a vertical-blank-throttling condition it can substitute a corresponding timing-limit value. This is **not a hardware HDMI waveform measurement**.

The real Intel source reports requested **74,250,000 Hz** for both 1080p30 (2200x1125 totals) and 720p60 (1650x750 totals), while reporting **73,979,530 Hz** as the framebuffer clock in the two physical source modes. The difference is about **-0.364269%**. Earlier host-time CoreVideo cadence also followed the nonstandard source scheduling. However, no test directly measured TMDS/HDMI electrical clock or captured AVI InfoFrames, so a bad physical source PLL and a colour-format receiver incompatibility remain **hypotheses, not accepted root causes of the lost Blackmagic lock**.

## Actual source-code maintenance performed and installed

- [Sierra cyuv colour-control helper](../src/sierra-hdmi-connection-color-control.c): blocks the known misleading set422 command with **exit 65**, before any IOKit write; its inspect mode and restore444 (return to cyuv automatic, including RGB intermediate) remain functional.
- [Retired cmod helper](../src/sierra-hdmi-color-control.c): refuses every write route with exit **65**; inspect remains read-only.
- [Retired cyuv 4:2:2 controller](../tools/sierra/hdmi-ycbcr422-cyuv-bounded-trial.py): blocks both CLI trial and direct run_trial function; independently callable restore-only remains available.
- [Old cmod trial script](../tools/sierra/hdmi-ycbcr422-bounded-trial.py): reduced to a fail-closed compatibility shim, with only restore-only delegated to the current cyuv automatic restore.
- Both modified C helpers were compiled with strict Sierra Clang and installed over their previous temporary diagnostic binaries; the modified Python trial scripts were installed on the authorized Sierra Mac.

**Physical acceptance of maintenance:** new and old 4:2:2 entrypoints each returned **65**, without source changes or vendor API calls. The current auto-restoration command returned success, and direct BMD IODisplay reads still reported **cmod=0x100, cyuv=0x10000000, BMD HDMI 1920x1080 at 30 Hz online**.

**Pure regression suites (locally run, no GitHub CI):** [11 fail-closed tests](test_hdmi_retired_422_guard.py) and [9 Apple 515.3 source-contract tests](test_apple_iographics_5153_hdmi_contract.py) each passed completely on **Sierra Python 2.7, modern macOS Python 3 and Ubuntu Python 3**. These verify implemented safe branches and the source-model decision table; the source-model tests do not claim the actual 515.4 binary was exhaustively validated.

## Next engineering target

The colour-control path can now be treated as exhausted without Intel-driver replacement or a different independent HDMI waveform witness. **Do not repeat the 422 request, install EDID overrides, patch graphics drivers, or touch HDMI wiring.**

Next, investigate the **original Blackmagic Desktop Video 10.4 input/status lifecycle** and whether a no-stream status query can truthfully detect signal lock, given that earlier native streaming returned only frames marked no-input-source and a separate legacy frame-lifetime destructor crash. Work from existing source, vendor SDK contracts and simulator tests, keeping crash-prone native streaming disabled until a contained recovery path exists.

**Physical capture acceptance task D4 remains OPEN.**
