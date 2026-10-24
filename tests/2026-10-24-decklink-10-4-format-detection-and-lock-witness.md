# Original Intensity Extreme: input format detection unsupported; real signal lock still absent

**Date:** 24 October 2026. **Physical host:** Sierra 10.12.6 silver Macmini5,3 and original Blackmagic Desktop Video 10.4 / Intensity Extreme. **Fixed wiring:** silver Mac HDMI OUT to original Intensity Extreme HDMI IN; Intensity HDMI OUT empty; Thunderbolt unchanged.

**Verdict:** The card's physical DeckLink 10.4 attribute BMDDeckLinkSupportsInputFormatDetection (four-character ID **infd**) is **false**. Therefore, errors reading bmdDeckLinkStatusDetectedVideoInputMode (**dvim**) were expected and must not be used as a proof of no signal or a mandatory prerequisite for a signal-lock candidate. The separate BMDDeckLinkStatusVideoInputSignalLocked (**visl**) flag is the valid physical signal-lock query. A new capability-aware 1080p30 test independently read that flag **eight times, all successful, none locked**. No actual frame has been accepted.

## Official SDK contract and actual device

[Blackmagic's DeckLink Status ID definition](https://sdk-doc.blackmagicdesign.com/decklink-sdk/DeckLinkAPI/enums/bmddecklinkstatusid.html) specifically limits *detected* video input mode to devices supporting input format detection. Its [automatic-format-detection guide](https://sdk-doc.blackmagicdesign.com/decklink-sdk/HighLevel/automodedetection.html) requires checking the hardware capability before enabling this feature. The separate [synchronized-capture workflow](https://sdk-doc.blackmagicdesign.com/decklink-sdk/HighLevel/synchronized.html) explicitly queries the **input signal-lock flag after EnableVideoInput and before StartStreams**, so a bounded no-stream signal-lock check is valid as a diagnostic.

The original source-owned [device enumerator](../src/decklink-enumerate.cpp) now reads the live 10.4 IDeckLinkAttributes::GetFlag on **infd** without changing device settings:

    stage=device_model status=ok model=Intensity Extreme
    stage=query_attributes status=0x00000000 present=1
    stage=attribute_flag name=input_format_detection status=0x00000000 supported=0
    stage=mode interface=input code=0x48703330 ... name=1080p30
    stage=mode interface=input code=0x68703630 ... name=720p60
    stage=complete enumerated_devices=1

This is direct evidence for this physical original model and installed vendor library, not an inference from a newer device.

## Diagnostic correction

The earlier [input-lock probe](../src/decklink-input-lock-probe.cpp) incorrectly required BOTH >=2 signal-locked queries and >=2 successful **detected-mode** queries. It would have rejected a candidate even if the original card's true signal-lock flag were positive, because this device does not support automatic format detection.

Corrections:
- Discover and report **input_format_detection supported=0** directly through physical IDeckLinkAttributes.
- Do **not** request or require detected-mode status for this original device.
- Classify only the explicit **visl** signal-lock flag. Eight successful queries with zero/one locked => **NO_LOCK**; two or more positive samples => **LOCK_CANDIDATE**, explicitly not genuine pixel/frame acceptance; any query failures => **STATUS_UNKNOWN**.
- The [pure signal-lock contract](../src/decklink-signal-lock-contract.h) and its [8 synthetic tests](decklink_signal_lock_contract_test.cpp) are source-owned and compile/pass on **Sierra, modern macOS and Ubuntu**. They contain no hardware calls, streams or UI mode changes.

## Corrected test on the original physical rig

Compiled the new no-stream diagnostic on the silver Mac with strict Sierra Clang++11. Its preflight confirmed original BMD HDMI 1080p30 before the vendor runtime was opened. Executed the exact source-gated physical test:

    stage=source result=BMD_HDMI active_width=1920 active_height=1080 selected_rate_hz=30.000000 active_mode_match=1
    stage=input_connector rc=0x00000000 value=0x2 expected=0x2
    stage=capability input_format_detection rc=0x00000000 supported=0
    stage=enable_input mode=1080p30 pixel_buffer=2vuy rc=0x00000000
    stage=receiver sample=0..7 lock_rc=0x00000000 locked=NO
    stage=receiver result=NO_LOCK locked_samples=0 successful_lock_queries=8 input_format_detection_supported=0 detected_mode_not_required=1 real_frame_acceptance=NOT_TESTED
    stage=disable_input rc=0x00000000
    stage=complete exit=32 callback_installed=0 stream_started=0 captured_frames=0 display_mode_changes=0 driver_firmware_changes=0

A diagnostic exit 32 means **NO_LOCK**, not a crash. The hardware was not streamed, callbacks were not installed, and the known original 10.4 video frame-destruction crash was not exercised.

Post-test physical inspection: BMD HDMI main display still online 1920x1080 at UI 30 Hz, original automatic cyuv=0x10000000 and cmod=0x100 YCbCr444 preserved.

## Interpretation

The mode detection **error is not a signal failure** on this old model; that diagnostic gate is fixed. However, the independent signal-lock flag still reported false eight out of eight times. Earlier bounded physical streaming delivered only frames tagged **bmdFrameHasNoInputSource**, corroborating that no useful real pixels had been acquired, though this frame path is currently guarded because of a separately witnessed vendor frame-release SIGSEGV.

**Next engineering target:** Analyze the **original 10.4 frame-delivery and input-init lifecycle**, and the feasibility of safely isolating the proprietary frame destructor crash, before another streaming attempt. The source-side requested 74.25 MHz versus driver-reported 73.97953 MHz remains an unproven physical cause. RGB/YCbCr wire encoding is still not directly measured and userland forcing 4:2:2 via IOGraphics has been retired as invalid. Do not replug hardware, install alternate drivers or bypass capture crash guards without a boot-tested rollback and independent witness.

**D4 genuine input-frame acceptance remains OPEN.**
