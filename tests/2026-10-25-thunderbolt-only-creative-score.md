# Thunderbolt-only creative score: live hardware readout, not HDMI capture

Date: 2026-10-25. Hardware: original silver Intel Mac mini, macOS Sierra 10.12.6; Blackmagic Intensity Extreme BDBD:A124 on Thunderbolt only; Samsung U28E590 directly connected to the Mac HDMI output.

## Owner's fixed topology and goal
The owner explicitly removed **both** Intensity HDMI connections and retained only Intensity <-> Thunderbolt <-> Sierra Mac. The Mac's direct HDMI connection to Samsung is a separate, already functioning display path. No external video source, camera, HDMI loop, i-tec display, mode switch or new driver is a prerequisite to this experiment.

Goal: use the actual, attached hardware creatively, without inventing that internal capture or hardware readback has been demonstrated.

## Genuine, present-tense hardware witnesses
Read-only `system_profiler SPThunderboltDataType` observed the Intensity Extreme attached at a 10 Gb/s x1 link, Device ID A124.
Read-only `ioreg` observed an IOPCIDevice at `pcibdbd,a124`, PCI Express tunneled, device resources allocated and original Blackmagic driver matched.
`kextstat` found Blackmagic Desktop Video 10.4 framebuffer and device kernel modules loaded.
Original `src/decklink-enumerate.cpp` was compiled and run with **no** smoke-input or smoke-output flags:
- one native Intensity Extreme enumerated;
- input and output interfaces present, each with 16 reported modes;
- reported video input/output support flags `0x3`;
- maximum audio channels `0x2`;
- `device_busy_state` returned `0` at the enumeration instant;
- dynamic input-format detection explicitly unsupported.

Original `src/decklink-hdmi-sink-status.cpp` was compiled and run **read-only**:
- output mode read `0x6e747363` (NTSC);
- output flags read `0x0`;
- querying `busy` on the status interface returned **unsupported**; do not silently convert this into an idle reading;
- no output state changes or unsafe `GetBytes` calls.

## Creative software
- `tools/sierra/intensity-thunderbolt-score.py`: Sierra Python 2.7 local-only server, bounded at ten minutes; checks exact PCI vendor/device, PCI tunneling, original `OUTPUT_DISABLED` guard and live DeckLink status.
- `tools/sierra/intensity-thunderbolt-score.html`: source-owned Die Maschine composition, animated slowly by Mac software, geometry seeded by *genuinely read* device status. Optional muted-by-default audio oscillator is also Mac-generated; no sound/video is claimed to come off the device's HDMI ports.
- `tools/sierra/run-intensity-thunderbolt-score.sh`: guarded, manual launcher. Compiles the existing safe status probe; does not install a service, start at boot or activate the Blackmagic media engine.

Live owner machine acceptance:
- Python and JavaScript syntax verified, Sierra Python 2.7 compilation verified.
- Local `/health` returned `READ_ONLY_THUNDERBOLT_SCORE`.
- Local `/api/snapshot` returned real `BDBD:A124`, PCIe link raw status 17, output mode `0x6e747363`, output flags `0x00000000`, output-disabled guard intact and explicit `frame_readback_verified=false`.
- Safari front document was `http://127.0.0.1:18743/`; the genuine Sierra screenshot showed the rendered Die Maschine canvas and `THUNDERBOLT / HARDWARE ONLINE` label.
- Server process stayed near 1% CPU after startup; WebKit near 3% in the sampled settling period; transient Safari Safe Browsing startup CPU subsided.
- A screenshot of the Mac WindowServer is **not** proof of actual optical monitor reception. The owner confirmed the direct Mac/Samsung route functioning before the score, but has not yet confirmed this particular new creative screen visually.
- Original Blackmagic `OUTPUT_DISABLED` marker remained intact. No playback/capture process was started.
- The experimental session is temporary and self-terminates; not an installed background agent.

## Defined limits
This implementation uses the **genuine PCIe/DeckLink status channel**, not a captured video stream, a physical HDMI output, or proven DMA frame-buffer readback. The animation's changing pixels are created by the Mac. Whether the board supports an internal video test generator, hardware frame readback or non-HDMI media flow remains **unverified**; none are presumed absent or present.

No physical video, signal lock, frame capture, optical screen result, kernel-driver independence or internal loopback is claimed here.

## Reproduce safely
On the original Sierra Mac, with the existing `OUTPUT_DISABLED` marker intact and the repository checked out:

```sh
sh tools/sierra/run-intensity-thunderbolt-score.sh
```

In that Mac's Safari, open `http://127.0.0.1:18743/`. The page offers Freeze, optional local sound and Stop. The server is bound to loopback and terminates after at most ten minutes. No GitHub CI or driver installation is involved.

This is a **creative, hardware-grounded read-only experiment**; the separate independent A124 driver programme remains distinct.
