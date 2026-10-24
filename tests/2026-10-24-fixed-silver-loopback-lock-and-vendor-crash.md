# 24 October 2026 — fixed silver HDMI loopback locks, original driver 10.4 crashes on frame arrival

**User-owned physical test fixture, final and non-negotiable:** the same silver Intel Macmini5,3 runs Sierra 10.12.6 and sends its built-in Intel HD Graphics 3000 **HDMI OUT straight to the original Blackmagic Intensity Extreme HDMI IN**; the Intensity Extreme returns to the **same silver Mac via Thunderbolt 1 / Comdis THUNDERDOCK2**. Linux/Docker remains downstream of the Sierra capture adapter. No camera, second test generator, alternative Mac or rearrangement of cables is part of this project's acceptance.

## Physical source verified

On the silver Mac, `system_profiler SPDisplaysDataType` continues showing `BMD HDMI`, online main display at `1920 x 1080 @ 30 Hz`. The vendor 10.4 KEXT (`com.blackmagic-design.desktopvideo.iokit.driver`) and original `DeckLinkAPI.framework` remain loaded and unmodified.

A new source-owned `src/decklink-mode-armed-no-stream.cpp`, compiled with Sierra's local clang/Xcode, **briefly enabled input mode `Hp30` (1080p30) / `2vuy`**, polled `IDeckLinkStatus::GetFlag('visl')` four times, and disabled the input **without installing a callback or calling StartStreams**. The test actually ran on the silver Mac:

    stage=enable_video_input rc=0x00000000 requested=1080p30 pixel_format=2vuy
    stage=mode_armed_status step=0 lock_rc=0x00000000 lock=no detected_rc=0x80000001
    stage=mode_armed_status step=1 lock_rc=0x00000000 lock=yes detected_rc=0x80000001
    stage=mode_armed_status step=2 lock_rc=0x00000000 lock=yes detected_rc=0x80000001
    stage=mode_armed_status step=3 lock_rc=0x00000000 lock=yes detected_rc=0x80000001
    stage=disable_video_input rc=0x00000000
    stage=complete exit=0 stream_started=0 physical_frame_captured=0

**Result: INPUT SIGNAL LOCK ACCEPTED at the vendor API level on the actual fixed silver HDMI loopback.** This is substantially stronger than EDID recognition. The `detected_mode_rc=0x80000001` is a separate unsupported/failed autodetection query on a device previously shown not to support format autodetection; it does not invalidate successful signal-lock readings. We have NOT measured the actual on-wire RGB/YUV encoding or independently decoded pixels.

## Lock-gated capture attempt

A separate `src/decklink-lock-gated-input-observer.cpp` uses the same existing COM callback ABI, but **does not start streams until two successive successful `visl=true` queries**. It was compiled on the silver Mac and launched with an eight-second parent alarm under an isolated user process. The current source mode remained 1080p30, and no KEXT, device firmware, boot preference, display mode or cable was modified.

    stage=enable_input status=0x00000000
    stage=prestream_lock sample=0 rc=0x00000000 locked=no
    stage=prestream_lock sample=1 rc=0x00000000 locked=yes
    stage=prestream_lock sample=2 rc=0x00000000 locked=yes
    stage=prestream_gate result=PASS stable_lock=1
    stage=set_callback status=0x00000000
    stage=start_streams status=0x00000000
    stage=signal_lock rc=0x00000000 locked=1 detected_mode_rc=0x80000001
    process exit=139 (SIGSEGV)

Sierra host continued running 10.12.6; the original driver 10.4 remained loaded. The privately retained **new** crash report `~/Library/Logs/DiagnosticReports/fc-lock-gated-input-observer_2026-10-24-142410_Uwes-Mac-mini.crash` repeats the **same** library fault site as the prior test, on the Blackmagic's own notification thread:

    CDeckLinkVideoInputFrame::~CDeckLinkVideoInputFrame() + 132
    CDeckLinkVideoInputFrame::Release() + 46
    CDeckLinkInput::inputFrameArrivalCallack() + 1943

**Result: hardware input signal LOCKED, but physical frame acquisition NOT ACCEPTED** because vendor frame processing crashes; no completed sample, frame bytes, valid timestamp or clean stream stop was witnessed. The occurrence inside the vendor's destructor does not by itself prove the vendor library was solely responsible; consumer ABI/format/lifetime and driver/DMA interaction still require differential evidence.

## Consequence and next test

The HDMI rig works well enough for the receiver to report stable input lock and cannot reasonably be dismissed as an absent external source. **Never ask the operator to replace the silver Mac HDMI source with an independent camera** for this acceptance. Work solely within this fixed electrical topology.

Next safe path: inspect the installed historical Black Syphon r4 / VVBlackmagicKit adapter and its exact callback conventions versus our new observer, plus the original 10.4 vendor API's owned-frame memory lifecycle. The signed Sierra-compatible Desktop Video 10.11.2 and exact 10.4 rollback installer are staged and audited, but upgrading kernel components is a separate change requiring boot-tested recovery, not an automatic workaround. An already-working Linux/Docker scratch receiver can accept authenticated, exactly hashed synthetic frames from Sierra; those frames must **not** be represented as physical Blackmagic output.

SDK signal-lock semantics: https://sdk-doc.blackmagicdesign.com/decklink-sdk/DeckLinkAPI/enums/bmddecklinkstatusid.html
