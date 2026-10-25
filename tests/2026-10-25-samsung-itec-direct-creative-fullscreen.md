# Samsung U28E590 through i-tec — direct full-screen creative display

Date: 25 October 2026, original silver Macmini5,3 running macOS Sierra 10.12.6.

## Correction of monitor identity and wiring

The monitor is **Samsung U28E590**, not Panasonic. Earlier references to a Panasonic in the historical test logs refer to this same Samsung and are not evidence of a second monitor.

The owner has disconnected the Samsung from the **Intensity Extreme HDMI OUT** and attached it to the **i-tec HDMI OUT**, leaving the Intensity Extreme's own HDMI OUT disconnected. The original Mac HDMI OUT → Blackmagic HDMI IN and original Thunderbolt connections have not been changed in this test.

Sierra recognizes the monitor as:

    U28E590:
      Resolution: 3840 x 2160 @ 30 Hz
      Display Serial Number: HTPM210405
      Main Display: Yes
      Mirror: Off
      Online: Yes
      Connection Type: DVI or HDMI
      Adapter Firmware Version: 7,36

The I/O Registry confirms a connected display; it does not prove the external adapter maker's identity by itself. The owner identified the new port as i-tec.

## Source-owned Mac display presentation

[Actual screenshot of the Fountain Video Instrument's full-screen visual score running on Sierra](evidence/2026-10-25-samsung-itec-die-maschine-fullscreen.jpg).

This is a screenshot captured from the real application's actual 3840 × 2160 window and downscaled to 1280 × 720 for convenient viewing, **not an image generated in chat and not an optical photograph of the Samsung**.

Source change: [native Fountain Video Instrument](../src/fountain-syphon-instrument.m) supports `--live --fullscreen` on the connected `NSScreen`. It uses a borderless screen-sized Cocoa window, calculates the OpenGL viewport using physical backing pixels, preserves source aspect ratio, and publishes the same 960 × 540 animated generator via Syphon. It makes **zero Blackmagic capture or output calls**.

Native build on original Sierra: PASS, Clang `-Wall -Wextra -Werror -fobjc-arc` compiled x86_64 application. The previous GUI executable was backed up as:

    ~/Library/Application Support/Fountain Coach/Video Instrument/tmp/FountainVideoInstrument.pre-Samsung-fullscreen.20261025075444

The new executable replaced only the application's GUI executable in:

    ~/Applications/Fountain Video Instrument.app/Contents/MacOS/FountainVideoInstrument

It was launched through the existing Sierra Aqua session:

    open -a "$HOME/Applications/Fountain Video Instrument.app" --args --live --fullscreen

A separate CoreGraphics process reports:

    window: Fountain Video Instrument | Die Maschine
    onscreen: true
    bounds: X=0, Y=0, Width=3840, Height=2160

The generated score was captured from that window using `screencapture -l`; its 3840 × 2160 pixels were downscaled by `sips` to the evidence JPEG. The screenshot visibly contains colored rings, diagonal traces, fields and signal bars. It is genuine window content, not a blank placeholder.

Independent live Syphon pixel witness while the full-screen window was running:

    stage=syphon_client result=connected source=Fountain_Die_Maschine
    stage=syphon_frame sample=1 pixels=960x540 crc=0x62247686 sampled_nonzero=21378
    stage=syphon_frame sample=2 pixels=960x540 crc=0xe4c5dd23 sampled_nonzero=21378
    stage=syphon_client result=PASS real_pixels_read=1 frame_variation=1 capture_source=generated

This establishes actual changing generated pixels across the independent GPU publication boundary. It does not independently confirm what the user sees optically on the Samsung; the user should confirm that observation.

## Blackmagic safety invariant

- `OUTPUT_DISABLED` owner stop marker: PRESENT.
- Hardware playback executable: non-executable (BLOCKED).
- Automatic Panasonic-named launch jobs: NONE.
- Original Blackmagic A124 device busy state: `0x0` (IDLE).
- **No Blackmagic hardware output was re-enabled**; its physical HDMI OUT remains disconnected from Samsung.

The visual instrument is a direct Mac display application, not the pending independent Intensity driver. The original A124 hardware driver and Panasonic-named historical test branch remain a separate engineering problem. This preview makes the creative score useful immediately without claiming the hardware-revival acceptance.

## Current acceptance

| Boundary | Status |
| --- | --- |
| Samsung U28E590 detected at 3840 × 2160 @ 30 Hz | PASS (Sierra) |
| Fountain native GUI launch on silver Sierra | PASS |
| Source-owned full-screen 3840 × 2160 application window | PASS (CoreGraphics) |
| Actual nonblank full-screen window image captured | PASS |
| Changing generated Syphon frames in second process | PASS |
| Blackmagic output lock preserved; hardware idle | PASS |
| Optical confirmation from Samsung's physical display | **AWAITING OWNER OBSERVATION** |
| Independent Blackmagic HDMI output/capture driver | **NOT IMPLEMENTED / SEPARATE** |
