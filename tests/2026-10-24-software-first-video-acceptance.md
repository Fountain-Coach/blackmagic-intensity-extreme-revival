# Video instrument software-first acceptance — 24 October 2026

**Owner decision:** Stop making video development dependent on a powered Nikon D5200, Blackmagic HDMI input/output, the Sierra driver or historical Syphon. Model and verify every meaningful application-facing function with complete *software* instruments. Camera is OFF for this acceptance.

## Outcome

The repo now contains an independent full-stack video/audio simulator (virtual camera, virtual receiver, virtual transmitter, exact memory link), a browser test laboratory, a bounded synthetic raw-media interchange archive, and a repeatable HTTP acceptance command. The real Sierra Fountain Video.app now **starts with generated video by default**, with hardware-format controls disabled, no false NO SOURCE overlay, and a button to open the full Ubuntu-based lab.

This acceptance is for **software behavior** only, not actual camera/HDMI capture.

## Available services, intentionally separate

- Earlier single-device simulator: Ubuntu Mac mini 192.168.8.50 port **8172**, unchanged.
- Full video laboratory: Ubuntu Mac mini 192.168.8.50 port **8272**, source-owned Python HTTP server bound to private interface.
- URL usable from the connected Macs on trusted WLAN: http://192.168.8.50:8272/lab
- Sierra native app: /Users/bene/Applications/Fountain Video.app
- Its Software lab button: user-defaults FountainVideoLabURL=http://benedikt-macmini.local:8272/lab, verified accessible with HTTP 200 from Sierra.

This server remains intentionally unauthenticated and private-network-only, with no real-device API or privileged access. It must not be forwarded or published into the public Estate in this form.

## Tested simulator behaviors

1. Read-only device capabilities: two virtual half-duplex instances, 16 declared historical frame modes, synthetic provenance.
2. Virtual camera OFF: no frames, no preview available; camera power restore produces software-only video and PCM, on a monotonic timeline.
3. Video+audio integrity: complete generated packed 2vuy video and stereo signed 16-bit PCM, validated by CRC and timestamp.
4. Archive integrity: bounded last-six full-size raw video frames plus paired audio packets and manifest, declared SYNTHETIC.
5. Format changes: camera mode changed from 720p50 to 1080i50 with continuity of master PTS and sample timeline; interlaced *timing* only.
6. Virtual transmitter → receiver loopback: exact video bytes, audio bytes, video/audio PTS equality across two separate simulated devices.
7. Single-device half-duplex: capture+playback request correctly rejected using observed conflict code 0x80000009.
8. Output underflow: the preview actively withdraws stale output rather than showing a previous frame as current; scheduling a new packet restores output.

All eight tests are executed through actual HTTP routes in the repeatable file scripts/accept-software-video-lab.py, and return to a usable, running synthetic two-device demo afterwards.

## Test witnesses

Full local simulator suite:

    python3 -m unittest discover -s simulator/tests

**70 / 70 tests passed** on:
- Ubuntu Python 3.12.3
- Modern macOS Python 3.14.0

The source-owned browser JavaScript passed Node 24's --check syntax validation. Browser preview was visually checked on modern macOS using a headless browser: both separate monitors rendered a generated moving shape with the same per-frame CRC. Source raw PNG endpoints returned valid 320×180 images derived from full-size 1280×720 2vuy frames.

Live software lab acceptance command:

    python3 scripts/accept-software-video-lab.py --url http://192.168.8.50:8272

Expected output after version supporting output-underflow scenario:

    stage=full_software_lab result=pass scenarios=8 hardware_access=0 real_camera_required=0

Native macOS Sierra source build and synthetic tests:

    sh apps/fountain-video-sierra/build.sh
    build/Fountain Video.app/Contents/MacOS/FountainVideo --self-test
    build/Fountain Video.app/Contents/MacOS/FountainVideo --self-test-window
    build/Fountain Video.app/Contents/MacOS/FountainVideo --self-test-movie

The native GUI test generated a frame and verified its visible state without a NO SOURCE overlay; its format selector is disabled for the virtual source. The actual Sierra GUI was installed, launched and privately visually inspected. The 12-frame synthetic QuickTime MOV encoder test previously wrote and validated a real movie file, deleting the temporary fixture; it was not a physical camera recording.

## Hardware unchanged

The original Blackmagic Desktop Video 10.4 kernel drivers remained in place. The independently authored IOKit extension has never been loaded. The problematic CoreMediaIO DAL plugin remained isolated. No output monitor timing was changed, no HDMI port accessed, and neither a firmware update nor PCI access was involved. The Nikon was switched off; the simulation never used it.

The historic Black Syphon r4 / VVBlackmagicKit → Syphon → Vuo → NDI route and real-source capture issue stay as **separate historical and physical adapter evidence**, not test prerequisites. The two device loopback is in **software memory**, not a prohibited physical self-loop on the half-duplex card.

## Honest limitations

The simulator does not implement physical HDMI voltage/timing negotiation, accurate interlaced video fields, proprietary firmware or DMA, real analog conversion, HDCP decryption, GPU Syphon publishing or a real NDI server. The browser's 48kHz stereo PCM and YUV frames are generated and exact, but the ZIP is a raw interchange archive, not a fully encoded user playback movie. Future consumer/Teatro integration should use this owned media contract; a genuine physical adapter must pass separate hardware tests before claiming a real frame.

No GitHub CI or public deployment. Git repo holds the source and dated verification.
