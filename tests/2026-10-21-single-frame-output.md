# Native single-frame output — 21 October 2026

The test is implemented in [`src/decklink-output-frame.cpp`](../src/decklink-output-frame.cpp). It is a separate, explicitly opt-in program that uses the **installed** Blackmagic DeckLinkAPI framework. No SDK files, firmware, or vendor binaries are redistributed.

## Test procedure

```sh
clang++ -std=c++11 -Wall -Wextra -O2 \
  src/decklink-output-frame.cpp -framework CoreFoundation -o decklink-output-frame

./decklink-output-frame --frame-once
```

The opt-in flag is required because this test temporarily **activates external video output**. Use only with a compatible display and when it is safe to change the device's output mode. The program disables output and frees the frame afterward.

## Observed outcome

Running on the legacy Mac mini with the Intensity Extreme and Desktop Video 10.4:

```text
stage=device status=0x00000000 present=1
stage=output_interface status=0x00000000 present=1
stage=enable_output status=0x00000000
stage=create_frame status=0x00000000 present=1
stage=get_frame_bytes status=0x00000000 present=1
stage=fill_frame status=ok bytes=4147200 pattern=eight_yuv_bars
stage=display_frame_sync status=0x00000000
stage=disable_output status=0x00000000
stage=complete exit=0
```

The program created a 1920 × 1080 8-bit packed YUV 4:2:2 frame (1920 × 2 × 1080 = 4,147,200 bytes), wrote eight generated color bars, and invoked the synchronous `DisplayVideoFrameSync` API.

**This is API-level proof of an accepted playback frame, not proof of optical verification on a physical monitor.** No display on the capture device's output was independently confirmed. There was no incoming video, no audio, no archival recording, and no network transport in this test. Output was disabled after submission.

## Next acceptance

Observe the generated test pattern on a directly connected compatible sink, then assess actual video output signal integrity and documented connectors. In parallel, test input-stream callbacks for signal presence and valid frames without retaining private footage.

Avoid confusing the attached i-tec dock's HDMI output with the *Intensity Extreme* HDMI output: they are separate connections.
