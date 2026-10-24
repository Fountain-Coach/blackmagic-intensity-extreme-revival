# Source-aware HDMI capture acceptance and independent A124 read contract

Date: 24 October 2026. Equipment: Intensity Extreme on 2011 Intel Mac mini (macOS Sierra 10.12.6, Desktop Video 10.4); authorized Apple Silicon MacBook and Ubuntu analysis host.

## Critical topology change

A live \`system_profiler SPDisplaysDataType\` examination found **U28E590** as the active external display on BOTH computers:

| Source/computer | Display endpoint reported | Interpretation |
| --- | --- | --- |
| Modern MacBook | U28E590, source UI timing 1920x1080 @ 30 Hz | **Not** BMD HDMI EDID |
| Sierra Mac mini | U28E590, 1920x1080 @ 50 Hz | **Not** BMD HDMI EDID |

On 21 October the MacBook was separately documented with the **BMD HDMI** endpoint at 1920x1080 @30Hz. That previous cabling/source observation is **no longer live evidence**. Neither machine's current display enumeration proves a video source reaches the capture card. A third-party HDMI source cannot be excluded from host display enumeration alone.

## New cross-version source preflight

[\`scripts/hdmi-source-preflight.py\`](../scripts/hdmi-source-preflight.py) reads structured \`system_profiler -xml\` (supported by Sierra's Python 2.7 and modern macOS Python 3). It inspects display names, online state and exact current timing, without logging EDID bytes/serials or altering the source video mode. If **exactly one BMD HDMI** sink is not active at the selected supported video timing, it exits nonzero. It does not infer source HDCP/protection state from EDID.

Physical checks:

\`\`\`text
Modern: label=U28E590 timing=1920 x 1080 @ 30.00Hz
stage=source_preflight result=bmd_endpoint_not_witnessed requested_mode=1080p30
modern_source_exit=2

Sierra: label=U28E590 timing=1920 x 1080 @ 50 Hz
stage=source_preflight result=bmd_endpoint_not_witnessed requested_mode=1080p30
sierra_source_exit=2
\`\`\`

Ten synthetic source-preflight tests passed on Ubuntu, covering positive BMD EDID, wrong monitor, mismatched/29.97 timing, offline, unknown or ambiguous BMD sink and mode parsing.

## Improved bounded native observer on Sierra

The [native DeckLink observer](../src/decklink-input-observer.cpp) now additionally records **post-observation signal lock**, and for the first frame without \`bmdFrameHasNoInputSource\`, a bounded fingerprint of at most 4096 bytes, width, height, stride and the video stream time/duration. It never persists raw video frames.

Read-only use of the vendor API, three-second 1080p30 test on the actual Sierra Mac mini:

\`\`\`text
stage=start_streams status=0x00000000
stage=signal_lock rc=0x00000000 locked=0 detected_mode_rc=0x80000001 mode=0x0
stage=signal_lock_final rc=0x00000000 locked=0 detected_mode_rc=0x80000001 mode=0x0
stage=stop_streams status=0x00000000
stage=remove_callback status=0x00000000
stage=disable_input status=0x00000000
stage=counts frames=84 valid=0 no_source=84 format_changes=0 observer_refcount=1
stage=first_valid_frame bytes_ok=0 width=0 height=0 row_bytes=0 sample_size=0 sample_fnv32=0x00000000 timestamp_ok=0 time=0 duration=0 timescale=30000
stage=complete exit=0
observer_exit=0
\`\`\`

**No real source frame captured.** Note how successful vendor API stream start/stop and 84 callbacks still give zero valid physical pixels.

Five self-authored synthetic callback fixtures passed on both the Sierra Mac and modern Mac; they test frames flagged no-source, successful bounded buffer fingerprint and stream time, missing pixels and missing stream time. The physical valid-frame branch has **not** been exercised.

## Capture witness command

The [source-host acceptance runner](../scripts/accept-bmd-hdmi-capture.sh) runs from the MacBook checkout:

\`\`\`sh
sh scripts/accept-bmd-hdmi-capture.sh 1080p30
\`\`\`

Before accessing Sierra, it requires the MacBook to witness **BMD HDMI** at exactly 1080p30 (1080p25 and 720p60 also supported). When it passes, the runner temporarily compiles the observer on the Sierra Mac mini, records at most three seconds of metadata-only capture, requires final receiver lock, at least one real-frame callback, actual frame buffer access, a valid stream timestamp and clean teardown, and deletes its temporary build directory. No kernel driver or firmware installation.

**Live result:** the runner exited **2**, \`blocked_by_source_preflight\`; it did not start a Sierra capture. Twelve synthetic log-validator tests passed on Ubuntu, including explicit rejection of the observed 84-callback no-source log.

Even a passing runner is only an API-level frame candidate: independently establish source authenticity, format, absence of content protection and actual picture integrity before declaring physical acceptance. Do not fabricate user media or count synthetic fixture logs as video capture.

## Independently authored A124 register contract

\`src/intensity-register-contract.{h,c}\` defines a read-only injected \`read32\` interface and two static, model-specific BAR0 offsets (**0x224** candidate firmware version, masked to low byte; **0x40** candidate input status). It combines the existing firmware and input-status gates, rejects unreadable/wrong version and closed gates, and never maps MMIO or enables DMA itself.

**Twelve synthetic register-contract cases passed under strict warnings on Sierra and Ubuntu**; the mock is the only register provider. The actual hardware status and firmware value remain unknown. This code is a testable future driver component, NOT an operating replacement kernel driver.

## Hardware-state preservation and next acceptance

Both original Blackmagic 10.4 kernel drivers remain loaded. The independent extension is not loaded; the crash-prone system-wide CoreMediaIO plug-in remains isolated. A separate, verified recoverable boot environment is still unavailable.

For a **MacBook-sourced** physical test, reconnect its HDMI output directly to the Intensity Extreme **HDMI IN** (not the THUNDERDOCK2's output or the card's own HDMI OUT), set a supported precise source timing such as 1080p30, confirm \`BMD HDMI\` appears on the MacBook, and run the acceptance command above. No remote command can move the cable physically.

**Do not** infer faulty HDMI receiver hardware solely from the present no-source capture run.
