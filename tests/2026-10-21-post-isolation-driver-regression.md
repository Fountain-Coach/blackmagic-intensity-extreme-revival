# Legacy Blackmagic driver regression tests after CoreMediaIO isolation

Date: 2026-10-21

## Environment

An owner-authorized 2011 Intel Mac mini, macOS Sierra 10.12.6, Blackmagic Desktop Video 10.4, and the original Thunderbolt Intensity Extreme. The old CoreMediaIO/DAL discovery plugin is **safely isolated and preserved**, not deleted; DeckLinkAPI.framework and the original kernel drivers remain loaded.

## 1. Native CoreMediaIO discovery (isolated plugin)

The original self-authored [CoreMediaIO probe](../src/cmio-discovery-probe.c) was compiled on the legacy host against system frameworks only:

```sh
clang -Wall -Wextra src/cmio-discovery-probe.c \
  -framework CoreMediaIO -framework CoreFoundation -o cmio-discovery
./cmio-discovery
```

Observed result:

```text
step=read_devices_size
size_status=0 bytes=0
step=read_devices
devices_status=0 bytes=0 count=0
exit=0
```

**Interpretation:** System-wide CoreMediaIO enumeration is healthy but no longer advertises Blackmagic as a video-device plugin. This is the expected functionality loss of isolation; it is not evidence of actual camera/video signal lock.

## 2. DeckLink output pixel-format matrix

A [separate self-authored native matrix tool](../src/decklink-output-matrix.cpp) uses the **locally installed** DeckLinkAPI framework, without vendor SDK headers. It calls `IDeckLinkOutput::DoesSupportVideoMode` for all 16 enumerated modes and four pixel formats. It does not enable output or submit frames. The 64 calls were made successfully without a process crash.

| Pixel format | Supported / 16 modes | API response |
| --- | ---: | --- |
| `2vuy` (8-bit YUV 4:2:2) | 16 | `0x00000000` in all modes |
| `v210` (10-bit YUV 4:2:2) | 5 | `0x00000000` in all modes; support bit true in five SD modes |
| `ARGB` | 0 | `0x80000003` in all modes |
| `BGRA` | 0 | `0x00000000` in all modes, support bit false |

The five reported `v210` modes are NTSC, NTSC 23.98, PAL, NTSC Progressive and PAL Progressive. The HDMI/component/composite/S-Video connection choice was **not** passed as a parameter in this legacy API, so this is a reported output-mode matrix, not verification of every physical socket.

**Interpretation:** The old API's `DoesSupportVideoMode` operation is not universally broken. It can handle an exhaustive set of mode/pixel-format queries without crashing. The App Store crash must depend on the plugin's particular sequence, state or assumptions. No precise memory-lifetime defect has yet been proven.

## 3. Media Express acquires and releases hardware

With the legacy CoreMediaIO plugin isolated:

- Media Express launched with no immediate failure dialog and showed its main capture/playback interface.
- The native DeckLink device busy-state query returned `0x2` while Media Express was running.
- The Media Express process was then closed; busy-state returned to `0x0`, while the device remained enumerated as **Intensity Extreme**.
- The App Store remained running, with no new crash report during these tests.

**Interpretation:** Media Express appears to acquire a device resource using the underlying DeckLink driver and relinquishes it when it exits. This does **not** certify capture, a physical video signal, successful playback to HDMI, or every Media Express control.

## 4. Existing App Store repair status

The older plugin's `DeckLinkOutputStream::AddAvailableFormatDescriptions` crash path was reproduced in App Store. A reversible administrator-approved A/B test confirmed that isolating the CoreMediaIO plugin allowed App Store to open without a new crash. The plugin is now retained in a safe, privileged recovery location with matching original SHA-256; its original kernel driver remains loaded. The App Store storefront was visually verified without publishing screenshots of the private Desktop.

See [full App Store repair finding](2026-10-21-app-store-blackmagic-crash.md).

## Unresolved work

1. The proprietary Blackmagic CoreMediaIO plugin has **not been patched**; its unmodified binary would remain crash-prone if restored.
2. Implement a safely scoped replacement CoreMediaIO/DAL integration or app-specific compatibility bridge, with local regression tests and versioned rollbacks, if system-wide camera discovery is actually required.
3. Separately investigate Intensity Extreme HDMI receiver's `signal_locked=false` observation using a known unprotected external source when available; software simulation does not establish electrical hardware signal lock.
4. Preserve Direct DeckLink API access as the operational baseline.

These observations involve no kernel modifications, firmware flashing, HDCP circumvention, Syphon, or redistribution of vendor binary files.
