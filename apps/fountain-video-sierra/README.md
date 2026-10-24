# Fountain Video — native Intensity Extreme application for Sierra

**Version:** 0.1 · 24 October 2026  
**Target:** macOS Sierra 10.12.6, 2011 Intel Mac mini, Xcode 9.2  
**Status:** Real native desktop application **starts in software-only synthetic video mode**. It no longer queries Blackmagic hardware at launch. A dedicated **Software lab…** button opens the [complete video/audio simulation](../../docs/full-software-video-laboratory.md). Optional real capture still uses the installed Desktop Video 10.4 driver if deliberately selected; physical HDMI capture remains unverified.

## What the application does

Fountain Video is a native **AppKit application**. On launch it selects **Virtual color bars · test** and starts drawing generated frames, without any camera, HDMI connection or Blackmagic device query. The separate full-stack simulator runs on the Ubuntu Mac mini and is reachable through a **Software lab…** button. Only optional real capture loads the installed **DeckLinkAPI.framework**; the isolated crashing CoreMediaIO plug-in is not required.

Its window provides:

- Visibly labeled **virtual color bars on startup**, without any external video or hardware dependencies; optional Blackmagic HDMI input is only selected explicitly.
- **Auto · Nikon D5200 (6 formats)** (PAL: 720p50, 1080i50, 1080p25; NTSC: 720p59.94, 1080i59.94, 1080p29.97), plus manual exact-rate choices. Modes are tried sequentially, prioritizing PAL for the connected D5200; failed modes release the stream before the next attempt. No system monitor modes are changed. See the [D5200-specific setup](../../docs/nikon-d5200-hdmi-setup-2026-10-24.md).
- Native **Start/Stop**, with callback counts, no-source status, and clean stream release.
- Live video preview using 8-bit YUV 4:2:2 to RGBA conversion when the device actually delivers valid frames.
- **Save current frame…** to a user-chosen PNG path.
- **Record movie…** to a user-chosen QuickTime MOV path, encoded with Sierra's AVFoundation JPEG codec. Recording begins only on genuine video frames unless the user explicitly selects the virtual source. No image/audio/video is saved automatically.
- **Software lab…** opens the full two-device video/audio lab with virtual capture and output, rational timing, hot swap, fault injection, and downloadable raw media. Hardware format controls are disabled while the virtual source is active.
- On-screen device and video-state reporting. An absence of HDMI signal is shown as an ordinary state when optional hardware capture is selected.

Virtual frames are marked **SYNTHETIC · NOT CAPTURED**, including in default export filenames. Synthetic capture never counts as physical evidence.

## Location and reproducible build

The installed user-owned app is at:

    /Users/bene/Applications/Fountain Video.app

For a source-owned rebuild on the Sierra Mac mini (no root, third-party SDK, package manager or CI):

    sh apps/fountain-video-sierra/build.sh

Output:

    build/Fountain Video.app

This requires only Xcode 9.2 and the macOS system frameworks. The source tree does **not** distribute Blackmagic binaries or firmware; the vendor library is loaded only from its locally installed framework.

The full video lab button reads user-defaults key **FountainVideoLabURL**. If unset, the generic default is localhost port 8272. For the existing Fountain Coach WLAN installation, the account preference is already set to **http://benedikt-macmini.local:8272/lab**. The link only opens the simulation browser; it never opens HDMI or performs a hardware scan.

## Tests passed on the physical Sierra host

    --self-test
    stage=fountain_video result=pass suite=converter,virtual_bars,preview

    --probe
    stage=app_probe result=device_found locked=0

    --self-test-movie
    stage=movie_test result=pass frames=12 file_bytes=30305 source=synthetic codec=jpeg

    --self-test-window
    stage=app_window result=pass virtual_frame=1

    --self-test-autoscan  (HISTORICAL hardware baseline, not run in software-only lane)
    Scanning Nikon HDMI · format 1 of 6
    Scanning Nikon HDMI · format 2 of 6
    ...
    Scanning Nikon HDMI · format 6 of 6
    No signal detected in six Nikon HDMI formats.
    stage=auto_hdmi_scan result=pass finished=1 inactive=1 no_source=1

The movie test actually encoded twelve generated frames, verified the file and deleted the temporary test movie. The previous camera-on auto scan exercised the vendor input API and found no valid frames; **no HDMI test was performed after the user turned the Nikon off and selected software-first development**. Synthetic video never counts as physical capture.

The first command-line AppKit GUI test revealed an artificial autorelease-pool teardown crash because it did not run the normal NSApplication lifecycle. The CLI harness was corrected; software-only tests now pass with no hardware access. The application itself was separately launched into the graphical user session and inspected visually; its normal Quit Apple Event completed without a crash.

## Safety and known gaps

- Blackmagic Desktop Video 10.4 kernel components remain loaded; the independent IOKit kernel-extension prototype remains unloaded.
- The crash-prone system-wide CoreMediaIO plug-in stays isolated.
- No firmware, EEPROM, PCI BAR, DMA, startup volume, system display setting or modern MacBook setup was changed.
- Real-source HDMI preview and movie capture are **implemented but not physically validated** because the input remains without a verified signal.
- HDMI **output** playback, audio recording, analog connector switching and a fully independently owned kernel driver are not implemented in version 0.1.

The app can control a connected device directly. No application can electrically supply an HDMI input signal without an actual connected source. That limit is represented by a clear NO HDMI INPUT SIGNAL state rather than a workflow requiring the user to manipulate another Mac.

## Owned components

- FountainVideo.mm — native AppKit GUI, dynamic DeckLink input client, YUV video preview, virtual frames, PNG and AVFoundation MOV export.
- Info.plist — bundle identity and Sierra platform boundary.
- build.sh — source-owned user-space build; no privilege escalation.

Source in this repository is authoritative. No GitHub CI or unsupervised installation.
