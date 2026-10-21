# Blackmagic's CoreMediaIO plugin crashes App Store — 21 October 2026

**Status: reproduced on the legacy capture host. Root cause narrowed to Blackmagic's CoreMediaIO / DeckLinkAPI transaction path, not yet fixed.**

## Why this matters

The original Intensity Extreme and its macOS 10.12.6 driver are detectable and directly accessible through our own DeckLink application, but Apple's **App Store** crashes during device discovery. The failure is therefore not merely that Media Express refuses capture or that the physical HDMI receiver has not synchronized: an independent operating-system application is affected by installed third-party video software.

The observations below were made on an owner-authorized Intel Mac mini running macOS 10.12.6 and Desktop Video 10.4, and are intentionally sanitized. Raw crash reports and copyrighted third-party binaries are not distributed here.

## Reproducible observations

App Store version **2.2.1** crashed four times on 21 October 2026 (local times approximately 06:05, 06:09, 06:45 and 11:51).

In *each* App Store report:
- `EXC_BAD_ACCESS (SIGSEGV)`, invalid address `0x000000000000000c`.
- The crashed thread entered `DeckLinkCMIO` via `DeckLinkOutputStream::AddAvailableFormatDescriptions()`.
- It invoked `CDeckLinkOutput::DoesSupportVideoMode` → `GetDoesSupportVideoOutputMode` → `DesktopVideoIOKitClient::scheduleFrame` → `DesktopVideoMessage::CreateScheduleVideoFrameRequest`.
- The top frame was `libdispatch.dylib: _os_object_release`.
- The loaded plug-in identifier in the crash report was `com.blackmagic-design.desktopvideo.DeckLinkAPI`.

At approximately **06:09, 06:45 and 11:51**, the Blackmagic helper process `com.blackmagic-design.desktopvideo.DeckLinkHardwareXPCService` also crashed within milliseconds of App Store. Its crashed work queue was `com.blackmagic-design.desktopvideo.XPCServiceDispatch`, with its top frame in `DesktopVideoIOKitService::handleIncomingMessage`. The helper died with `EXC_BAD_ACCESS (SIGSEGV)`, `EXC_I386_GPFLT`.

At **11:51**, we actively launched App Store through an authorized remote session. It started, exited, and produced another crash report exhibiting the same stack. This confirms current reproducibility rather than relying only on historical reports.

No evidence from these traces points to a failure of the App Store's own application logic as the initial faulting component. The traces directly implicate **Blackmagic's bundled APIs and helper service** as the proximate failure path, though they do not yet identify the precise ownership/lifetime defect responsible for the invalid memory access.

## Components and locations

| Component | Installation role |
| --- | --- |
| `/Library/CoreMediaIO/Plug-Ins/DAL/DeckLinkCMIO.plugin` | System-wide camera/capture-discovery integration loaded into application processes |
| `/Library/Frameworks/DeckLinkAPI.framework` | Legacy Blackmagic API framework used directly by our native probes |
| `/Library/Application Support/Blackmagic Design/Blackmagic DeckLink/...DeckLinkHardwareXPCService.xpc` | Blackmagic's helper that also crashes in the same transaction window |
| `/Library/Extensions/DeckLink_Driver.kext` | Legacy kernel hardware driver, attached to the Intensity Extreme |

The CoreMediaIO plug-in binary is dated 2015, Desktop Video package version is 10.4, and App Store is the macOS Sierra application. Do **not** conclude that any specific later operating system remains compatible with these components.

Private reference hashes, for identification only:
- CoreMediaIO executable SHA-256: `6f1eda3ee6521dcb4c62df86f4f4e64e7e10a35dd47b39ee97d35472ca469da3`.
- Blackmagic hardware XPC service SHA-256: `885b5f8d7d527335c60a1cdef69f8acf7f7635f93cd3ed749f69a96cd420ea89`.

## Negative control

A minimal self-authored command-line program asking AVFoundation for video capture devices completed successfully and returned zero devices without crashing. This tells us **not every AVFoundation query triggers the failing path**; it does not disprove the four Blackmagic-in-App-Store crash stacks.

The existing standalone DeckLink probes also enumerate one Intensity Extreme, expose video input and output interfaces and perform mode activation without the App Store's crash. They use the installed SDK and driver directly, not the CoreMediaIO discovery path.

## Proposed reversible isolation test — NOT YET EXECUTED

The *minimal* intervention is to temporarily withdraw **only** the system-wide `DeckLinkCMIO.plugin` from macOS's `/Library/CoreMediaIO/Plug-Ins/DAL/` discovery directory, to a preserved, administrator-controlled quarantine location outside that directory. This avoids changing the kernel driver, the underlying API framework, firmware or physical I/O.

Required acceptance sequence:
1. With administrator authorization and a tested rollback plan, back up the exact plug-in bundle, ownership, permissions and SHA-256 hash. Ensure App Store is not currently running.
2. Move only the CoreMediaIO plug-in bundle out of the automatic discovery directory. Do not uninstall Desktop Video and do not delete binaries.
3. Start a fresh App Store process. Check whether it survives beyond initialization and record whether any new Blackmagic XPC/App Store crash appears.
4. Test our own DeckLink device enumeration without relying on the disabled discovery integration; verify the Intensity Extreme still appears through the native SDK.
5. Restore the plug-in to its original location, ownership and permissions for the final A/B comparison, unless an owner-approved persistent isolation strategy has been agreed upon.
6. If disabling the plugin eliminates App Store crashes, document the isolation as a defensible workaround. Independently investigate correct fixing or replacing the defective integration.
7. If the App Store still crashes with the plugin absent, revise the hypothesis; report both results.

**Operational constraint:** The plug-in lives in a root-owned system directory. The authorized SSH user has admin-group membership but has no active passwordless sudo authorization. A/B isolation could not be run unattended. This limit is respected. No file was moved or changed.

Disabling the DAL plug-in may remove system-wide camera/video-device discovery for this Blackmagic product in unrelated applications, even if direct DeckLink API use continues working. This potential impact is why the isolation must be deliberate and reversible.

## What this does not prove

- It does not establish that the same bug prevents HDMI signal lock (that remains [separate issue #4](https://github.com/Fountain-Coach/blackmagic-intensity-extreme-revival/issues/4)).
- It does not prove the hardware's firmware is defective.
- It does not prove Blackmagic driver replacement is complete or required.
- It does not justify deleting or force-unloading the kernel extension without a rollback strategy.

**Immediate priority:** isolate and understand the faulty CoreMediaIO integration while preserving native access to the device. Then return to the HDMI receiver's signal-lock fault. No Syphon, no blind driver reinstall, no firmware flashing.
