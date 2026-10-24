# HDMI-to-browser frame streaming and scratching

**24 October 2026 — software transport working; actual HDMI capture not yet verified.**

## Open browser scratch

**http://192.168.8.50:8273/scratch** — private Fountain Coach WLAN, not a public website.

The native Sierra app at /Users/bene/Applications/Fountain Video.app now offers Browser scratch… and Video lab… buttons, plus a Stream frames to private browser checkbox. It starts with moving synthetic footage, not hardware probing. Both synthetic images and images from the optional DeckLink callback use the **same publication method**.

## Implemented media graph

    Nikon HDMI OUT → Intensity Extreme → existing Blackmagic 10.4 driver
         → DeckLinkAPI callback (when source frames are actually delivered)
              |
              +----------- Native Fountain Video receiveRGBA -----------------+
                                                                              |
    Native AppKit generated moving pattern → receiveRGBA ---------------------+
                                          |
                                  FCScratchPublisher
                               source label + timestamp
                              640x360 JPEG quality ~0.65
                        up to ~5.8 fps, one HTTP upload in flight
                                          |
                         Bearer-authenticated HTTP JPEG POST
                                          |
                        Ubuntu server 192.168.8.50:8273
                        Immutable indexed frame ring:
                          up to 144 frames or 24MB
                        Session and timebase discontinuity handling
                                          |
                         /api/scratch/state and frame/<seq>.jpg
                                          |
                         Browser canvas + reversible playhead
                         drag, freeze, reverse, forward, live

**This is JPEG frame streaming, not H.264, WebRTC, MJPEG multipart, RTP, or full-frame-rate production HDMI streaming.** Source-agnostic browser scratching requires random access to recent compressed frames; a live-only multipart video stream cannot rewind. Live audio is not transmitted through this scratch channel. The independent software video lab on port 8272 already models 48kHz PCM and rational video/audio timing.

## Exact browser controls

The frame cache supports mouse/pointer drag to jog, timeline range slider, eight-frame filmstrip, one-frame steps, reverse/forward play and return to live. Speeds 0.5x, 1x, 2x and 4x are supported. Keyboard: J reverse, K freeze, L forward, Left/Right one frame, Space live/freeze.

Frames are explicitly marked SYNTHETIC or DECKLINK OBSERVED. The latter is not an independently proven HDMI signal: the browser and frame headers always report that HDMI physical acceptance is not complete. If the source stops, existing frames may still be scratched but the source status becomes offline.

A process restart creates a fresh stream-session UUID. A source switch, stream-session change or significant presentation-timestamp reset begins a new cache epoch. Late out-of-order packets from the same session are discarded rather than flushing the valid history. Source frames are serialized from the native app.

## Ingest and privacy

The receiver runs with Python 3.10+ standard library:

    python3 -m simulator.scratch_server --host 127.0.0.1 --port 8273

For trusted private WLAN access only, specify --host 192.168.8.50. Do not bind the unauthenticated *viewer* to a public Internet address. The sender POST endpoint requires a 32+ character bearer token, stored only locally on Ubuntu (private file ~/.config/fountain-video/scratch-ingest-token, mode 0600; directory mode 0700) and in the Sierra app's user defaults. The secret is never distributed via Git or logs.

The Sierra configuration keys are FountainVideoScratchEndpoint, FountainVideoScratchPageURL and FountainVideoScratchToken in the coach.fountain.video defaults domain. Ingest requires Content-Type image/jpeg, PTS, origin, frame dimensions and stream UUID; single requests are capped at 1MiB. Frames are held only in bounded memory and evicted from oldest first.

**Security limitation:** Video and the ingest bearer token traverse unencrypted HTTP on the explicitly trusted private WLAN. Viewers on that subnet can read frames without a password. Do not expose actual sensitive pictures or route port 8273 to the public Internet. Add TLS and viewer authentication (or a controlled SSH tunnel) before any wider publication. This is not an estate-publication endpoint.

## Evidence from actual machines

- Native macOS Sierra 10.12.6 app compiled with Xcode 9.2, deployed and opened. Independent JPEG encoder self-test generated and decoded a valid 640x360 compressed image; the generated source now includes a clear moving square and visible binary frame sequence.
- Synthetic frames from the actual running Sierra application were POSTed across the LAN into the separately running Ubuntu receiver, stored as indexed JPEGs, then displayed in a headless Chrome browser on the modern Mac. These are **real transported bytes**, not an HTML placeholder.
- A first suite of 14 scratch-specific tests passed on both Ubuntu and macOS; after adding timebase and stream-session tests, the combined virtual video and scratch suite passed **88 tests** on Ubuntu Python 3.12 and macOS Python 3.14.
- A real Chrome DevTools Protocol acceptance test ran **eight user interactions** successfully: live ingest, freeze, backward frame step, reverse shuttle, forward shuttle, return-to-live, drag-to-jog, and J/K/L keyboard control. Source: tests/browser_scratch_cdp.mjs.
- The original Blackmagic 10.4 drivers remain loaded and the independently authored IOKit prototype remains unloaded. The Nikon was OFF; **no physical HDMI frame** was demonstrated.

## Hardware completion requirements

To call the physical device revived, we must still witness at least one genuine source frame arriving through DeckLink and appearing in the browser with the correct capture provenance, timing and pixels. HDMI output is a separate physical acceptance. Passing synthetic frame-stream tests does not satisfy either. No work in this streaming implementation requires that the Nikon be powered on.

## Owned implementation

- apps/fountain-video-sierra/FCScratchPublisher.h and FCScratchPublisher.mm — native bounded JPEG publisher
- apps/fountain-video-sierra/FountainVideo.mm — source-neutral frame publication; browser launcher
- simulator/scratch.py — time-coded in-memory frame ring
- simulator/scratch_server.py — token-protected ingest and indexed browser HTTP API
- simulator/web/scratch.html — drag, transport and timeline controls
- simulator/tests/test_scratch_server.py — software-only API, retention, provenance and epoch tests
- tests/browser_scratch_cdp.mjs — actual browser interaction acceptance

Git-owned source; no GitHub CI, no proprietary SDK copy, no changes to boot volumes or firmware.
