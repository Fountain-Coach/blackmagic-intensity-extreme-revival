# Physical HDMI loopback: silver Mac mini → Intensity Extreme HDMI IN

**Physical test:** 24 October 2026, original silver Intel Mac mini, macOS Sierra 10.12.6.

**Result:** HDMI sink identification and the host's 1080p30 output mode are witnessed. Actual Blackmagic video capture **FAILED / NOT ACCEPTED**. The legacy Desktop Video 10.4 user-space frame-receive library crashed after streaming was enabled. Do **not** re-run the crashing legacy input test automatically.

## 1. Source signal / EDID witness

The actual silver Mac mini's `system_profiler SPDisplaysDataType` reports:

    Displays:
      BMD HDMI:
        Resolution: 1920 x 1080 @ 30 Hz
        Pixel Depth: 32-Bit Color (ARGB8888)
        Main Display: Yes
        Mirror: Off
        Online: Yes
        Television: Yes

The kernel's display inventory also reports the same BMD HDMI EDID, `DisplayVendorID=8961`, `DisplayProductID=0`; the display name in EDID is `BMD HDMI`.

This is evidence that the Mac mini has negotiated an active video-output display mode against a sink identifying as BMD HDMI. It is **not** independent measurement of the HDMI electrical waveform, the Blackmagic receiver's input signal lock, or a captured frame.

Our separate `scripts/sierra-hdmi-loopback-readonly.sh` inventory ran successfully on that physical Mac after the failed capture:

    stage=hdmi_sink result=detected monitor_name=BMD_HDMI mode="1920 x 1080 @ 30 Hz" online=Yes main_display=Yes
    stage=hdmi_edid result=matched vendor=BMD
    stage=hdmi_capture result=not_tested source_lock=not_measured frame_count=not_measured
    stage=safety kernel_changes=0 pci_access=0 decklink_runtime_calls=0 display_mode_changes=0

**Do not change Mac mini display modes without a rollback plan**: BMD HDMI is the host's current main display. SSH access stayed available throughout the test. No MacBook changes or video-device driver installations occurred.

## 2. Correct capture mode and connector

The previously implemented Nikon-oriented automatic scanner omits exact `1080p30` (its six automatic selections are 720p50, 1080i50, 1080p25, 720p59.94, 1080i59.94, 1080p29.97). That is a software selection gap **for this Mac-mini source**, not proof of failed HDMI.

- Silver Mac output mode: **1920x1080, 30 Hz progressive**.
- DeckLink mode identifier: `0x48703330` (`Hp30`, 1080p30).
- The existing source-owned observer supports this mode with `--observe-1080p30`, and the Sierra Fountain Video GUI exposes it as a **manual** selection.
- Intensity Extreme's documented hardware input format list includes 1080p30. External spec: https://www.bhphotovideo.com/c/product/824118-REG/Blackmagic_Design_BINTSTBEXT_Intensity_Extreme_HDMI_and.html
- Prior hardware configuration inspection selected the HDMI input connector (`cfg=input status=0x00000000 value=0x2`).

**Important:** The card uses the original Desktop Video 10.4 driver. The independently authored PCI/KEXT experiment has never been loaded. The old Blackmagic CoreMediaIO plugin remains isolated.

## 3. Actual capture attempt and observed failure

Attempted a single direct user-space capture at exactly the Mac output mode, using the already-built bounded diagnostic `/tmp/fc-decklink-input-observer-with-evidence --observe-1080p30`. The native Fountain Video app remained in its **synthetic** source mode and did not own an active DeckLink input stream.

    stage=requested_video_mode code=0x48703330
    stage=input_interface status=0x00000000 present=1
    stage=set_callback status=0x00000000
    stage=enable_input status=0x00000000
    stage=start_streams status=0x00000000
    stage=queue_frames rc=0x00000000 queued=0
    stage=status_interface rc=0x00000000 present=1
    stage=signal_lock rc=0x00000000 locked=0 detected_mode_rc=0x80000001 mode=0x0
    ... process exited with SIGSEGV 139 ...

The first signal-lock query returned **locked=0** and failed to return a detected input mode. No bona-fide frame bytes, timestamp or checksum were emitted as acceptance evidence.

The host created a crash report for the observer at 08:37 local time. The captured crash stack identifies a problem during an incoming frame's **vendor-owned destruction/release**, on the vendor's own notification thread:

    EXC_BAD_ACCESS (SIGSEGV), EXC_I386_GPFLT
    Thread 5:
      CDeckLinkVideoInputFrame::~CDeckLinkVideoInputFrame()
      CDeckLinkVideoInputFrame::Release()
      CDeckLinkInput::inputFrameArrivalCallack()
      CoreFoundation / vendor notification run loop

The source-owned diagnostic's main thread was sleeping inside its fixed three-second observation window. A static read-only symbol/assembler inspection of the locally installed 10.4 library suggests the fault occurred at a virtual release call through an invalid internal pointer. That narrows the symptom to **the original Blackmagic frame lifecycle**, but cannot establish whether it is caused by defective vendor software, an older platform ABI incompatibility, corruption originating from the card/driver, or our consumer's callback behavior. It is **not evidence of a repaired physical device**.

No proprietary driver binary, firmware, complete crash dump or reverse-engineered vendor code is committed to Git. The full crash report remains privately on the authorized Sierra host under its local DiagnosticReports directory.

## 4. Immediate containment and result

After the process crash, both original Blackmagic kernel modules were still loaded, no independent KEXT was installed, and the native Fountain Video app remained running and streaming its **synthetic** test frames to Browser Scratch.

The source-owned safe EDID witness returned exit 0 with BMD HDMI still connected. We **did not**:
- change the host display mode or desktop;
- swap the MacBook display/capture configuration;
- alter HDMI firmware, PCI BARs, kernel modules or the Sierra boot environment;
- repeat the crashing input test, launch another potentially competing capture frontend, or claim frames based on monitor EDID alone.

## 5. Acceptance gate and next investigation

**HDMI display negotiation: PASS** (BMD HDMI active sink, 1080p30).

**Vendor API input enable/start: PASS at API-call level**, but that is *not* frame acquisition.

**Video input lock: FAIL / 0 at the first sampled moment.**

**Genuine captured pixels + stream timestamp: NOT WITNESSED.**

**Software stability while physical input runs: FAIL** (vendor library SIGSEGV).

**Physical Intensity Extreme revival: OPEN.**

Do not add this unstable path to the automatic video simulator acceptance suite. Prefer offline crash analysis and an independently witnessed, contained recovery environment before another legacy 10.4 capture test. The independently developed Fountain Video synthetic application/browser frame scratch contract remains accepted **only for synthetic streaming**; a frame labeled `decklink-observed` must never be asserted without real data.

A future physical acceptance witness must establish a lock, receive one or more actual non-no-source frames, extract valid pixel bytes and timestamps, and demonstrate a safe stream shutdown without process crash. Then inspect Browser Scratch for the new non-synthetic source label while maintaining separate source provenance.

Source-owned inventory tool: [sierra-hdmi-loopback-readonly.sh](../scripts/sierra-hdmi-loopback-readonly.sh). Existing manual capture source: [decklink-input-observer.cpp](../src/decklink-input-observer.cpp). Physical task: issue #8.

## 24 October follow-up: idle, read-only status query

A new source-owned `src/decklink-signal-status-readonly.cpp` was compiled and run on the actual Sierra 10.12.6 host **without enabling video input or starting streams**. It only enumerates the original DeckLink device and reads IDeckLinkStatus:

    stage=readonly_status status_interface=0x00000000 present=1
    stage=readonly_input_signal result=observed queried_without_streams=1 lock_query_rc=0x80000008 locked=0 mode_query_rc=0x80000001 mode=0x0 real_frame_verified=0
    stage=safety vendor_input_enable=0 start_streams=0 framebuffer_read=0 vendor_driver_changes=0 pci_mmio=0 firmware_update=0

The status interface exists; **the idle lock query returned an error** (`0x80000008`) rather than a trustworthy positive or negative lock measurement. An uninitialized boolean printed as zero is *not* a verified unlocked receiver. The detected-mode query also failed. Do **not** treat this as new physical-capture failure evidence or re-enter the crashing stream solely to populate this field. The prior active-stream attempt remains the last hazardous real-input test; capture is NOT ACCEPTED.

## Original Intensity Extreme source-encoding gate (24 October)

Model-specific product specifications for the *original* Blackmagic **Intensity Extreme** BINTSTBEXT report HDMI sampling **4:2:2** and color space **YUV 4:2:2**, with support for 1080p30. Do not substitute current Intensity Pro 4K or UltraStudio HDMI-RGB capabilities for this legacy model. Sources:

- https://www.bhphotovideo.com/c/product/824118-REG/Blackmagic_Design_BINTSTBEXT_Intensity_Extreme_HDMI_and.html
- https://www.expandore.com/blackmagic/blackmagic-intensity-extreme.htm

**Crucial unmeasured property:** The *actual HDMI wire encoding* emitted by the silver Mac at its BMD HDMI 1920x1080@30 display is **not verified**. CoreGraphics ARGB8888 describes a framebuffer, **not** the outbound HDMI 4:4:4 RGB/4:2:2 YUV InfoFrame. The BMD EDID advertising YCbCr 422 does not prove that macOS selected it. Thus an unsupported RGB output is a *candidate explanation* for the receiver's lack of lock, **not a proven cause**, and a driver reinstall would not necessarily fix it.

**Next decisive non-destructive physical acceptance:** Supply a known unprotected HDMI generator/camera with independently verified **YUV 4:2:2** at an exact supported mode (e.g. 1080p30 or 720p60), preferably checked on a working display, into the original Intensity Extreme's **HDMI IN**. Do not change the Mac's primary display, plug into the card's HDMI OUT, or evade HDCP. Only then attempt a bounded capture in a recoverable driver environment, witnessing input lock and genuine distinct pixels, timestamps and safe release. The source alone cannot be replugged by software or authenticated by EDID.
