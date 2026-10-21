# HDMI self-loop diagnostic — 21 October 2026

**Status:** Video receiver **not locked** during an active, manually matched capture test. The device remains enumerated, and its native API correctly starts and stops streams. This is **not proof of defective capture hardware**.

## Physical arrangement

The owner confirms the **Mac mini's HDMI output is connected directly to the Intensity Extreme's HDMI input**. The Mac mini is also the capture host over the i-tec/Comdis Thunderbolt dock, forming a video self-loop.

On this host, macOS `SPDisplaysDataType` reports an online primary display named `BMD HDMI` at **1920×1080, 30 Hz**. This confirms that macOS recognizes an HDMI display endpoint; it does *not* independently prove that video payload is arriving at the receiver.

The read-only legacy DeckLink `IDeckLinkConfiguration_v10_4` query returned:

```text
configuration_interface_status=0x00000000 present=1
cfg=input status=0x00000000 value=0x2
cfg=output status=0x00000000 value=0xa
```

The **input configuration** is HDMI (bit mask `0x2`), so connector selection is not the explanation. The current output-configuration mask is recorded without interpretation because it is not material to this test. The `IDeckLinkAttributes` read-only query of `BMDDeckLinkSupportsInputFormatDetection` returned `status=0x00000000 value=0`, confirming that auto-detection cannot be relied on for this unit/driver.

## 1080p30 test matched to source timing

The native [bounded input observer](../src/decklink-input-observer.cpp) was rebuilt for **1080p30, 8-bit YUV**. This is one of the enumerated video formats; source timing was confirmed on the source host before attempting capture.

```sh
./decklink-input-observer --observe-1080p30
```

Observed output:

```text
stage=requested_video_mode code=0x48703330
stage=input_interface status=0x00000000 present=1
stage=set_callback status=0x00000000
stage=enable_input status=0x00000000
stage=start_streams status=0x00000000
stage=queue_frames rc=0x00000000 queued=0
stage=status_interface rc=0x00000000 present=1
stage=signal_lock rc=0x00000000 locked=0 detected_mode_rc=0x80000001 mode=0x0
stage=stop_streams status=0x00000000
stage=remove_callback status=0x00000000
stage=disable_input status=0x00000000
stage=counts frames=0 valid=0 no_source=0 format_changes=0 observer_refcount=1
stage=complete exit=0
```

**Interpretation:** The live receiver status explicitly says **no input signal lock** while capture is running. The SDK's detected-video-mode query did not yield a mode. All capture lifecycle calls succeeded but no frames arrived. This is stronger diagnostic evidence than the earlier mismatched 1080p25 probe, and still not sufficient to identify whether the culprit is incoming HDMI encoding/protection, cable, device receiver, timing, or a lower-level software issue.

## Alternative HDMI timing — reversible 720p60

The macOS source's active display list included **1280×720 at 60 Hz**. A custom local test temporarily switched the HDMI output to 720p60 and ran the input observer with a matching 720p60 mode:

```text
stage=original_mode 1920x1080 30.000Hz
stage=switch_to_720p60 status=0
...
stage=enable_input status=0x00000000
stage=start_streams status=0x00000000
...
stage=counts frames=0 valid=0 no_source=0 format_changes=0 observer_refcount=1
...
stage=restore_display status=0
```

**Result:** No frames at 720p60 either. The test restored **1920×1080 at 30 Hz** with a successful return code. Signal-lock status was not queried during the 720p60 run, so the actual lock state for 720p60 remains unverified. No permanent display preference or driver/firmware setting was deliberately changed.

## Next decisive experiment

1. Use a **known, independently verified, unprotected HDMI test source** that outputs one of the documented formats, preferably 720p60 or 1080p30, and ensure an appropriate HDMI cable. A camera/test-pattern generator is suitable if available. Do not circumvent copy protection.
2. Repeat the same bounded observer and signal-lock check, with matching explicit video mode.
3. If signal lock becomes true and frames arrive, isolate a Mac mini output-specific problem (RGB versus YCbCr, source HDCP behavior, or HDMI negotiation) without declaring which one ahead of evidence.
4. If no signal from a second verified independent source, inspect the receiver, cabling, dock/Thunderbolt arrangement, firmware/video DMA state, and analog capture separately. Do not flash firmware as a diagnostic shortcut.
5. Once frames arrive, implement timestamped archival samples and a local-network preview for Reframe; no Syphon required.

Sources for general DeckLink behavior:
- [Blackmagic official FAQ on selecting HDMI capture source](https://www.blackmagicdesign.com/developer/support/faq/desktop-video-developer-support-faqs)
- [Blackmagic IDeckLinkStatus documentation](https://sdk-doc.blackmagicdesign.com/decklink-sdk/DeckLinkAPI/interfaces/idecklinkstatus.html)
- [Our original inventory and tested C++ SDK boundary](2026-10-21-native-enumeration.md)

No source video frames, copyrighted vendor binaries, proprietary SDK headers, or firmware images are stored in the public project.
