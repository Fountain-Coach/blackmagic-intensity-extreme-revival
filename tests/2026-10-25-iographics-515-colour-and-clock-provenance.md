# Original Sierra IOGraphics colour selection: mechanism, clock provenance, and enforced refusal

**Witness date:** 25 October 2026 (Europe/Berlin).  
**Ownership:** `Fountain-Coach/blackmagic-intensity-extreme-revival`.  
**Fixed test rig:** silver Macmini5,3 HDMI OUT → original Blackmagic Intensity Extreme HDMI IN; Intensity HDMI OUT empty; Thunderbolt as installed.

## Conclusion

The original Sierra graphics stack is almost certainly responsible for the surprising colour-setting behaviour previously observed: **a nonautomatic YCbCr422 connection-colour request is converted to RGB before it reaches the specific framebuffer driver**. This explains why `IODisplaySetIntegerParameter(cyuv, 0x10)` returned success while the selected `cmod` changed to RGB `0x1`. It does **not** prove that Intel HD3000 silicon cannot output 4:2:2, that a wire-encoded HDMI packet was ever measured, or that a different OS/driver would behave the same way.

The active Sierra OS reports **IOGraphicsFamily 515.4** and `AppleIntelSNBGraphicsFB 10.0.2`. The closest published Apple source tag independently located was **IOGraphics-515.3**, not a byte-for-byte source reconstruction of 515.4. Avoid claiming absolute binary identity.

## Original Apple source — source ownership and exact branch

[Apple IOGraphics source tagged 515.3](https://github.com/apple-oss-distributions/IOGraphics/blob/IOGraphics-515.3/IOGraphicsFamily/IOFramebuffer.cpp):

- Lines 82–83: active `#define AUTO_COLOR_MODE kIODisplayColorModeYCbCr444`; RGB alternative commented out.
- In `IOFramebufferParameterHandler::doIntegerSet` (around lines 10531–10550), when the parameter is `kConnectionColorMode`, **only** an automatic request can select `AUTO_COLOR_MODE`, provided the display platform allows it and the negotiated support mask includes 4:4:4.
- For every nonautomatic request on that branch, the value is **replaced with `kIODisplayColorModeRGB`** and written to selected-mode `cmod`; the rewritten value is then passed to `setAttributeForConnectionParam`. This is not a colour-format parser in our own code; it is the Apple framebuffer-family selection rule.
- `colorModesAllowed` is computed earlier (around line 6712) from the `graphic-options` / `kIOGPlatformYCbCr` flag. The live selected 444 result is consistent with this path.
- In `IOFramebuffer::doSetup` / setup of the active timing (around lines 7550–7582), `IOFBCurrentPixelClock` is derived from the graphics-driver-returned `timingInfo.detailedInfo.v2.pixelClock` (or the `minPixelClock` field when the vertical-blank throttling condition is enabled). It is a **reported timing property**, not a TMDS clock counter or HDMI wire analyzer. Likewise the `IOFBCurrentPixelCount` is computed from horizontal/vertical active and blanking totals.
- The separate CoreVideo display-link samples track the host display scheduler; they do not independently measure the electrical HDMI wire. An apparent 73,979,530 Hz output-clock discrepancy therefore remains a *software-timing observation*, not a diagnosed hardware PLL defect.

The exact running 515.4 source was not recovered. What is verified physically is the installed 515.4 behaviour, which matches the published 515.3 branch.

## Source-controlled physical observations

1. Before the colour experiment, the real BMD HDMI sink reports `cmod=0x100` (YCbCr444 selected), `cyuv=0x10000000` (automatic), `colr=0x111` (RGB/422/444 support advertised). Both direct AppleDisplay and CoreGraphics-derived IOKit services agree.
2. The first attempt to set the *selected* `cmod` field returned `kIOReturnError 0xe00002bc` via both service paths; no change occurred.
3. The corrected attempt set **connection-control** `cyuv=0x10`. The setter returned success, the control readback changed to `0x10`, and the *selected* colour mode became **RGB `cmod=0x1`**. The code then correctly declined to claim a 4:2:2 receiver-lock test. This incident initially exposed a recovery guard that could not restore from an RGB fallback.
4. The guard was repaired to permit restoration from observed RGB via `cyuv=auto`. Actual physical restoration was executed and witnessed: `cyuv=0x10000000`, `cmod=0x100`, original BMD HDMI **1920×1080 at UI 30 Hz**. The earlier automatic guard initially failed, and its failure remains part of the provenance, not recast as success.
5. Read-only timing audit independently confirms exact standard **74,250,000 Hz** *requested* pixel clocks for both 1080p30 (2200×1125) and 720p60 (1650×750). The driver instead reported **73,979,530 Hz**, −0.364269%, in both physical modes. This has **not** been measured on the HDMI wire.
6. Original 1080p30 and physically selected 720p60 both produced **no Blackmagic receiver lock**. There is still no valid physically captured frame.

## Enforced engineering safety — completed 25 October

Four previously invokable source-owned 4:2:2 experiments are **retired and fail closed**, rather than inviting repeated RGB fallbacks:

- [Correct IOKit connection-control helper](../src/sierra-hdmi-connection-color-control.c) `--set422` now exits **65** before any IOKit display write; its `--restore444` continues to restore **cyuv=automatic** from previously observed RGB.
- [Obsolete selected-colour helper](../src/sierra-hdmi-color-control.c) blocks all write/restore options, because `cmod` is the wrong surface for both operations.
- [cyuv bounded 422 trial](../tools/sierra/hdmi-ycbcr422-cyuv-bounded-trial.py) rejects `--trial-422` with **65**; the direct `run_trial()` function also raises before hardware access. Restore-only remains supported.
- [Legacy cmod bounded 422 trial](../tools/sierra/hdmi-ycbcr422-bounded-trial.py) likewise rejects its trial command and direct function calls.

[Regression suite](test_hdmi_retired_422_guard.py): **14/14 on Sierra Python 2.7, modern macOS Python 3, and Ubuntu Python 3**. It verifies C source guard ordering before setters, direct Python invocation fails closed and automatic restoration remains callable.

**Live Sierra installation acceptance:** both C helpers were recompiled using the original Sierra Clang with strict warnings, installed over their old `/tmp` diagnostic executables, and all four old trial entrypoints were run. Each returned **65** without changing hardware. The installed `--restore-only` returned **True**; live independent selected-mode readback remained `cmod=0x100`, `cyuv=0x10000000`, BMD HDMI 1080p30 online.

## What remains unproved

- Actual HDMI electrical clock / TMDS symbol frequency (no independent physical instrument).
- Actual on-wire AVI InfoFrame colour format, as opposed to OS-selected `cmod`.
- Whether an untouched standard 74.25 MHz 4:2:2 HDMI reference would lock the original Intensity receiver.
- Whether input lock is blocked primarily by source colour, timing, receiver-driver initialization, electrical link, or an independent receiver fault.
- Whether the frame-lifetime crash in original Desktop Video 10.4 can be safely contained during real frame capture.

**Next path:** do **not** patch Sierra graphics drivers, install an EDID override, re-enable crash-prone `StartStreams`, or repeat the forced 4:2:2 source request. Continue independent evidence gathering for receiver link/format status and legacy source capabilities. Any future physical colour-source experiment must use a proven *actual 4:2:2 source*, not merely a driver property naming that format, and must preserve the current fixed cabling contract unless the owner explicitly changes it.

**Hardware acceptance D4: OPEN.** No current physical receiver lock; original source restored and guard paths verified.

## Legacy path reconciliation and final validation

The old cmod Python entrypoint is now a **fail-closed compatibility shim**. The retired `--trial-422`, `--preflight` and `--watchdog` return **65**, without starting hardware procedures; `--restore-only` forwards directly to the supported cyuv automatic-colour restoration script. The shim handles both source-tree sibling naming and the known `fc-`-prefixed Sierra `/tmp` diagnostic installation path; no PATH search or arbitrary program resolution is involved.

An initial compatibility iteration failed its test suite because Python import-style execution omitted `__file__`, and a subsequent real installed-path check uncovered the `fc-` filename difference. Both issues were repaired and the full checks rerun. **14/14 tests now pass on each of Sierra, modern macOS, and Ubuntu.** Physical Sierra execution of the installed legacy `--restore-only` returned `external_restore=True`; its old preflight, watchdog and 4:2:2 trial each returned expected refusal **65**. Final source inspection remained BMD HDMI **1920×1080@30**, `cyuv=automatic`, `cmod=YCbCr444`.

The successful restoration was **idempotent**: the original colour was already selected. It does not establish that a newly induced graphics failure can always be automatically recovered.
