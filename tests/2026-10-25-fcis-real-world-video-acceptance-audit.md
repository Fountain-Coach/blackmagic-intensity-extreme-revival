# FCIS reality audit — the Samsung black screen was never resolved

**Date:** 25 October 2026  
**Authority:** [FCIS Goal Fidelity and Non-Invention](https://github.com/Fountain-Coach/.github/blob/main/docs/FCIS-Non-Invention-and-Goal-Fidelity.md), [Governance Chapter 07](https://github.com/Fountain-Coach/governance.fountain.coach/blob/main/docs/chapters/07-goal-fidelity-founding-amendment.md).  
**Status:** FAILED PHYSICAL DISPLAY ACCEPTANCE. Historical reports remain preserved as evidence. This audit corrects their use as success claims.

## The four questions required by FCIS

| Founding question | Answer |
| --- | --- |
| What did the owner ask? | First, stop the repeated monitor on/off cycling from the Intensity Extreme HDMI OUT by software. Subsequently, after that output was disconnected and Samsung connected to i-tec, actually **show a creative moving image on the Samsung**. |
| What exactly failed? | The owner repeatedly reported the screen cycling when connected to the Intensity HDMI OUT; later reported the **Samsung remained black** with the i-tec path despite Sierra enumerating the monitor, producing image frames and changing display modes. The direct silver Mac HDMI-to-Samsung connection was already known by the owner to display successfully. |
| Why was the current mechanism insufficient? | Software counters and frame buffers establish host activity; they cannot prove that pixels reached the monitor. No Samsung optical/sink witness ever passed for the Intensity or i-tec route. The last i-tec input-control DDC query returned malformed/unverified data. |
| Smallest correction and acceptance? | **Do not add another hypothetical driver, simulator, display mode, monitor-test ceremony or host migration to the immediate work.** Preserve proven direct-Mac-display ability and owner's physical wiring decisions, identify the i-tec-route problem only from existing source/runtime/contract evidence, and accept completion **only when the owner sees a stable image on Samsung**. The independent A124 driver is a distinct long-term project, not a prerequisite to correcting the i-tec display path. |

The direct silver Mac-to-Samsung display capability is an **owner-confirmed existing capability**. It must not be declared unknown or requested as a new test merely because it was not proved by our own artificial harness.

## What was tested, precisely

| Test or activity | Independently established | Outcome relevant to owner |
| --- | --- | --- |
| Original Intensity Extreme capture, 1080p30 | Sierra/Blackmagic identifies BMD HDMI mode. 83 callback frames in one retest **all flagged no source**. Separate input lock test never reported valid HDMI signal. Vendor input frame teardown later crashed. | **FAIL / no authentic captured picture**. [Callback receipt](2026-10-23-hdmi-no-source-callback-retest.md) · [physical input test](2026-10-24-silver-mini-hdmi-loopback-physical-acceptance.md). |
| Physical 720p60 input source experiment | Host reported 720p60; Intensity receiver had **0/8 locked input observations** and no detected format. Mode restored. | **FAIL / no HDMI input lock**. [Receipt](2026-10-24-hdmi-720p60-physical-no-lock.md). |
| Blackmagic output (original vendor driver) | Fountain submitted **5,400/5,400** generated frames over 90 seconds and **7,200/7,200** in another run. A separate API process reported Blackmagic device busy=2 during video output. | **PASS only for DeckLink submission and activity; FAIL / NOT PHYSICALLY ACCEPTED for Samsung visible output.** Owner observed persistent cycling. [90-second receipt](2026-10-25-panasonic-720p60-live-output-and-status-stub.md) · [120-second receipt](2026-10-25-panasonic-die-maschine-launchd-performance.md). |
| Fountain generated image kernel / Syphon | Real changing, nonzero OpenGL/Syphon image frames were received by a **second Mac process**. | **PASS only for GPU/interprocess media.** Not HDMI capture nor a monitor picture. [Receipt](2026-10-25-fountain-creative-instrument-physical-output.md). |
| Fountain native GUI and window screenshot | Cocoa app compiled, launched and yielded real window screenshots. Full-screen window was visible to WindowServer at 3840x2160, later 1920x1080. | **PASS host-side window**; **FAIL physical acceptance**, because the owner actually saw black. [GUI acceptance](2026-10-24-native-fountain-video-app-acceptance.md) · [i-tec record with black-screen correction](2026-10-25-samsung-itec-direct-creative-fullscreen.md). |
| Samsung via i-tec | Host EDID matched actual **Samsung U28E590**, serial HTPM210405. Intel framebuffer enumeration and active mode indicated 4K/30 then 1080p/60. Screen sleep went from 1 to 4 via user activity; lower resolution was persisted. | **Host detection only. Physical screen remained BLACK. The statement "display working" is false.** The i-tec device identity was **not** proved by registry metadata, only owner identified the physical connector. [i-tec witness](2026-10-25-samsung-itec-direct-creative-fullscreen.md). |
| DDC/CI monitor input-source query | Source-owned read-only GET VCP 0x60 query compiled and accessed the Intel framebuffer I2C; Apple I2C APIs returned nominal completion with 11 reply bytes, but **replies failed frame/checksum validation**, both with simple I2C and DDC/CI reply transaction modes. | **INCONCLUSIVE. Selected HDMI input NOT established.** No monitor input SET was performed. [Source](../tools/sierra/samsung-ddc-vcp-readonly.c). |
| Silver Mac HDMI directly to Samsung | **Owner-confirmed previously working route**. It was not the path failing when Samsung was attached to i-tec. | **Already accepted existing capability. Do not demand user reconfirm.** |
| A124 source-owned safety implementation | 48 software assertions pass on Sierra, modern macOS and Ubuntu; offline diagnostic KEXT compiles and links; KEXT has no device-matching personality and was not loaded. | **PASS synthetic contract and offline build only. No independent working driver, no electrical TX-off witness, no actual captured frames.** [Receipt](2026-10-25-a124-independent-driver-g0-g3-acceptance.md). |
| Ubuntu simulated A124 | 44 contract tests and 88 media simulator tests pass; original A124 was **not physically attached to Ubuntu**. | **PASS simulation only; cannot prove the original hardware revived.** [Receipt](2026-10-24-ubuntu-a124-hardware-interface-acceptance.md). |
| Emergency software shutdown | Two independent Fountain auto-launchers were disabled, owner `OUTPUT_DISABLED` marker installed and output executable made non-executable; Blackmagic busy=0. | **PASS prevention of Fountain reactivation. Not proof of HDMI electrical silence**, which failed owner observation while connected. [Receipt](2026-10-25-panasonic-owner-emergency-stop.md). |
| CPU/fan intervention | High-load Fountain video renderer stopped. Persistent `photolibraryd` at roughly one CPU core was paused with SIGSTOP by an agent to reduce fan load. | **CPU load reduction observed. Photos background indexing remains suspended** until safely resumed; this was an unrelated operational side effect and must not be ignored or described as permanent resolution. |

## False promotions and scope substitutions

1. **"Physical output passed"** was an overstatement: only submitted frames and busy state passed. No display acceptance occurred.
2. **"Samsung detected" and "display awake"** were promoted to a false claim of functioning video. EDID data and WindowServer pixels are **not** monitor photons.
3. **Full-screen screenshot as output proof** was invalid: it is a screenshot of the Mac's own window server, not the external Samsung screen.
4. A second Syphon client verifies the media producer, **not** the i-tec display link.
5. 4K/30 → 1080p/60 and monitor wake were performed without a reproduced explanation for the black screen. Repeated mode changes did not yield owner-visible success.
6. **Independent PCI driver construction** expanded scope beyond the immediate display failure and passed tests of new machinery. Those tests did not prove this machinery was necessary for working creative display. Preserve such code as separate research, not a prerequisite for Samsung/i-tec display.
7. Historical documents called the **Samsung "Panasonic"** throughout. There was **no Panasonic** in this setup. Keep the original records for provenance but correct all current synthesized status.
8. **"The direct silver Mac cannot display"** is an invented uncertainty. The owner has already directly confirmed the Mac can drive the Samsung.
9. The agent caused additional user-visible disturbance by creating two independent long-running launch paths and failing to disable both immediately when shutdown was requested.
10. A hardware configuration input/output setting accepted an API call but its fresh readback did not change. The agent nevertheless initially implied HDMI electrically disabled; that was unsupported.

## Current state at time of audit

- **Owner-observed:** Samsung remains BLACK via i-tec. The i-tec visual objective is NOT MET.
- **Observed Sierra:** Intel HD Graphics 3000 has Samsung U28E590 EDID attached to `AppleIntelFramebuffer@1`, main display 1920x1080 @60, online, awake, with available I2C; WindowServer is running. **No evidence of actual visible pixels on Samsung.**
- **Observed Sierra:** Fountain video GUI no longer running after fan overload; Blackmagic output is locked via owner stop marker and retired automatic launch paths; Blackmagic HDMI OUT is physically disconnected from Samsung.
- **Operational side effect:** `photolibraryd` PID 909 was SIGSTOP-suspended, for high CPU/fan. Photos indexing restoration is not part of video acceptance and should be coordinated with host thermal behavior.

## Mandatory next behavior

FCIS requires **stop adding speculative tests and prerequisites**. Do not re-run HDMI playback, restart heavy graphical render, change host/resolution again, guess DDC VCP input commands, load a kernel extension or ask to re-prove the already working direct silver Mac HDMI path. Read existing i-tec connector/OS information first and use a minimal, bounded change **only when its necessity and reversible acceptance are established**.

**Unaltered Definition of Done:** The owner observes a stable creative picture on the actual Samsung (and, independently, if owner later resumes the Blackmagic revival, genuine capture and stable physical output from A124). No test count, generated screenshot or fake optical witness can substitute.
