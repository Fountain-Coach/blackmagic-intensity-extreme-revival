# 24 October 2026 — original Intensity Extreme HDMI OUT monitor / recording replay extension

**Requested final physical topology:**
Silver Macmini5,3 HDMI OUT → ORIGINAL Blackmagic Intensity Extreme HDMI IN → the SAME Sierra Mac over original Thunderbolt 1/Comdis dock. Added expectation: Intensity Extreme **HDMI OUT → an external reference monitor**. No substitution of the silver Mac as signal source. The new monitor connection has **not been independently optically witnessed** by the assistant.

## Summary

| Gate | Result | Grounded evidence |
| --- | --- | --- |
| Original source recognized as BMD HDMI 1920×1080@30 | **PASS** | Sierra system_profiler, exact Macmini5,3 and original Desktop Video 10.4 driver |
| Original Blackmagic output interface + 1080p30 2vuy support | **PASS** | `IDeckLinkOutput` query rc=0 and `DoesSupportVideoMode` rc=0, support=1 |
| Blackmagic hardware accepts six-second generated YUV color-bar cue | **PASS at vendor API level** | EnableVideoOutput, CreateVideoFrame, GetBytes, DisplayVideoFrameSync, DisableVideoOutput each rc=0; output 1080p30 |
| Optical cue actually shown by newly connected monitor | **NOT YET WITNESSED** | API success is not a monitor image sensor |
| Playback of two prior private, real Blackmagic-captured JPEGs | **PASS at vendor API level** | Input archive SHA verified, YUV decode/upscale, four output calls rc=0 |
| Full decoded archived-frame bytes identical to Blackmagic mutable output buffer | **PASS** | 4 of 4 full `memcmp` comparisons before `DisplayVideoFrameSync` |
| New 30-frame HDMI IN recording after monitor extension | **NOT ACCEPTED** | HDMI input lock stayed false in two guarded runs; zero callbacks and zero frames |
| Native Fountain Video independent HDMI-input differential | **NOT ACCEPTED CURRENTLY** | Existing native AppKit `--self-test-fixed-silver-hdmi` returned exit 68: no signal lock |
| Input, playback, and native GUI cleanly released; original OS/driver preserved | **PASS** | Both bounded test procedures stopped safely; GUI reopened; no driver/firmware/OS changes |

## Source-owned implementation

- `src/decklink-hdmi-record-replay.cpp`: uses the **already-installed** original DeckLinkAPI.framework and established COM interface ABI. No vendored proprietary code. Read-only `--probe-output-1080p30`, explicitly activated `--monitor-cue-1080p30`, bounded `--record-replay-1080p30` and `--play-archived-physical-jpegs-1080p30`.
- `scripts/sierra-hdmi-roundtrip.sh --check|--prepare|--cue|--archive|--run`: verifies original Sierra host and 1080p30 BMD input display; builds a user-space x86_64 binary on silver; rejects concurrent Media Express or Black Syphon sessions; gracefully quits and restores installed Fountain Video.app for active tests. Replays are **not** automatically enabled during ordinary app launch.
- `--run` requires two consecutive successful `IDeckLinkStatus::GetFlag('visl')` locks before StartStreams. Once locked, it is designed to copy 30 complete 4,147,200-byte 1080p30 UYVY frames into caller-owned memory, using **Sierra host monotonic timestamps**, NEVER the old vendor `GetStreamTime` accessor (previously reproduced vendor destructor SIGSEGV). It records exact 30 raw frames with SHA-256 manifest in a **private user-owned 0700** folder on Fresh, then **stops and releases input** before it enables hardware output. Its output playback reads the recorded bytes back from disk and compares each full raw frame SHA with its input-side recording, submits each `DisplayVideoFrameSync` at 1080p30, and shuts down output. If no input lock, it creates **no recording** and does **not** falsely claim matched replay.
- Output cue and archived playback each explicitly activate and then disable HDMI output. The archived playback verifies full known source JPEG SHA-256 before decoding, converts to 1080p 2vuy output without claiming the JPEG lossy preview is byte-identical to the earlier original full raw UYVY frame.

## Read-only/output preflight on actual silver Mac

```text
stage=output_interface rc=0x00000000 present=1 active_output_enable_calls=0
stage=output_mode_probe mode=1080p30 pixel=2vuy rc=0x00000000 support=1
stage=output_preflight result=PASS monitor_light_not_tested=1
```

Six-second physical HDMI output cue on same device:

```text
stage=monitor_cue_enable rc=0x00000000
stage=cue_bars rc=0x00000000 seconds=6
stage=monitor_cue_disable rc=0x00000000
stage=monitor_cue result=API_ACCEPTED mode=1080p30
    output_API_accepted=1 optical_display_verified=0
```

The successful cue is a vendor playback acceptance only; no assertion of connected monitor's displayed pixels.

## Archived real recording replay — API accepted

The private raw captured screenshots were recorded from original physical Intensity Extreme HDMI IN earlier in this same session and JPEG encoded using native Sierra CoreGraphics. Source files remain **0600** on the silver Mac and are never committed or uploaded to public Git:

```text
/tmp/fc-intensity-extreme-physical-frame0.jpg
  30578 bytes SHA256 ea1f51836311a496d91f493f82bb9e380f41efff76992ef8438cea6511c562d3
/tmp/fc-intensity-extreme-physical-frame1.jpg
  30532 bytes SHA256 ab1abfddb80eb4a7d1404272aefed88d55373fd344dbb1debdc63a19c0c32634
```

The program checked both SHA-256 hashes *before* decoding the JPEGs; decoded full RGB 320×180 and upscaled/converted to the original card's 1920×1080 8-bit packed UYVY format for HDMI output. It played color-bar cue, then JPEG0, JPEG1, JPEG0, JPEG1, then closing bars. For **each of four outputs**, the created vendor frame's full 4,147,200-byte mutable buffer matched the decoded source-derived UYVY bytes, and `DisplayVideoFrameSync` returned **0 (S_OK)**:

```text
stage=archived_output_enable rc=0x00000000
stage=cue_bars rc=0x00000000 seconds=1
stage=archived_frame_playback recording_index=0 repeat=0 output_rc=0x00000000 full_output_buffer_matches_decoded_recording=1
stage=archived_frame_playback recording_index=1 repeat=0 output_rc=0x00000000 full_output_buffer_matches_decoded_recording=1
stage=archived_frame_playback recording_index=0 repeat=1 output_rc=0x00000000 full_output_buffer_matches_decoded_recording=1
stage=archived_frame_playback recording_index=1 repeat=1 output_rc=0x00000000 full_output_buffer_matches_decoded_recording=1
stage=cue_bars rc=0x00000000 seconds=1
stage=archived_output_disable rc=0x00000000
stage=archived_playback result=API_ACCEPTED actual_source_JPEGs=2
    output_submissions=4 byte_identical_converted_buffers=4
    optical_monitor_witness=NOT_MEASURED new_recording_accepted=0
```

This is successful **playback of historical real captured material**, NOT an acceptance of the entirely new input-record-output cycle, NOT HDMI cable passthrough, NOT byte-identical *optical* pixels at the external display and NOT evidence that a connected monitor was physically illuminated.

## Current input signal-lock regression — REAL BLOCKER

The silver Mac currently still reports **BMD HDMI** online as its **main display**, 1920×1080@30. Its original vendor driver **10.4** remains loaded. The original vendor configuration inspection (read-only) returned input connection `0x2` (HDMI) and output connection mask `0xa` (includes HDMI); no settings were changed.

During **two guarded `--run` attempts** separated by an output cue and a longer GUI handover delay:

```text
stage=enable_input rc=0x00000000 mode=1080p30 pixel=2vuy
stage=input_lock sample=0..11 rc=0x00000000 locked=no
stage=input_lock result=no_stable_lock
stage=disable_input rc=0x00000000
stage=recording_capture callbacks=0 frames=0 complete=0
stage=roundtrip result=NOT_ACCEPTED recording_folder=none
```

This is **not** because the newly created recorder mislabels the state. The original **already-accepted native Fountain Video** AppKit path also failed a separately invoked bounded retest at 15:50 local time:

```text
FountainVideo Silver HDMI loopback · 1080p30 · private preview
FountainVideo No HDMI signal lock. Capture NOT started.
stage=native_fixed_hdmi result=fail actual_rgba_preview=0
    dimensions=1920x1080 private=1 clean_teardown=1
    device_timestamp_called=0
exit=68
```

Thus actual HDMI input signal lock is **currently unavailable** although it was independently witnessed and yielded genuine frames earlier today. Do not assert a regression cause without a separately verified cable/signal/controller state. The new external HDMI output monitor connection, macOS output timing, and original HDMI input link need independent physical observations without changing the **approved** source topology. Do not substitute a second Mac, camera or generator.

## Bounded acceptance gate and next work

1. Human or an independent sensor **must verify the reference monitor** attached to Blackmagic HDMI OUT actually showed the color bars and the two archived captured images. An S_OK `DisplayVideoFrameSync` does not prove optical display.
2. Restore/confirm **actual signal lock** on the **same** silver Mac HDMI OUT → original Blackmagic HDMI IN topology, without replacing test source.
3. Re-run `sh scripts/sierra-hdmi-roundtrip.sh --run`, accept only when there are 30 complete valid new frames, an on-disk private raw recording+SHA manifest, 60 digitally byte-identical vendor output submissions, clean stop/release and a visible monitor playback witness.
4. Native installed `Fountain Video.app` was automatically restored after all one-off tests, and the separate Linux Docker guest/driver/firmware/host volumes remained unchanged. Test source images remain private.
5. PR #10 stays draft for physical monitor and *fresh roundtrip* acceptance. Earlier physical HDMI capture and local Linux Docker acceptance are retained as historical independently witnessed results, not erased by the later missing input lock.
