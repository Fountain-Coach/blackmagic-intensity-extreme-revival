# 24 October 2026 — FINAL silver HDMI input-lock regression, verified no-frame result, guarded recording/replay gate

## Authorization and unaltered topology

The physical setup is final and must not be replaced:

**Original silver Macmini5,3 HDMI OUT → original Blackmagic Intensity Extreme HDMI IN → original Intensity Thunderbolt 1/Comdis dock → SAME silver Mac running Sierra 10.12.6. The original Intensity HDMI OUT is now additionally connected to the external monitor.**

The original Desktop Video **10.4** driver and system remain untouched. The separate Alpine xhyve/Linux Docker guest remains downstream and unmodified. The installed `~/Applications/Fountain Video.app` retains its session-scoped 1080p30 output default and `black` idle picture; it was stopped temporarily for **exclusive input API diagnostics** and restored automatically after each bounded run.

Past independently witnessed successful input capture, two genuine physical raw frames/JPEGs, 17 genuine frames in same-silver Linux Docker, 918 vendor-accepted output playback frames and four archived recording frame submissions are preserved as **historical successes**. The new HDMI IN no-lock regression is **current**, and does not erase past success.

## Actual present signal state — grounded observations

- Sierra still enumerates exactly `BMD HDMI` as its **main, online, active 1920×1080 @ 30 Hz** display.
- GPU registry reports the expected **74,250,000 Hz pixel clock** for its 1080p30 HDMI output. This does **NOT** prove valid TMDS data or the Mac's actual output color encoding.
- IOKit EDID is 256 bytes, SHA-256 `13a90125fb31020f722bbf308adc50753a9e51e513865679a3c6b14c05d9ca25`, two blocks and CTA flag `0x77`, including YCbCr 4:2:2/4:4:4 capability advertisements. No earlier immutable raw EDID fingerprint was available for conclusive byte-by-byte change detection; sink EDID correctness is **not** proof of electrical signal lock.
- Original Blackmagic configuration reports **HDMI input (0x2)**, output connections `0xa`; no alternate analog input was selected. Desktop Video 10.4 `IDeckLinkAttributes` reports full duplex false, hardware bypass false and idle output capability true. Its modern capture passthrough setting is unavailable.
- The original first monitor plug-in showed the Sierra desktop for **1–2 seconds** according to the operator; then picture disappeared. This is **real historical human observation** but not a sustained return-video recording or evidence of full-duplex support.

## Old success-proving hardware diagnostic repeated — NO LOCK

On the actual silver, recompiled the exact original `src/decklink-mode-armed-no-stream.cpp` and ran after cleanly quitting the native app:

```text
stage=enable_video_input rc=0x00000000 requested=1080p30 pixel_format=2vuy
stage=mode_armed_status step=0 lock_rc=0x00000000 lock=no
stage=mode_armed_status step=1 lock_rc=0x00000000 lock=no
stage=mode_armed_status step=2 lock_rc=0x00000000 lock=no
stage=mode_armed_status step=3 lock_rc=0x00000000 lock=no
stage=disable_video_input rc=0x00000000
stage=complete exit=0 stream_started=0 physical_frame_captured=0
```

This exact diagnostic earlier reported `lock=yes` on samples 1, 2, 3 and was the basis of the accepted original physical capture. It now reports no signal, independently of the new recorder.

## Distinguishing source-negotiation from input client state

New source-owned `src/decklink-fixed-silver-input-lock-lifecycle.cpp`: queries original input lock, current input format, current output mode and busy status (where implemented). Then it holds **input armed, no stream** for **15 seconds**. A second run first calls `DisableVideoOutput` while nothing is playing to release any lingering output state. Both ended:

```text
stage=real_silver_input enable_input_rc=0x00000000 mode=Hp30 pixel=2vuy
stage=real_silver_input phase=armed_input sample=0..59 lock_rc=0x00000000 lock=NO
stage=real_silver_input result=NO_LOCK stable_lock=0
    locked_samples=0 unknown_samples=0 streams_started=0 frames_captured=0
```

`GetInt('cvim')` returned **Hp30** while input was armed, but that's the **selected input configuration**, not detected source. Unsupported `busy` or auto-detect queries were recorded with their error codes rather than interpreted as an active or idle device.

## Receiver format differential without changing the silver HDMI output

New source-owned `src/decklink-silver-fixed-source-receiver-modes.cpp` calls `DoesSupportVideoMode` then briefly arms each supported **input** format while the silver Mac's actual source remains **unchanged at 1080p30**:

- Original `Hp30` / **1080p30.000**
- Nearby `Hp29` / **1080p29.970**
- `Hp25` / **1080p25.000**
- `Hi59` / **1080i59.940**

**Result:** four supported input modes tested, **zero** modes reported lock, each disabled cleanly, **no StartStreams**. This excludes a simple alternative input frame-rate selection as the current fix; does not measure actual HDMI wire color coding.

## Direct frame-delivery differential despite false lock — still NO FRAMES

New source-owned `src/decklink-silver-bounded-unlocked-frame-flags.cpp`: based on the previously safe old-driver callback ABI, explicitly arms **original 1080p30** and, despite no lock status, attaches a callback and starts streams for only **1.5 seconds**. The callback only calls previously tested-safe `GetFlags`; it never calls the original crashing `GetStreamTime`, reads any frame bytes, or stores pixels.

Actual live silver result:

```text
stage=bounded_input_flags enable_rc=0x00000000 mode=1080p30
stage=bounded_input_flags prestart_lock_rc=0x00000000 locked=NO
stage=bounded_input_flags callback_rc=0x00000000
stage=bounded_input_flags start_streams_rc=0x00000000
stage=bounded_input_flags poststream_lock_rc=0x00000000 locked=NO
stage=bounded_input_flags stop_streams_rc=0x00000000
stage=bounded_input_flags detach_callback_rc=0x00000000
stage=bounded_input_flags disable_input_rc=0x00000000
stage=bounded_input_flags result=NO_FRAMES callbacks=0 nonnull=0
    valid_flags=0 no_source_flags=0 pixel_bytes_read=0
    vendor_GetStreamTime_called=0 recorded_frames=0
```

This is crucial: the failure is **not merely a false-negative `visl` status flag hiding valid frame callbacks**. Even when the driver returns success for `StartStreams`, it currently delivers **zero callbacks**.

## Reversible source-side HDMI timing retrain — did NOT restore capture

The earlier identical-mode `CGDisplaySetDisplayMode` reassertion had not restored input lock. New source-owned `tools/sierra/fixed_silver_hdmi_retrain.py` first verified the actual silver GPU offers both 1080p25 and its original 1080p30. In an isolated **0.7s temporary** experiment, with a process exit restoration guard, the original silver HDMI source requested 1080p25, then re-applied and independently verified the exact original 1080p30 `CGDisplayModeRef`:

```text
stage=silver_link_retrain action=brief_1080p25 cg_rc=0
    expected_hz=25.000 observed_hz=25.000 source_unchanged=1
stage=silver_link_retrain action=restore_exact_original_1080p30 cg_rc=0
    expected_hz=30.000 observed_hz=30.000 source_unchanged=1
stage=silver_link_retrain result=RESTORED_SOURCE_1080p30
```

The original BMD 1080p30 `visl` lock remained **false on all subsequent polls**. No mode or resolution change persisted, and the external HDMI source was **never substituted**. A source-owned `--restore-30-if-needed` recovery command is available but was not needed.

## Executed fail-closed end-to-end acceptance gate

New canonical script `scripts/sierra-hdmi-physical-roundtrip-gate.sh` implements:

1. `--prepare`: build the exact original accepted input diagnostic and native real-frame output recorder; compile source-owned independent Sierra Python 2.7 checksum auditor.
2. `--check`: verify silver Sierra 10.12.6, Macmini5,3, original Blackmagic 10.4 KEXT, active BMD HDMI at 1080p30, and 1080p30 YUV output support.
3. `--run`: exclusively release native GUI and run original `visl` input-lock gate. **If two consecutive genuine locks are absent, stop and return nonzero (42); do not start recording or playback.** If lock succeeds, run bounded 30 complete 1920×1080 raw UYVY frame recording into private 0700 folder, then stop input and replay saved raw frames 2× (60 real original HDMI OUT submissions). Independently read **all 124,416,000 raw bytes** and verify every SHA-256 against manifest, enforce increasing host timestamps, and require actual 60-frame vendor acknowledgments. The actual monitor optical signal remains a separate, unmeasured property.
4. Always reopen installed `Fountain Video.app` and leave sharing OFF.

**Real hardware gate result, executed on silver:**

```text
scenario_exit=42
stage=roundtrip_gate result=NO_INPUT_LOCK
stage=roundtrip_gate capture=NOT_STARTED record=NOT_CREATED
stage=roundtrip_gate playback=NOT_STARTED output_api=NOT_CLAIMED
```

The old genuine local Linux guest mailbox `/health` still reported `last_host_commit=17`; native Fountain Video was **restored and running**. No false new physical recording or output was declared.

The independent verifier `tools/sierra/verify_hdmi_roundtrip_recording.py` passed **four Python 3 unit tests** on the Ubuntu test machine, including a synthetic 30-frame reduced-size *verifier-only fixture*, corruption, missing frame, and missing actual playback acknowledgment. It was also byte-compiled with **actual Sierra Python 2.7**, and `--check` of the original-hardware acceptance gate passed. Synthetic verifier fixtures are NOT presented as physical Blackmagic frames. [Source-owned tests](../tests/test_private_hdmi_roundtrip_verifier.py).

## Open hardware boundary, not a software inference

The remaining problem is **physical HDMI input signal availability / clock or signal-format negotiation** on the fixed silver Mac HDMI OUT → Blackmagic HDMI IN interface, or original device input-receiver state. The monitor's original 1–2 second Sierra desktop sighting and later blinking are noteworthy, but do NOT individually explain the loss of signal.

The original Intensity Extreme specifications include 1080p30 and YUV 4:2:2 sampling and do not allow capture of protected HDCP HDMI input: https://www.bhphotovideo.com/c/product/824118-REG/Blackmagic_Design_BINTSTBEXT_Intensity_Extreme_HDMI_and.html . We have **NOT** measured the actual source's HDMI InfoFrame, actual RGB/YUV wire encoding, or HDCP electrical/authentication state. **Color-format incompatibility, HDCP, physical cable/link renegotiation or original receiver hardware state remain hypotheses, not demonstrated root causes.** Do not assert replacing the driver or using another HDMI source is a solution.

**Verdict:** original capture and playback functions separately established earlier; **fresh physical acquisition currently NO GO**, validated by independent 15s lock, mode differential and no-frame callback tests. Continuous monitor passthrough is not advertised by this original device. PR #10 must remain DRAFT, and the final silver hardware topology is unchanged.
