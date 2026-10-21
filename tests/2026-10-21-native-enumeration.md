# Native device enumeration — 21 October 2026

The first published, independently written diagnostic is [`src/decklink-enumerate.cpp`](../src/decklink-enumerate.cpp).

## Test environment

- Legacy capture host: Apple Mac mini (Mid 2011), Intel x86-64, macOS 10.12.6.
- Capture peripheral: Blackmagic Intensity Extreme, behind the Thunderbolt dock.
- Installed vendor runtime: Blackmagic Desktop Video 10.4 / DeckLinkAPI.framework.
- Method: compile a small C++ program on the old Mac; load the installed framework with `dlopen`, access a small subset of stable COM interface methods, enumerate the installed device and its reported input/output display modes.
- No third-party SDK headers, proprietary driver binaries, firmware, decompilations, or personal host identifiers included in this repository.

## Verified command

```sh
clang++ -std=c++11 -Wall -Wextra -O2 \
  src/decklink-enumerate.cpp -framework CoreFoundation -o decklink-enumerate
./decklink-enumerate
```

Compile **on a supported legacy macOS host with the installed DeckLink API framework**. The binary does not replace drivers, and its existence does not imply that the Intensity Extreme works on current macOS.

## Observed result

- Process exit code: **0**.
- Framework load: success.
- C entry point `CreateDeckLinkIteratorInstance`: found.
- Factory returned a usable iterator.
- One device enumerated, model string: **Intensity Extreme**.
- `QueryInterface` for **IDeckLinkInput**: success.
- `GetDisplayModeIterator` for input: success; **16 modes**.
- `QueryInterface` for **IDeckLinkOutput**: success.
- `GetDisplayModeIterator` for output: success; **16 modes**.
- No unexpected signal or exception during this corrected test.
- **IDeckLinkAttributes** interface queried successfully through the SDK.
- **Video I/O support**: `0x3` — both capture and playback advertised.
- **Input connectors**: `0x3a` — HDMI, component, composite and S-Video advertised.
- **Output connectors**: `0x3a` — HDMI, component, composite and S-Video advertised.
- **Maximum audio channels**: `2`.
- **Device busy state**: `0x0` (idle during this query).
- **Pixel support — 1080p25 and 720p50, both input and output**: 8-bit YUV (`'2vuy'`) is supported (`1`); 10-bit YUV (`'v210'`) is not supported (`0`) in these two modes. All eight read-only `DoesSupportVideoMode` invocations returned API status `S_OK`. This does not establish support or lack of support in other modes.

These values were reported by the installed runtime on the tested hardware. They do not by themselves demonstrate that every connector passes a signal.

Both input and output iterators advertised the following formats. The SDK-reported frame rates here are the *field rates for interlaced formats* only where the interface reports them; they do not guarantee capture or playout:

| Name | Dimensions | SDK-reported rate numerator/denominator |
| --- | ---: | ---: |
| NTSC | 720×486 | 30000/1001 |
| NTSC 23.98 | 720×486 | 24000/1001 |
| PAL | 720×576 | 25000/1000 |
| NTSC Progressive | 720×486 | 60000/1001 |
| PAL Progressive | 720×576 | 50000/1000 |
| 1080p23.98 | 1920×1080 | 24000/1001 |
| 1080p24 | 1920×1080 | 24000/1000 |
| 1080p25 | 1920×1080 | 25000/1000 |
| 1080p29.97 | 1920×1080 | 30000/1001 |
| 1080p30 | 1920×1080 | 30000/1000 |
| 1080i50 | 1920×1080 | 25000/1000 |
| 1080i59.94 | 1920×1080 | 30000/1001 |
| 1080i60 | 1920×1080 | 30000/1000 |
| 720p50 | 1280×720 | 50000/1000 |
| 720p59.94 | 1280×720 | 60000/1001 |
| 720p60 | 1280×720 | 60000/1000 |

## Important engineering distinction

Earlier, Media Express displayed **“Blackmagic hardware could not be found in your system.”** This new independent test confirms the installed vendor API *does* enumerate a device and expose capture/playback interfaces. We have **not** demonstrated hardware readiness, an active input signal, recording, output framing, firmware recovery, or operation without the vendor runtime.

While constructing the test, an earlier experimental `QueryInterface` call failed because we incorrectly treated macOS's `CFUUIDBytes` argument as a pointer instead of a by-value aggregate. The test program crashed locally in the vendor API. We corrected that calling convention before obtaining the above successful result. Do not repeat the invalid invocation. The machine and driver remained intact.

## Next checks

1. Query optional, read-only device attributes and compare runtime interface availability to vendor application enumeration (basic I/O attributes now verified).
2. Where safe, request supported pixel formats/modes without enabling input/output.
3. Check actual signal presence and compile a bounded video-frame probe with explicit timeouts.
4. For each physical connector (HDMI, component, composite, S-Video, analog audio), record a controlled signal test separately.
5. Only after these tests, characterize latency, frame rate, synchronization and network delivery.

**Sources:** [hardware capability inventory](hardware-capabilities.md), [investigation record](investigation.md), and [Desktop Video SDK](https://sdk-doc.blackmagicdesign.com/decklink-sdk/).
