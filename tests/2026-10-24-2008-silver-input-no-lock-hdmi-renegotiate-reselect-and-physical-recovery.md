# 24 October 2026, 20:08–20:18 CEST — original HDMI IN no-lock: source/sink rehandshake and volatile input selection

## Final hardware contract

The only authorized **input source** remains the **silver 2011 Macmini5,3 HDMI OUT → original Blackmagic Intensity Extreme HDMI IN**, via original Thunderbolt/Comdis back to **that same silver Sierra 10.12.6 host**. The new external Samsung U28E590 monitor is intended on **original Blackmagic HDMI OUT (Samsung HDMI2)**.

No other input generator, Mac or camera has been introduced; no new driver, firmware, OS, KEXT, network service, alternate cable routing or production release is approved. Any independent return from the monitor remains uninstrumented.

## Most important new observation: GPU sink identity changed

- Sierra **rebooted at 18:04** (observed with `last reboot`; not initiated by this investigation). Mac remained original Sierra 10.12.6 and Blackmagic Desktop Video KEXT 10.4.
- At **20:10 CEST**, Sierra reported its sole active display as **Samsung U28E590**, vendor ID **19501**, EDID SHA-256 **`13a90125fb31020f722bbf308adc50753a9e51e513865679a3c6b14c05d9ca25`**, timing **1920×1080 at 30Hz**. Previously source output had appeared as **BMD HDMI** vendor **8961**. This means the *visible EDID/HDMI negotiation state* changed, **not** proof a particular cable was physically moved: sink EDID might be passed through or there might be a different active route. Actual on-wire HDMI encoding and electrical source-to-card continuity were not measured.
- The original Blackmagic Intensity Extreme still enumerated over Thunderbolt through the original Comdis THUNDERDOCK2. Current use of Blackmagic HDMI OUT was evidenced by **separate existing user-owned logs** on silver (not initiated here) reporting a completed **90-second 720p60 output** with **4061 vendor-accepted frames** and a separate **300-second 1080p30 output** with **7120 vendor-accepted frames**, zero API errors. These demonstrate vendor output continuity, not optical monitor pixels or incoming signal.
- At approximately **20:15**, after an Apple CoreGraphics temporary timing renegotiation, Sierra again reported **BMD HDMI** vendor **8961**, 1920×1080@30. The input was independently retested after this identity recovered and remained without signal lock. **EDID return is not video reception**.

## Independent actual device input tests

Source-owned original `src/decklink-mode-armed-no-stream.cpp`, compiled on the real silver:

```
stage=enable_video_input rc=0x00000000 requested=1080p30 pixel_format=2vuy
stage=mode_armed_status step=0..3 lock_rc=0x00000000 lock=no
stage=disable_video_input rc=0x00000000
stage=complete exit=0 stream_started=0 physical_frame_captured=0
```

Results unchanged both before and after 20:15 BMD HDMI source-EDID identity returned. These are independent of Fountain Video and the newer record/replay adapter.

Prior detailed diagnostics (same day) were independently conclusive: 15-second no-lock signal poll, explicit inactive output disable with no improvement, attempts at 1080p30/29.97/25/i59.94 all no-lock, and a 1.5-second original vendor StartStreams/callback test with `GetFlags` only returned **zero callbacks**, safely stopped and released. Thus the **original input currently does not deliver frames**, not merely a bad UI/lock flag.

## Safely retried macOS HDMI source timing

Source-owned `tools/sierra/silver-hdmi-rehandshake.py` uses public CoreGraphics on the verified original silver model and 10.12.6 host. The then-visible Samsung sink provided 1920×1080@24 and @30; program switched @30→@24 for 2s:

```
stage=silver_hdmi_rehandshake preflight=PASS vendor=19501 original=1920x1080@30 alternative=1920x1080@24
stage=silver_hdmi_rehandshake switch_to_p24_rc=0
stage=silver_hdmi_rehandshake restore_to_original_rc=1001
stage=silver_hdmi_rehandshake result=RESTORE_UNVERIFIED
```

The initial direct restoration used the stale Quartz display ID, which became invalid after reconfiguration. Critically, **after the temporary program exited**, independent `system_profiler` and `Quartz.CGDisplayCopyDisplayMode` checks both reported **the original 1920×1080@30**. This is consistent with documented temporary Quartz mode rollback on process termination. The source was **patched** to re-enumerate Quartz display IDs for any future test; its Python 2.7 syntax was compiled on actual Sierra, but **no second live mode-change run was claimed or needed**.

After this rehandshake Sierra changed back to identifying **BMD HDMI**. Nonetheless the original input-armed signal-lock check remained 0/4, proving HDMI display identity recovery alone was insufficient.

Public Apple API documentation: https://developer.apple.com/documentation/coregraphics/cgdisplaysetdisplaymode%28_%3A_%3A_%3A%29 .

## Original Blackmagic input route reset — actual live test

Source-owned **`src/decklink-silver-hdmi-input-reselect.cpp`**, compiled on real Sierra under exact **BMD HDMI at 1080p30** fixture, uses only original user-space `IDeckLinkConfiguration`:

```
stage=physical_input_reselect original_connector_rc=0x00000000 original_connector=0x00000002 expected_hdmi=0x00000002
stage=physical_input_reselect temporary_component_set_rc=0x00000000 read_rc=0x00000000 observed=0x00000008
stage=physical_input_reselect restore_hdmi_rc=0x00000000 readback_rc=0x00000000 restored_connector=0x00000002 changed_away=1 preferences_written=0
stage=physical_input_reselect enable_p30_rc=0x00000000
stage=physical_input_reselect sample=0..15 rc=0x00000000 locked=NO consecutive=0
stage=physical_input_reselect disable_input_rc=0x00000000
stage=physical_input_reselect result=STILL_NO_LOCK original_input_hdmi_restored=1 signal_lock_samples=0 stream_started=0 frames_captured=0 firmware_driver_display_and_cabling_unchanged=1
```

This proves the original input connector can be reselected and reset through the old driver without admin rights and with independent readback, but this **did not fix the physical HDMI input lock**. No preferences were saved and no streams or framebuffer data recorded.

Blackmagic documents original input selection using `IDeckLinkConfiguration::SetInt(bmdDeckLinkConfigVideoInputConnection, bmdVideoConnectionHDMI)`: https://www.blackmagicdesign.com/developer/support/faq/desktop-video-developer-support-faqs .

## Minimum next **physical** discriminating test

**Do not substitute any input source or make a different permanent rig.** Reversible test of the newly added monitor's possible HDMI negotiation effect:

1. **Temporarily unplug ONLY the external Samsung monitor cable from the original Blackmagic HDMI OUT**, leave the silver Mac HDMI OUT → Blackmagic HDMI IN and original Thunderbolt→silver connected exactly as required. This removes only the **newly added downstream HDMI sink**, not the source.
2. On the silver, inspect active display/EDID then run original `src/decklink-mode-armed-no-stream.cpp` at exact 1080p30 (two consecutive `visl=true` are needed for acceptance); if lock returns, the new downstream sink/EDID/handshake is implicated. An optical monitor-image check is unnecessary.
3. If lock remains false, **reseat the existing silver Mac HDMI OUT cable at original Blackmagic HDMI IN**, retaining the same cable/source/physical topology, and repeat; verify *IN* versus *OUT* labels.
4. If still false, a **controlled power cycle of the original Intensity Extreme's Thunderbolt power path** (after stopping card clients, and without rebooting/reinstalling Sierra) may be necessary to reset hardware HDMI receiver/EDID/HPD state. This is an explicit separate physical change and should not be improvised unattended. Record after reconnection.
5. Only after actual stable lock is restored run the known-safe, bounded complete frame capture and original raw SHA manifest, then sequential record→HDMI OUT playback with independent digital comparison. Input and output are not advertised as full-duplex/bypass on original 10.4; no unsupported simultaneous pass-through claim.

There is **no honest software-only solution demonstrated yet**. No cable, monitor, electrical HDMI waveform or downstream optical pixel data can be remotely read from this original rig. Driver upgrade/firmware flashing is not warranted; original KEXT 10.4 captured genuine physical pixels earlier on exactly this source.

**Acceptance status:** original output API independently accepted at 1080p30; original input currently **NO LOCK**, **NO NEW FRAMES**, no accepted end-to-end physical record/replay. Preserve PR #10 as draft.
