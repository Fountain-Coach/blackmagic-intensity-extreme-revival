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

## Later black-screen observation and read-only input diagnostics (08:34–08:55)

**Owner's physical observation:** the Samsung remains **BLACK**, with Die Maschine closed and CPU-intensive photolibraryd temporarily suspended. A Sierra WindowServer screenshot is not a monitor-side image witness; the previous successful fullscreen claims are only host window/synthetic Syphon acceptance. The old i-tec direct-display attempt has **NOT PASSED optical acceptance**.

- The actual Samsung `U28E590` EDID resolves to Intel `AppleIntelFramebuffer@1`, monitor vendor 19501, product 3148, serial 810375238. I/O Registry exposes one associated I2C bus.
- The main display is marked online, active and awake by macOS. The persistent 1920×1080 60Hz mode is accepted. Those properties prove host-side enumeration, NOT that the HDMI picture appears.
- The original Blackmagic OUTPUT_DISABLED lock remains present. FountainVideoInstrument is stopped. Silver Mac display was not rebooted, nor was Blackmagic output restarted.
- Source-owned [Samsung DDC input-source probe](../tools/sierra/samsung-ddc-vcp-readonly.c) compiled on original Sierra. It uses single-shot GET VCP `0x60`, **without monitor-setting writes**, matched to exact Samsung PCI framebuffer/EDID.
- Apple's I2C API returned `0x0` and 11 reply bytes but **the reply did not pass DDC/CI frame/checksum validation**. A simple-I2C reply and the framebuffer-advertised DDC-CI reply type both produced invalid, differing response bytes. Current physical input selection could **NOT** be determined; do **not** report HDMI 1 or HDMI 2 and do not issue a guessed SET VCP command.
- The Samsung U28E590D manual describes automatic or manual source detection and distinct HDMI 1, HDMI 2 and DisplayPort selections. An EDID exchange does **not** prove which source its screen is actually displaying.

**Next hardware isolation, only by explicitly requested cable movement:** bypass the i-tec output entirely by temporarily unplugging the cable from the silver Mac HDMI OUT to Blackmagic HDMI IN **at the Mac end only**, and moving the Samsung's HDMI cable from i-tec OUT to the now-free silver Mac HDMI OUT. The Blackmagic HDMI OUT must stay disconnected. Then read original Mac display enumeration and ask for an actual Samsung-side picture observation. If the direct connection also remains black, test Samsung's selected physical input/monitor behavior; if direct works, the i-tec/dock route is the failure boundary. This plug change has **not** occurred yet.

No claim of Samsung optical success or a repaired screen until a user physical witness confirms it.
