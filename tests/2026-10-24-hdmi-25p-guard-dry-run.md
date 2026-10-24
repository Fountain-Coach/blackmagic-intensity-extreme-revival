# Sierra HDMI recovery guard and 1080p25 lock-probe preflight

**Date:** 24 October 2026. **Witness:** original silver Macmini5,3 / Sierra 10.12.6 / original Desktop Video 10.4.

**Physical cabling: unchanged.** Silver HDMI OUT → Intensity Extreme HDMI IN; Intensity Extreme HDMI OUT empty; original Thunderbolt stays connected.

**Decision: guard DRY-RUN PASS; 1080p25 offered by macOS; NO-GO to live display-mode change on this session.** The source-selected mode stays 1080p30. We do not have an independently witnessed local console, external bootable rollback or physical mode-recovery acceptance. The simulated watchdog is not an armed real mode-restoration service.

## Source-owned components

1. [Sierra mode recovery guard](../tools/sierra/hdmi-mode-recovery-guard.py): `--inspect` reads the direct BMD EDID and current source mode; `--dry-run` launches an independent process and deliberately SIGKILLs a dummy test controller to verify failure detection, synthetic 25→30 recovery decision and actual unchanged BMD mode. It makes **zero** `CGDisplaySetDisplayMode` calls. The `restore_mode_30` implementation can be exercised via *mock* CoreGraphics, but no live-writing command-line entrypoint is exposed. No vendor frame/video API calls.
2. [Guard unit tests](../tests/test_hdmi_mode_recovery_guard.py): eleven synthetic tests, including fresh display identifiers after mode changes, refusal of a Samsung EDID / multiple active displays, wrong-mode failure, unavailable original mode, unsuccessful restore API and independently detected dead controller. **11/11 passed** on Sierra Python 2.7, modern macOS Python 3 and the Ubuntu 24.04 analysis host.
3. [No-stream HDMI input-lock probe](../src/decklink-input-lock-probe.cpp): on the **exact original Sierra model**, checks a direct BMD HDMI source and requested active 1080p25 or 1080p30. An informational `--preflight-1080p25` never loads the vendor runtime. A `--probe-active-1080p25`/30 command refuses to arm input if the selected source rate differs, checks HDMI connector selection through the original DeckLink configuration, temporarily `EnableVideoInput`, reads status eight times, and always disables input. It does not call `StartStreams`, install callbacks, touch captured frame objects, change macOS display mode, or reconfigure HDMI connectors. Exit 32 means no lock, not a process crash.
4. [Previous source-cadence measurements](2026-10-24-source-cadence-receiver-no-lock.md): macOS selected 30 Hz mode, I/O Registry and CoreVideo display cadence ~29.8907 Hz. These are **not** electrical HDMI wire measurements.

## Executed acceptance on physical Sierra Mac

```text
stage=guard_inspect bmd_present=True current=(1920, 1080, 30.0) mode_1080p25=True mode_1080p30=True mode_change_calls=0
stage=guard_preflight result=pass source=BMD_HDMI current=1080p30 candidate=1080p25 recovery_target=1080p30
stage=guard_fault_injection result=controller_killed separate_watchdog_pid=[ephemeral]
stage=guard_observation result=pass trigger=controller_died actual=ALREADY_ORIGINAL simulated25=RETURN_TO_ORIGINAL_REQUIRED display_writes=0 video_streams=0
stage=qualification dry_run_only=1 physical_restore_from_1080p25_NOT_TESTED=1 live_mode_change_authorized=0
```

The original physical display remained **BMD HDMI 1920 × 1080 @ 30 Hz** before and after the failure-injection.

```text
stage=source result=BMD_HDMI active_width=1920 active_height=1080 selected_rate_hz=30.000000 required_rate_hz=25 mode_advertised=1 active_mode_match=0 cg_display_only=1 mode_changes=0
stage=preflight result=pass display_modified=0 receiver_lock=not_tested no_vendor_calls=1
stage=mode_guard result=BLOCKED_wrong_current_rate vendor_calls=0
```

The initial attempt to invoke `--probe-active-1080p25` on the current 1080p30 source was correctly rejected with **exit 25** before any DeckLink interface was opened. The command compiled with **Sierra Clang++ / C++11 `-Wall -Wextra -Werror`**.

A second 1080p30 baseline measurement with this new source-owned probe yielded:

```text
stage=input_connector rc=0x00000000 value=0x2 expected=0x2
stage=enable_input mode=30p pixel_buffer=2vuy rc=0x00000000
stage=receiver result=NO_LOCK locked_samples=0 correct_detected_mode_samples=0 real_frame_acceptance=NOT_TESTED
stage=disable_input rc=0x00000000
stage=complete exit=32 callback_installed=0 stream_started=0 captured_frames=0 display_mode_changes=0 driver_firmware_changes=0
```

This means **8/8 physical input-lock status readings were negative** and 8/8 input-mode detection requests failed with `0x80000001`. The driver configured and released its **input mode only**, without streaming or frame-object release. Afterward, Sierra still reported BMD HDMI 1080p30.

## Future bounded physical 1080p25 experiment (not run)

### Preconditions

1. **A verified local recovery/observation arrangement** independent of the silver Mac's current only active HDMI display; do not assume SSH alone or mocked watchdog code guarantees a recoverable graphics state. The earlier Sierra 1080p24↔30 experiment returned a stale Quartz display identifier and did not immediately restore through the old ID.
2. An operational, independently launched, **live restoration watchdog** that is verified for *physical* 25→30 mode restoration, along with a fallback for lost SSH / display enumeration. The dry-run tool in this commit deliberately does not supply such proof.
3. Capture-driver crash protection stays active. No device driver swap, PCI MMIO, firmware update or guest/host operating-system changes.

### Gated procedure

- Verify BMD direct EDID and source clock; preserve exact original mode details.
- With restoration independently secured, request **source 1080p25**, bounded and nonpersistent; immediately verify the actual timing with source I/O Registry and CoreVideo clock witnesses. If timing is wrong or the display cannot be enumerated, return immediately.
- Only after the physical source really selects **1080p25**, run `--probe-active-1080p25` (no `StartStreams`). A positive signal lock and detected 1080p25 mode would support source-timing diagnosis, but still not establish real video-frame capture.
- Restore 1080p30 automatically using a freshly enumerated display ID, verify timing/display identity afterward and repeat the no-stream baseline if appropriate.
- Preserve both positive and negative evidence; never claim source code/simulator proof equals physical video acceptance.

## Status and blocker

| Boundary | Acceptance |
| --- | --- |
| Exact physical wiring | Confirmed, unchanged |
| BMD HDMI EDID and available 1080p25 mode | PASS |
| Source-side abnormal 1080p30 display cadence | OBSERVED; physical wire unmeasured |
| Independent watchdog surviving controller SIGKILL | PASS **in dry-run** |
| Mock 25→30 restoration algorithm, including changed Quartz IDs | PASS (11/11 synthetic tests) |
| Actual 25→30 restoration while silver Mac is headless | **NOT TESTED — NO-GO** |
| Current 1080p30 Blackmagic input lock | **FAIL — 0/8** |
| Live 1080p25 Blackmagic input-lock experiment | **NOT RUN — gated** |
| Actual captured frame, timestamp and crash-free teardown | **NOT ACCEPTED** |

**Do not connect additional cables or switch output mode automatically under this report.** We have accomplished all available no-change software and simulated recovery work for this step. The remaining physical experiment has a real, identified recovery precondition rather than a speculative software blocker.
