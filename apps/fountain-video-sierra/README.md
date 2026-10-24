# Fountain Video — native Intensity Extreme application for Sierra

**Version:** 0.1 · 24 October 2026  
**Target:** macOS Sierra 10.12.6, 2011 Intel Mac mini, Xcode 9.2  
**Status:** Real native desktop application built, launched, and connected to the actual Blackmagic Intensity Extreme through Desktop Video 10.4. Virtual playback and movie encoding are tested. Live HDMI capture remains unverified because the device reports no input signal.

## What the application does

Unlike the earlier scripts and the Python virtual-only browser interface, Fountain Video is a native **AppKit application**. It uses the installed **DeckLinkAPI.framework** directly, without depending on the crashing CoreMediaIO video device-discovery plug-in.

Its window provides:

- Blackmagic HDMI input for physical capture, or visibly labeled **virtual color bars** for internal testing, with no external video or MacBook required.
- **Auto · scan 4 formats** (1080p30, 1080p25, 720p60, 720p50), plus manual format selection. Modes are tried sequentially; failed modes release the stream before the next attempt. No system monitor modes are changed.
- Native **Start/Stop**, with callback counts, no-source status, and clean stream release.
- Live video preview using 8-bit YUV 4:2:2 to RGBA conversion when the device actually delivers valid frames.
- **Save current frame…** to a user-chosen PNG path.
- **Record movie…** to a user-chosen QuickTime MOV path, encoded with Sierra's AVFoundation JPEG codec. Recording begins only on genuine video frames unless the user explicitly selects the virtual source. No image/audio/video is saved automatically.
- On-screen device and video-state reporting. An absence of HDMI signal is shown as an ordinary state, not a request to manipulate the MacBook.

Virtual frames are marked **SYNTHETIC · NOT CAPTURED**, including in default export filenames. Synthetic capture never counts as physical evidence.

## Location and reproducible build

The installed user-owned app is at:

    /Users/bene/Applications/Fountain Video.app

For a source-owned rebuild on the Sierra Mac mini (no root, third-party SDK, package manager or CI):

    sh apps/fountain-video-sierra/build.sh

Output:

    build/Fountain Video.app

This requires only Xcode 9.2 and the macOS system frameworks. The source tree does **not** distribute Blackmagic binaries or firmware; the vendor library is loaded only from its locally installed framework.

## Tests passed on the physical Sierra host

    --self-test
    stage=fountain_video result=pass suite=converter,virtual_bars,preview

    --probe
    stage=app_probe result=device_found locked=0

    --self-test-movie
    stage=movie_test result=pass frames=12 file_bytes=30305 source=synthetic codec=jpeg

    --self-test-window
    stage=app_window result=pass virtual_frame=1

    --self-test-autoscan
    Scanning HDMI · format 1 of 4
    Scanning HDMI · format 2 of 4
    Scanning HDMI · format 3 of 4
    Scanning HDMI · format 4 of 4
    No signal detected in four common capture formats.
    stage=auto_hdmi_scan result=pass finished=1 inactive=1 no_source=1

The movie test actually encoded twelve generated frames, verified the file and deleted the temporary test movie. The auto scan really exercised the installed vendor driver's input API and released each session; zero input signals were found. These do not constitute physical video capture.

The first command-line AppKit GUI test revealed an artificial autorelease-pool teardown crash because it did not run the normal NSApplication lifecycle. The CLI test was corrected; both window and auto-scan self-tests now return exit 0. The application itself was separately launched into the graphical user session and inspected visually; its normal Quit Apple Event completed without a crash.

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
