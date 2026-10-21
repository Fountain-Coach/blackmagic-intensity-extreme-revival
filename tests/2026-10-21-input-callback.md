# Bounded input-stream callback experiment — 21 October 2026

Source: [`src/decklink-input-observer.cpp`](../src/decklink-input-observer.cpp). The program is self-authored and only interfaces with the **locally installed** DeckLinkAPI framework; it does not include proprietary SDK or driver binaries.

## Command

```sh
clang++ -std=c++11 -Wall -Wextra -O2 \
  src/decklink-input-observer.cpp -framework CoreFoundation -o decklink-input-observer
./decklink-input-observer --observe-three-seconds
```

The program requires explicit opt-in because it temporarily enables and starts a 1080p25 8-bit YUV capture stream. It does **not** write captured pixels, audio buffers, screenshots or recordings. The callback records only whether a frame arrived and whether the vendor's `bmdFrameHasNoInputSource` flag is present.

## Observed output on the legacy Mac

```text
stage=input_interface status=0x00000000 present=1
stage=set_callback status=0x00000000
stage=enable_input status=0x00000000
stage=start_streams status=0x00000000
stage=stop_streams status=0x00000000
stage=remove_callback status=0x00000000
stage=disable_input status=0x00000000
stage=counts frames=0 valid=0 no_source=0 format_changes=0 observer_refcount=1
stage=complete exit=0
```

**Result:** all driver calls to prepare, start, stop and release an input stream returned success. **No frame callback occurred** during the three-second observation. This does not distinguish among no source connected, a mismatched format, an unexpected connector choice, or other capture-path issues. The lack of callbacks also means no determination of signal presence/absence is possible through the frame flags yet.

## Next physical precondition

Establish a test signal from an **authorized, HDCP-free** source connected directly to an **Intensity Extreme input**, not the i-tec dock's separate HDMI output or the host Mac's video output. Prefer a verified 1080p25 or 720p50 signal, then repeat a fixed-mode test and inspect the callback counts. Use a proper breakout cable for analog connector tests.

No proprietary binaries, firmware or source media have been distributed or retained.
