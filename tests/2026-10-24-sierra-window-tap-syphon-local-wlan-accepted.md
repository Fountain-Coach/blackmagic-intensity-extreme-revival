# 2026-10-24 — Fixed-silver window capture, cross-process Syphon, existing WLAN

## Operator-owned topology, unchanged

This project's source machine is the **original Macmini5,3 on Sierra 10.12.6**. Its native HDMI OUT connects to the original Blackmagic Intensity Extreme HDMI IN. The Intensity returns by Thunderbolt through the Comdis i-tec THUNDERDOCK2 to the **same** silver Mac; the Blackmagic HDMI OUT currently remains unused. The i-tec HDMI OUT drives the Samsung monitor. The physical connections, installed original Desktop Video 10.4 driver, firmware and operating system were unchanged during this work.

The original Blackmagic PCI BDBD:A124 still enumerates. Its live HDMI signal-lock problem remains a separate unaccepted hardware test. The local Alpine/xhyve Docker guest remains stopped after the earlier Sierra reboot and is not needed for the existing network browser scratch host.

The existing WLAN route of the silver Mac is en1 / 192.168.8.23. The previously installed Ubuntu Scratch receiver remains at 192.168.8.50:8273 and is accessed through its existing 64-character bearer credential, not through a new service or physical port.

## The selected software boundary

The user requested selecting an application or defined rectangle, rather than capturing the entire monitor. Syphon is used as a **real interapplication GPU transport** on Sierra, not as an analogy or source of application pixels. Classic CoreGraphics supplies a specifically selected on-screen application WINDOW's pixel content; a source-owned Fountain Window Tap publishes that content to a local Syphon server. An optional **synthetic-only** path reuses the existing FCScratchPublisher JPEG transport over the trusted private WLAN to the already-running browser scratch service.

Legacy OpenGL is required only at the old Syphon adapter. Modern Metal/ScreenCaptureKit is a possible **optional provider on another supported Mac**, not grounds to move capture away from the silver host, replace Sierra or change the Thunderbolt/HDMI cables. Blackmagic DeckLink is a separate hardware input/output adapter, not a prerequisite for selected-window acquisition.

Software paths:

    SELECTED VISIBLE WINDOW / RECT -> Sierra CoreGraphics -> Fountain Window Tap
                                                  |           |
                                             LOCAL SYPHON   optional SYNTHETIC
                                             GPU / IOSurface   JPEG transport
                                                  |           |
                                           Syphon clients  192.168.8.50:8273
                                                              |
                                                     Browser Scratch on WLAN

## Source-owned files

- tools/sierra/fountain_window_tap.m: original-silver-only app-window discovery, exact window ID and owner PID gate, selected pixel rectangle, generated test window, optional explicitly saved private snapshot, local Syphon publisher and synthetic-only WLAN acceptance.
- tools/sierra/fountain_syphon_fixture_receiver.m: independent Sierra process that subscribes only to the synthetic Fountain Window Tap Test server and reads actual received GPU pixels from Syphon's IOSurface-backed texture through an OpenGL framebuffer (not the ineffective naive direct texture read).
- scripts/sierra-window-tap-crossprocess-test.sh: second-process synthetic window detection and explicit owner/window ID checks, changing frame hashes, cropped frame verification, bad region and wrong owner rejection.
- Reused unchanged apps/fountain-video-sierra/FCScratchPublisher.h and .mm: existing bounded JPEG sender, session UUID, monotonic presentation timestamps, one upload at a time, bearer credential and HTTP 201 acknowledgement.
- Existing original simulator/scratch.py and scratch_server.py: original bounded RAM frame ring and authenticated ingest, not replaced.

Syphon Recorder, Black Syphon and SyphonInject were already installed. The installed x86_64 Syphon framework was copied to the private development lab only for linking. The installed Fountain Video GUI, Black Syphon and Vuo compositions were **not** overwritten or launched by these tests.

## Measured original Sierra acceptance

**1. Internal moving-window test:** The original Mac compiled and ran the native tool. The generated visible 640×382 window yielded two distinct raw RGBA SHA-256 values, exit 0, no disk screenshots and no network transfer. First two digests:

- f2c928f26615eb055d76f1c315699e0cdd3095e329cb400408d2d7a1a4bd8ec2
- 61f1daf97725518d55c8b8bbc18fca9fcdedde8766baf35afb7ea61894bbb1b3

**2. Syphon server and independent receiver:** The original native Syphon API accepted 20 generated-frame publication calls. A distinct process connected to the synthetic server. The first naive receiver reported 20 identical zero-texture readbacks and **FAILED**. After replacing the unsupported direct read with an OpenGL FBO attachment and real glReadPixels, the independently executed receiver accepted 3 distinct 640×382 GPU frames, independently hashing and exactly matching all three publisher raw RGBA source hashes. Result: PASS, textures_received=3, unique_gpu_readbacks=3, gpu_read_errors=0. Third digest:

- a5745410a40fb41fa875a0919e27f407fef6a109355dbea224f67042f9cd9d33

The receiver did not create screenshots or send images over a network.

**3. Independent *other-process* selected-window capture:** The script launched a separate generated moving window. Another process captured the exact named window ID under its true owning PID twice with different pixel digests; extracted the explicitly requested 128×96 subrectangle; rejected a rectangle outside the window; rejected malformed rectangle syntax without falling back to whole-window capture; and rejected a mismatched owning PID. Result: PASS, no disk pixels, no Syphon publication, no WLAN transfer. This establishes external process capture without reading any real private app.

**4. WLAN synthetic transport:** With the existing Sierra Fountain Video scratch endpoint/secret from its private settings, the generated fixture published 12 local Syphon GPU frames and sent 12 source-labeled synthetic JPEGs using the existing authenticated FCScratchPublisher. Result: PASS, delivered=12, failed=0. The current LAN browser Scratch viewer is still HTTP and has no separate viewer authentication, so **NO real user application pixels were sent**.

**5. Independent WLAN receiver proof:** A distinct connected Mac requested the real receiver's /api/scratch/state. It verified one consistent synthetic epoch with 12 sequential frames, increasing sequence/PTS, simulated=true and physical_hdmi_verified=false. It then fetched the last /api/scratch/frame/SEQ.jpg and independently checked complete JPEG markers and expected exact 31,385-byte length. Result: PASS, independent replayable Scratch JPEG actually retained. No M2 application capture was substituted: the video source was the silver Mac.

## Privacy, capability and open gates

The actual chosen application window is never automatically recorded, published or sent over WLAN. The command-line --probe option only accesses exact PID and window ID and optional crop, --snapshot requires a deliberate filename in a private owner-only directory, and --publish requires an explicit --allow-local-syphon flag. Local Syphon servers are discoverable by other applications on that Mac; they are **not a protected private viewer**. Only --fixture-wlan may transmit network frames, which are generated synthetic images. Legacy synthetic PNGs from earlier tests may remain in the private lab, but this final accepted suite wrote no new screenshots.

The bounded WLAN scratch leg sends JPEGs (up to 640×360 and about 5 fps), not uncompressed GPU textures, NDI, WebRTC or production full-frame-rate video. The network viewer requires security controls and encrypted transport before real private application pixels may be enabled. Original Blackmagic HDMI input lock, output optical acceptance, GUI window picker and a continuous high-performance LAN feed remain separate open tasks. The original optional Linux Docker guest remains stopped.

**Verdict:** ACCEPTED on real original silver Sierra hardware: selected moving window capture, different-process window and crop selection, real cross-process Syphon pixel exchange, authenticated synthetic WLAN transfer and independent remote JPEG readback. No changes to any physical cable, driver, firmware or Sierra. PR #10 remains DRAFT while real application UI selection, private network viewing and HDMI acquisition remain open.
