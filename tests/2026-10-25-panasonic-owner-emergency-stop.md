# Owner-directed emergency stop — Panasonic HDMI on/off cycling

**Date:** 25 October 2026, Europe/Berlin. **Machine:** original silver Mac mini (Macmini5,3), Sierra 10.12.6, Intensity Extreme A124 over Thunderbolt, Desktop Video 10.4. **Requested outcome:** stop all unintended physical Blackmagic playback and prevent automatic restart **by software, without asking the owner to unplug anything**.

## The actual software restart defect

The first response terminated `fountain-video-output` and Black Syphon and verified `device_busy_state=0`. That was **not sufficient**: separately installed Fountain launch mechanisms subsequently restarted hardware playback. The owner reported the Panasonic repeatedly switching on/off.

Two independent launch paths were identified on Sierra:

1. **LaunchAgent `coach.fountain.video.panasonic`** under `~/Library/LaunchAgents/coach.fountain.video.panasonic.plist`. At observation, the agent held a `fountain-video-output --mode 720p60 --minutes 20` process, PID **8017**. This agent was unloaded with `launchctl unload -w`, and its plist was renamed to `.plist.disabled`. Its playback process was stopped.
2. **A separate GUI AppKit launcher** `~/Applications/Fountain Panasonic Show.app`, registered as `coach.fountain.video.panasonic.show.6024`. At observation it was PID **8398**, with child hardware playback PID **8399**, running the same 20-minute command. Both processes were terminated; the launch job was removed; the app executable was disabled and the bundle renamed `Fountain Panasonic Show.app.disabled`.

This was a genuine Fountain software lifecycle defect. Stopping only a child output process was not reliable because independent launchers could start another. **The earlier assistant did not establish complete shutdown before reporting success.**

## Owner-directed fail-closed controls now installed

- Persistent stop marker: `~/Library/Application Support/Fountain Coach/Video Instrument/run/OUTPUT_DISABLED`. The marker explicitly prohibits further playback until **a new owner instruction**.
- The installed hardware-output executable `~/Library/Application Support/Fountain Coach/Video Instrument/bin/fountain-video-output` has been replaced by a [source-controlled guarded version](../src/fountain-video-output.cpp) and is **non-executable (mode 0644)**.
- The guarded version was compiled on actual Sierra and tested with `--mode 720p60 --seconds 5`. It returned **exit 65, BLOCKED_BY_OWNER_HARDWARE_STOP, vendor_calls=0, frame_submissions=0**, before loading the DeckLink runtime. **No video was emitted for this test.**
- The local Panasonic show CLI `fountain-panasonic-show.sh start` was also tested; it returned **exit 65** with no playback process.
- Source-owned [launcher installer](../tools/sierra/install-fountain-panasonic-launchd.sh) and [GUI app installer](../tools/sierra/install-fountain-panasonic-app.sh) are retired/fail closed. The [standard instrument installer](../tools/sierra/install-fountain-video-instrument.sh) now preserves non-executable hardware output when the owner's stop marker exists.
- The display composition and independent Syphon image kernel are retained as research assets, but **must not automatically enable the original Blackmagic HDMI output again**.

A subsequent independent check after the shutdown found:

```text
launchctl Fountain/Panasonic jobs: NONE
active fountain-video-output or FountainPanasonicShow processes: NONE
installed output binary: non-executable
Panasonic Show app bundle: .app.disabled
IDeckLinkAttributes device_busy_state: 0x0 (idle)
IDeckLinkConfiguration HDMI input: 0x2 (unchanged)
```

## The electrical HDMI connector remains a separate limitation

We also attempted to turn off **HDMI OUT selection** while preserving **HDMI IN** using the original 10.4 `IDeckLinkConfiguration` interface:

- Before: input `0x2`, output `0xA` (HDMI + component video).
- `SetInt(video_output_connection, 0x8)` (component-only) returned `S_OK`, **but immediate and fresh GetInt continued reporting output 0xA**. This is **NOT accepted as a successful physical HDMI disable**.
- `WriteConfigurationToPreferences()` returned `0x80000009` (access denied). Silent administrative elevation was not attempted.
- A separate explicit `IDeckLinkOutput::DisableVideoOutput()` invocation while the device was idle also returned `0x80000009`; the video output was already not under the caller's active control.
- The original Desktop Video 10.4 does not expose usable Panasonic EDID or on-wire HDMI image verification. **Therefore the Panasonic's actual power/on-off behaviour cannot be independently witnessed or proven silenced from these software readings.** Do not claim `output=0x8`, no electrical HDMI voltage or optical success. Do not request that the owner perform software setup.

The legitimate source of unwanted **Fountain playback restarts** has been eliminated in the live Sierra user session, and its executable is now locked out. HDMI OUT remains a selected hardware-capability bit `0xA` in the vendor interface, even while playback is idle. This distinction must be preserved for any future driver-replacement investigation.

## Owner contract

**Do not relaunch the Panasonic GUI show, reinstall the retired launch agent, remove the output-stop marker, restore execute permissions, or begin physical video playback without a subsequent explicit instruction from the owner.** The owner should not be asked to perform software tasks. Keep HDMI IN, Thunderbolt and the Panasonic HDMI OUT cables unchanged unless the owner explicitly decides otherwise.

This incident is not a completed Panasonic optical output acceptance. It is a **software playback shutdown and restart prevention**.
