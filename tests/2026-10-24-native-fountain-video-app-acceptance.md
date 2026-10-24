# Native Fountain Video.app acceptance — 24 October 2026

## User requirement and implementation choice

The owner explicitly rejected using their modern MacBook as an HDMI source or performing more physical/source-mode experiments. The accepted next step is an **application** that operates the legacy capture peripheral directly. We therefore implemented Fountain Video as a native, self-contained **AppKit** Sierra application on the original 2011 Intel Mac mini, using the installed Blackmagic Desktop Video 10.4 user-space runtime and avoiding the known CoreMediaIO plug-in crash.

This is not a promise that software can generate a signal on an unconnected HDMI input. That condition must appear as an ordinary, truthful "No input signal" state.

## Build and deployment evidence

Source: [Native Fountain Video app](../apps/fountain-video-sierra/README.md).

- Built with Sierra's Xcode 9.2 and no external SDK package; result is a Mach-O 64-bit x86_64 executable.
- Info.plist passed Apple's plutil validation.
- Installed to **/Users/bene/Applications/Fountain Video.app**, entirely user-owned without elevated privileges.
- The native GUI was actually opened on the original Mac mini console, and a visual screenshot was privately inspected. It displays device availability, source and format selectors, live preview stage, capture buttons, snapshot and movie recording controls.
- The latest installed UI shows **Auto · scan 4 formats** as the default physical capture-mode option.
- A normal Apple Event Quit was tested on a prior GUI launch without a crash. The updated app was relaunched successfully after installation; it remains open.
- Final repository source revision: **7cc9648** (subsequently amended only for reporting, if applicable).

## Verified operational results

    stage=native_app result=built
    stage=fountain_video result=pass suite=converter,virtual_bars,preview
    stage=app_probe result=device_found locked=0

The idle status read reported an unsupported/not-active status HRESULT; this is **not proof of a firmware defect**.

The AVFoundation MOV encoding test wrote a genuine QuickTime file using twelve generated test frames. It verified the resulting file size **30,305 bytes** and deleted the temporary file:

    stage=movie_test result=pass frames=12 file_bytes=30305 source=synthetic codec=jpeg

The native virtual-preview/GUI test succeeded:

    stage=app_window result=pass virtual_frame=1

A **real capture-interface auto scan** performed by the app on the device, with one serial session per format, completed all four common modes, and then stopped/released the input:

    Scanning HDMI · format 1 of 4
    Scanning HDMI · format 2 of 4
    Scanning HDMI · format 3 of 4
    Scanning HDMI · format 4 of 4
    No signal detected in four common capture formats.
    stage=auto_hdmi_scan result=pass finished=1 inactive=1 no_source=1

The app found **no actual HDMI input signal** on this connection. This remains a valid application result but is **not successful source capture**.

## Lifecycle issue resolved

The initial manual AppKit command-line test harness ended with a crash at outermost autorelease-pool teardown because it created/closed windows without using the normal NSApplication event loop. The actual launched GUI Quit worked. The command-line harness was updated to terminate explicitly after hardware release and diagnostic reporting; both GUI and auto-scan test cases now return zero. User-facing app stability is not inferred solely from the harness: real GUI launch and Quit were separately witnessed.

## Unchanged system boundaries

- Original Blackmagic framebuffer driver 10.4 and kernel driver 10.4 both remained loaded.
- Independent IOKit diagnostic extension still not loaded or installed.
- Crash-prone CoreMediaIO DAL plug-in remains isolated.
- No system display preferences, MacBook HDMI output, PCI registers, boot volumes, vendor kernel binaries or firmware were altered.
- No unverified physical frame is declared captured; native recording and preview from **genuine** source pixels remain acceptance work.
- Synthetic bars and synthetic movie encode are explicitly distinguishable from a genuine hardware frame.

**Acceptance status:** Native application **built / installed / launched / GUI inspected / tests passed**. Physical HDMI capture **still no source**. Independent kernel driver **still offline experimental source**.
