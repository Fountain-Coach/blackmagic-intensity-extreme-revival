# HDMI output-to-input self-loop: half-duplex limitation (21 October 2026)

## Purpose and wiring

The device owner physically connected the Blackmagic **HDMI OUT** directly to **HDMI IN** with a single HDMI cable. The 2011 Sierra Mac mini remained attached over Thunderbolt and served as the host for both native test processes.

Both programs were written by Fountain Coach and call the locally installed legacy DeckLink SDK. The output program submits a generated 1080p25 8-bit YUV 4:2:2 color-bar frame 125 times; the input observer requests the same video mode for a three-second test and records only callback counts.

This is a functional test of simultaneously enabling video input and output, **not** an attempt to modify firmware, reconfigure the PCI device, or bypass copy protection.

## Trial A — output already running, then input

The output program was launched in the background, followed by a one-second delay and a second process requesting video input.

```text
INPUT:
stage=requested_video_mode code=0x48703235
stage=input_interface status=0x00000000 present=1
stage=set_callback status=0x00000000
stage=enable_input status=0x80000009
stage=remove_callback status=0x00000000
stage=counts frames=0 valid=0 no_source=0 format_changes=0 observer_refcount=1
stage=complete exit=26

OUTPUT:
stage=enable_output status=0x00000000
stage=burst status=starting target_frames=125 format=1080p25
stage=burst status=done submitted=125 last_status=0x00000000
stage=disable_output status=0x00000000
stage=complete exit=0
```

Input activation was refused with `0x80000009` while the output mode was active. The program did not start an input stream and did not receive frames. All 125 generated output frames were accepted by the API and output was cleanly disabled.

## Trial B — input already running, then output

The input observer was launched first, followed by a one-second delay and an output attempt.

```text
INPUT:
stage=enable_input status=0x00000000
stage=start_streams status=0x00000000
stage=signal_lock rc=0x00000000 locked=0 detected_mode_rc=0x80000001 mode=0x0
stage=stop_streams status=0x00000000
stage=remove_callback status=0x00000000
stage=disable_input status=0x00000000
stage=counts frames=0 valid=0 no_source=0 format_changes=0 observer_refcount=1
stage=complete exit=0

OUTPUT:
stage=enable_output status=0x80000009
stage=complete exit=25
```

The input stream started successfully, but the output mode was refused with the same code while capture was active. With no output signal generated, no capture frames or receiver lock were expected.

The device busy-state query after these tests returned `0x0` (idle). No firmware, driver, or persistent preferences were changed.

## Interpretation

Both directions independently succeeded in earlier tests; the same `0x80000009` error appears on whichever interface is enabled second. This is direct evidence of **mutually exclusive input/output mode activation on this unit and software stack**. It agrees with Blackmagic's DeckLink SDK manual, which states that non-4K DeckLink devices and sub-devices are **half-duplex**: capture *or* render can be enabled, not both simultaneously.

Reference: [Blackmagic DeckLink SDK manual](https://documents.blackmagicdesign.com/UserManuals/DeckLinkSDKManual.pdf), section 2.5.4 IDeckLinkInput, note on non-4K DeckLink devices.

This does **not** establish that any individual HDMI input receiver or output transmitter is broken. A cable from the unit's own output to its own input **cannot generate a useful capture test** under this half-duplex restriction. The earlier 125-frame output submission still lacks physical verification on a sink. Earlier MacBook and Mac mini HDMI source tests had `video_input_signal_locked=false`; that remains an independent unresolved issue.

The earlier report [MacBook HDMI source and proposed hardware self-loop](2026-10-21-macbook-hdmi-source.md) recorded a preliminary *apparent* overlap of independent SDK calls, but without confirming that both video modes were enabled simultaneously. The new deliberately synchronized trials supersede any full-duplex inference from that preliminary result.

## Next acceptance

1. Disconnect the self-loop from the **HDMI OUT** socket. Leave the host-to-Thunderbolt cabling undisturbed.
2. Connect a **separate, known-unprotected 720p60 or 1080p30 source** to **HDMI IN**, while Intensity Extreme is in **capture-only** mode.
3. Verify `video_input_signal_locked=true` and at least one real frame callback; if lock remains false, proceed with isolated HDMI receiver and firmware analysis.
4. Separately connect Blackmagic HDMI OUT to an actual compatible monitor and validate the generated bars in **output-only** mode.
5. Continue to expose accurate `half_duplex` capability semantics in future Fountain Video Instrument implementations.

**No Syphon**, no firmware flashing, and no proprietary binaries published.
