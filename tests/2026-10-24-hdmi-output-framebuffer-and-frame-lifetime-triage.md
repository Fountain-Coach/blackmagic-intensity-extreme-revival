# Physical HDMI triage: actual output framebuffer, frame release, callback ABI

**Date:** 24 October 2026  
**Host:** Silver Intel Mac mini (Macmini5,3), Sierra 10.12.6, original Thunderbolt Intensity Extreme.  
**Physical acceptance:** STILL OPEN. BMD sink handshake and changing source-side framebuffer are witnessed, but no actual Blackmagic input lock or video frame has been accepted.

## HDMI source-side witness

Actual Sierra display inventory, confirmed after the legacy crash:

    BMD HDMI
    1920 x 1080 @ 30 Hz
    Main Display: Yes
    Online: Yes
    I/O Registry DisplayVendorID: 8961

The live Thunderbolt card remains PCI vendor/device 0xBDBD:0xA124 and the existing input connector configuration selects HDMI.

New source-owned tool: tools/sierra/hdmi-output-framebuffer-witness.c

It locates the actual BMD display using public CoreGraphics display enumerations, acquires two **real OS framebuffer snapshots**, and hashes the raw pixels in memory one second apart. It does not persist or transmit desktop imagery or invoke DeckLink.

Witness on the silver Mac:

    stage=hdmi_output_display result=found vendor=8961 width=1920 height=1080 rate_hz=30.000 main=1 online=1
    stage=hdmi_framebuffer_reference result=observed width=1920 height=1080 byte_count=8294400
    stage=hdmi_framebuffer_change result=changed interval_s=1.0 source=macos_framebuffer
    stage=hdmi_receiver_capture result=not_tested input_lock=unknown received_frame_bytes=unknown

Different SHA-256 hashes establish that **the Mac's display framebuffer is actually changing**. This is more than an EDID-only fixture, but it does NOT prove HDMI electrical transmission, color format on the wire, receiver signal lock, or a captured hardware frame.

## Legacy vendor frame-destruction crash

The single physical-input test requested exact mode 0x48703330 (1080p30), matching the Mac's current output. Its DeckLink SetCallback, EnableVideoInput and StartStreams calls each returned S_OK. The initial vendor signal-lock query reported false; detected-mode retrieval returned error 0x80000001. The observer then terminated with SIGSEGV on the **vendor notification thread** before valid received pixels could be verified.

Correct installed kernel-driver identity has also been verified using the **actual loaded bundle**, not the separate BlackmagicIO extension:

    /Library/Extensions/DeckLink_Driver.kext/Contents/Info.plist

- Bundle ID: com.blackmagic-design.desktopvideo.iokit.driver
- Version: 10.4, bundled March 30, 2015
- Built with SDK: macosx10.9 (Xcode 5.1.1-era toolchain)
- Exact `IOKitPersonalities/DeckLink_Driver/IONameMatch` contains `pcibdbd,a124` at list entry 27.
- Actual original kernel executable SHA-256: c4ec945aa46d07123788e66c219203026d1dcddffad20d1a55b8b5b4943d3780

This proves Desktop Video 10.4 was **intended to attach to the Intensity Extreme PCI identity**; the problem is not simply that an unrelated card's driver loaded. It does not establish Sierra support or valid input-frame memory.

Installed user-space Blackmagic library SHA-256:

    6972a2b025cf333a6bbf2c8a1199af89a9f85c1ccfcb17d2055876251c558137

Original Desktop Video 10.4 (2015 era) is running on Sierra (2016 era). Chronological mismatch is a driver-compatibility concern, not an independently proven root cause.

Private, locally retained crash report:

    ~/Library/Logs/DiagnosticReports/
    fc-decklink-input-observer-with-evidence_2026-10-24-083717_Uwes-Mac-mini.crash

The crash stack enters CDeckLinkInput::inputFrameArrivalCallack, then CDeckLinkVideoInputFrame::Release, then its destructor. A **read-only** disassembly of that exact locally installed x86_64 library resolves the crash PC to image-relative address 0x26e98. The destructor reads a non-null frame member at offset +0xB0, loads that member's virtual-method pointer, then dispatches through its vtable at offset +0x38. The saved vtable pointer is 0x2000000000000000, which is not a valid method table.

This locates the invalid cleanup dependency but does **not** establish whether the corruption originated in vendor user space, kernel interactions, format negotiation or our real-device callback. The observer's main thread was still sleeping within its bounded capture interval; the failure was not in our StopStreams cleanup.

No proprietary executables, complete crash dump, or decompiler output were published.

## Source-owned callback ABI validation (without a vendor frame)

New test: tests/mock-decklink-callback-abi.cpp

The test compiles the original src/decklink-input-observer.cpp into a standalone executable with its hardware-opening main function renamed and **not called**. It supplies its own COM-style fake video frame with a correct vtable and exercises:

- interface lookup and reference counts;
- no-source flags;
- valid packed synthetic video, dimensions and strides;
- valid GetBytes plus independently calculated FNV32 sample hash;
- format-change notifications;
- timeline at scale 30,000;
- proper handling of a borrowed frame without AddRef or Release by our callback.

Witness from **actual Sierra Xcode 9.2 toolchain**:

    stage=decklink_callback_mock result=pass tests=12 queried=1 no_source=1 valid_video=2 sample_fnv32=0x31936c95 timebase=30000 borrowed_release_calls=0 hardware_access=0 vendor_library_loaded=0

It also passed after rebuilding with **AddressSanitizer and UndefinedBehaviorSanitizer**. These tests validate our synthetic ABI behavior, not the proprietary implementation's object lifetime.

Offline inspection of the historically working Black Syphon wrapper shows its VVBMInputCallback forwards the vendor frame into a VVBMInput processing method. It still depends on the original DeckLink runtime; no independent replacement of its frame-lifetime machinery was established.

## Reproducible noninvasive physical preflight

A new one-command tool runs on the physical Sierra Mac:

    sh scripts/sierra-hdmi-loopback-preflight.sh

It verifies, in one run: the BMD HDMI EDID and current output timing; two live changing framebuffer hashes; our observer against its fake DeckLink frame; installed vendor driver 10.4; and the retained real capture-crash signature. It **does not start capture or call the vendor runtime**.

Recorded exit 0, with:

    stage=hdmi_sink result=detected monitor_name=BMD_HDMI mode="1920 x 1080 @ 30 Hz" online=Yes main_display=Yes
    stage=hdmi_framebuffer_change result=changed interval_s=1.0 source=macos_framebuffer
    stage=decklink_callback_mock result=pass tests=12 ... vendor_library_loaded=0
    stage=installed_blackmagic result=detected version=10.4
    stage=capture_crash_evidence result=present vendor_frame_release=invalid
    stage=physical_capture result=NOT_ACCEPTED signal_lock=unverified real_frame=not_witnessed
    stage=hdmi_preflight result=pass edid=1 framebuffer=1 callback_mock=1 crash_evidence=1 vendor_capture_calls=0 driver_modifications=0

**Preflight PASS is not physical video capture PASS.**

## Native application crash containment, installed

The real Sierra Fountain Video.app now blocks inadvertent re-entry into the known-crashing legacy capture path before destroying the ongoing software source. Selecting hardware and pressing Start without an explicit user default produces an informative inline status while the virtual video and browser-scratch session continue. The guard is not a kernel modification or permanent hardware disablement.

Default user preference key: FountainVideoAllowLegacyCapture (absent/false on the host). It is deliberately reversible for a future controlled test on a recovery-verified host. Do NOT enable it on this current boot merely to repeat the crash.

Native self-test after building:

    stage=app_window result=pass virtual_frame=1 physical_crash_guard=1
    stage=scratch_jpeg result=pass source=synthetic width=640 height=360 jpeg_bytes=8723 network_access=0

The protected app was rebuilt and deployed to /Users/bene/Applications/Fountain Video.app and relaunched. The Ubuntu scratch receiver continued receiving SYNTHETIC frames, with a new sender epoch and source_online=true. Both original Blackmagic 10.4 kernel modules remained loaded; independent kernel prototype remains unloaded.

## Next physical acceptance

What is **still needed**: a captured input lock, a real non-no-source frame with valid changing pixel bytes and hardware-derived timestamps, safe repeatability and shutdown without a vendor crash, and ultimately a non-synthetic browser scratch frame from the actual Blackmagic input.

The device cannot be declared revived simply because the host display pixels change, a mock receives frames, or an unstable vendor API returned S_OK for StartStreams.

**Next driver task:** Obtain an authentic later 10.x Blackmagic Desktop Video installer for OFFLINE inspection first; verify Sierra compatibility and support for the original model 0xA124 before considering an isolated driver trial. A boot-tested, removable recovery installation remains missing. Do not overwrite current driver, reformat the populated Fresh volume or disable SIP. If a compatible stable vendor revision cannot be established, continue the independent model-specific driver work on an isolated boot environment.

Source scope: Git-owned tests and tools above. Hardware issue #8 remains open. No GitHub CI, no public deployment of private desktop pixels.
