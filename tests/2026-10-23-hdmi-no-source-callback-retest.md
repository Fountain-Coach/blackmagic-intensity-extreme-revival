# 23 October 2026 — physical Intensity Extreme capture retest

Host: 2011 Intel Mac mini, macOS Sierra 10.12.6. The Thunderbolt Intensity Extreme is visible as PCI ID `0xBDBD:0xA124`, and the installed legacy Blackmagic kernel components remain loaded.

This is a **vendor-driver baseline test**, not capture by our independent driver. No Blackmagic kernel software, firmware or device registers were modified.

## Read-only connector selection

Our source-owned [configuration inspector](../src/decklink-configuration-inspect.cpp) was built on Sierra and queried the installed `DeckLinkAPI.framework`:

```text
configuration_interface_status=0x00000000 present=1
cfg=input status=0x00000000 value=0x2
cfg=output status=0x00000000 value=0xa
config_exit=0
```

Input value `0x2` corresponds to HDMI in this device's connection-mask convention.

## Bounded 1080p30 capture observer

Our [input observer](../src/decklink-input-observer.cpp) was compiled from the repository on Sierra and invoked for its built-in three-second observation window:

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
stage=counts frames=83 valid=0 no_source=83 format_changes=0 observer_refcount=1
stage=complete exit=0
vendor_capture_exit=0
```

**Finding:** Unlike earlier runs in which no input callbacks were reported, this test delivered **83 callback frames, all flagged `no source`, and zero valid physical video frames**. In other words, the callback path is functioning, but there is no detected HDMI input signal lock at 1080p30. The test stopped and restored its input session cleanly.

**Do not misinterpret `frames=83` as successful capture:** the callback's frame flags marked every one as missing a video input source.

## Open physical-source question

The read-only protocol data cannot establish whether a live, unprotected 1080p30 source was physically connected and outputting a compatible signal during this retest. A source-free run is expected to report no lock; therefore the result **does not prove that the HDMI receiver is defective**. The next physical acceptance requires a known unprotected HDMI pattern from a second device, validated simultaneously on a working display and at the Blackmagic input, with exact timing documented.

The independent Sierra hardware-driver bundle is still offline-only; see [independent driver smoke test](2026-10-23-sierra-independent-driver-smoke.md).
