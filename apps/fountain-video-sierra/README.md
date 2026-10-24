# Fountain Video — native Intensity Extreme application for Sierra

**Version:** 0.1 · 24 October 2026  
**Target:** macOS Sierra 10.12.6, 2011 Intel Mac mini, Xcode 9.2  
**Status (24 October 2026): ORIGINAL HARDWARE HDMI CAPTURE ACCEPTED.** The silver Mac mini continues booting **Sierra 10.12.6**, drives the **same physical HDMI cable from its HDMI OUT into the original Blackmagic Intensity Extreme HDMI IN**, and owns the device over native Thunderbolt 1. We have **genuinely captured separate full 1920×1080 YUV frames, encoded them as private JPEGs, accepted them in separate Ubuntu Linux Docker staging**, and physically tested this **native Fountain Video AppKit application** with real RGBA preview and clean stream/device release, all using original untouched Blackmagic driver **10.4**. [Complete hardware acceptance](../../tests/2026-10-24-fixed-silver-hdmi-physical-capture-and-native-app-accepted.md).

## NEW: original HDMI OUT idle-sync repair installed (24 October)

The external monitor on the original Intensity Extreme HDMI OUT was reported switching on/off repeatedly. The original Desktop Video 10.4 **default output was 1080i50 (Hi50)**, inconsistent with the silver's fixed 1080p30 source and tested output playback. An uninterrupted vendor test accepted **918 YUV frames in 40 seconds**, current device output remained exactly 1080p30, and no API errors occurred. The correct least-invasive fix is to hold default idle output **1080p30 (Hp30)** while Fountain Video owns the card, keeping idle picture **BLACK**, not a potentially private last desktop frame.

The installed native application implements this via the existing DeckLink user-space configuration interface: on the exact silver HDMI rig only, compare existing default Hi50 and idle black, set Hp30 for this app's session, retain configuration reference, and explicitly restore previous Hi50 on a normal close **only if the app made that change and no other client changed it since**. If Hp30 was already configured, adopt without claiming ownership. The original card refused writing permanent preferences as an ordinary Sierra user, so **no administrator privileges, driver edits or preference writes** were used.

This change was compiled and passed codec/window/scratch tests and an actual original-device test beginning at Hi50/black, reading Hp30/black while open, and independently confirming Hi50/black after close. Installation and repeat **quit→restore→relaunch→Hp30** acceptance passed. Four independent live reads during the installed GUI session showed Hp30/black and status mode Hp30, with mailbox head 17 unchanged (no automatic sharing). Exact installed executable SHA-256: `3804867ec6298b771df646f3b2fcf4be7ec799c0e669a61838f324af0b168f18`, installed at `~/Applications/Fountain Video.app`. Hidden rollback: `~/Applications/.FountainVideo-before-output-standard-20261024.app`.

**Important:** optical monitor on/off behavior has *not* been independently measured; the device exposes its video-output mode, not monitor pixels/power. The fresh physical HDMI IN capture lock regression and full record→monitor roundtrip remain open, as does unattended Linux Docker runtime. [Complete native repair and hardware acceptance notes](../../tests/2026-10-24-blackmagic-hdmi-output-idle-standard-native-repair.md).

## NEW: local HDMI → Linux Docker live mailbox (24 October)

**Bounded live capture to the silver Mac's own Linux Docker RECEIVER: ACCEPTED.** The source-owned Sierra `Fountain HDMI Frame Mailbox` at `127.0.0.1:8483` accepted **17 true live source frames**, zero failed uploads, while the separate Alpine Linux Docker guest independently reported **17 actual JPEG deliveries** with `origin=decklink-observed` and `simulated=false`. This bypasses the obsolete ~6-seconds-per-tiny-frame COM2 serial transport. The new 4 MiB mailbox is a disposable, checksum-validated **read-only virtual block device from Linux's perspective**, separate from all OS disks. [Full test witness](../../tests/2026-10-24-silver-local-physical-hdmi-docker-mailbox-accepted.md).

**New GUI privacy behavior, installed and tested:** On the real silver BMD HDMI source, the control reads **“Share HDMI with local Linux Docker”** and is **OFF** on launch and whenever a source changes. Turning it on builds a new **localhost-only** publisher from the existing Sierra private 0600 token file, never from the old LAN/synthetic endpoint or from NSUserDefaults. Turning it off invalidates that publisher. Different source types can no longer reuse one another's active network publisher. The native GUI revision passed synthetic/codec/window tests and an **independent real HDMI 1920×1080 AppKit preview test** before being installed in `~/Applications/Fountain Video.app`. The original repaired build was preserved as a private rollback copy. It was reopened with mailbox head still **17**, confirming the installation did not automatically send more desktop frames.

**Scope caution:** A 201 from the Sierra mailbox means committed into virtual block storage; only a separate **guest Docker receipt** proves acceptance by Linux. That independent proof passed for the 17-frame bounded test. The Linux console subsequently became unresponsive during extra Docker frame-GET inspection, which remains a durability issue. The guest viewer is still **inside an offline container network namespace**; the GUI's old Browser Scratch link does not establish a publicly or locally accessible browser path to this particular guest. Continuous unattended reader operation and host-accessible browser controls remain pending. Do not call the guest itself a native PCI Blackmagic driver.

## Fixed physical HDMI loopback (no alternative sources)

**The rig is final:** silver Mac's own HDMI OUT → ORIGINAL Intensity Extreme HDMI IN → the SAME silver Mac over Thunderbolt via Comdis dock. Do not propose a camera, second Mac, generator or other video source as a substitute.

The original 10.4 DeckLink callback crash was narrowed by independent per-method tests. `GetFlags`, dimensions, `GetBytes` and bounded pixel copying work and return genuine frame pixels; invoking the old `GetStreamTime` accessor returned an abnormal result and reproduced a destructor crash. The **repair never calls that method**. Instead it copies each borrowed vendor frame into its own `NSData` during the callback, uses explicit **host monotonic timestamps** and decimates/previews frames via AppKit after crossing the callback boundary. It arms **exact 1080p30 / 2vuy**, waits for **two stable actual input signal-lock readings**, then starts streaming. On its own physical Sierra computer, the native application passed:

    stage=native_fixed_hdmi result=pass actual_rgba_preview=1 dimensions=1920x1080 private=1 clean_teardown=1 device_timestamp_called=0

Real native logs repeatedly reported **"Real video frame received."** Converter, synthetic JPEG and synthetic AppKit window tests passed as regressions.

**Installed and running:** `~/Applications/Fountain Video.app` is the tested repaired native application (candidate source commit `6c92aca`), not just a new diagnostic executable. It was installed after safely preserving private rollback bundles on both Data and Fresh, and launched successfully with the original DeckLinkAPI 10.4 runtime loaded. On launch it **automatically selects REAL HDMI** only if the exact BMD HDMI 1920×1080@30 display is verified; otherwise it uses simulated color bars. **Main-display privacy:** whenever real hardware is selected, `Stream frames to private browser` switches **OFF by default**, so the Mac's desktop cannot be sent to the network merely by launching this app.

## Browser Scratch integration

The separate Ubuntu **Docker staging** scratch at `127.0.0.1:8373` privately accepted **two actual physical JPEGs**, both HTTP 201 with `decklink-observed`, `simulated=false` and matching source-to-receiver SHA-256. The browser page responded HTTP 200. **These frames were uploaded in a controlled private transfer after capture**; continuous automatically streamed HDMI-to-Docker frames have **not yet been accepted**. The original trusted-LAN scratch receiver `192.168.8.50:8273` remains separate and its previous synthetic stream was not replaced. The silver Mac's local Linux/Docker guest uses a much slower COM2 serial transport suitable for integration tests, **not real-time video**. The existing private browser-share toggle is explicit opt-in for physical desktop frames.

## Historical 10.4 capture crash (now repaired for this fixed rig)

Earlier tests crashed in `CDeckLinkVideoInputFrame::~CDeckLinkVideoInputFrame()`; the original source-only preflight did not enter the video callback and could not establish physical pixels. Those tests are historical and **do not supersede** the full acceptance achieved above. The signed alternative Desktop Video 10.11.2 installer and original 10.4 rollback remain archived for recovery purposes; **no driver installation, firmware update or Sierra OS replacement was needed.**

The actual Sierra application now includes a **Browser scratch…** button and **Stream frames to private browser** checkbox. Its source-neutral frame path converts both generated video and optional DeckLink-received YUV frames into preview images, then uses [FCScratchPublisher](FCScratchPublisher.mm) for token-protected, bounded, single-flight JPEG uploads to the Ubuntu receiver. The user-owned application does not launch the receiver itself.

**[Browser Scratch](http://192.168.8.50:8273/scratch)** runs on the trusted WLAN. It supports a rolling 144-frame cache, reverse/forward shuttle, freeze, jog dragging and return to live. The [full design and security boundary](../../docs/hdmi-to-browser-stream-and-scratch.md) explains how source-provenance, stream epochs and media acceptance remain separate. The app now starts with the **verified fixed silver HDMI loopback** when it is present, while the synthetic lab remains a fallback. Browser publishing stays OFF for the Mac's main-display capture unless deliberately selected.

Compile and test the JPEG encoder using the app executable's `--self-test-scratch` argument. Unlike a cosmetic mock, the Sierra application was witnessed actually sending generated video bytes to the Ubuntu receiver, which were shown in Chrome on a separate Mac. The browser's eight interaction acceptance checks passed. Audio is not yet forwarded by this scratch transport.

## What the application does

Fountain Video is a native **AppKit application**. On the fixed silver HDMI loopback it opens the **genuine captured hardware preview** by default, with no LAN sharing. Without the verified BMD HDMI 1080p30 monitor, it selects virtual color bars and does not open the Blackmagic hardware. The full-stack software simulator remains separately available through the **Video lab…** button. Original `DeckLinkAPI.framework` handles real capture, without the isolated crashing CoreMediaIO plugin.

Its window provides:

- Verified **physical 1080p30 HDMI preview on the silver's final fixed loopback**, or visibly labeled virtual color bars as a safe fallback when that monitor is absent.
- **Auto · Nikon D5200 (6 formats)** (PAL: 720p50, 1080i50, 1080p25; NTSC: 720p59.94, 1080i59.94, 1080p29.97), plus manual exact-rate choices. Modes are tried sequentially, prioritizing PAL for the connected D5200; failed modes release the stream before the next attempt. No system monitor modes are changed. See the [D5200-specific setup](../../docs/nikon-d5200-hdmi-setup-2026-10-24.md).
- Native **Start/Stop**, with callback counts, no-source status, and clean stream release.
- Live video preview using 8-bit YUV 4:2:2 to RGBA conversion when the device actually delivers valid frames.
- **Save current frame…** to a user-chosen PNG path.
- **Record movie…** to a user-chosen QuickTime MOV path, encoded with Sierra's AVFoundation JPEG codec. Recording begins only on genuine video frames unless the user explicitly selects the virtual source. No image/audio/video is saved automatically.
- **Software lab…** opens the full two-device video/audio lab with virtual capture and output, rational timing, hot swap, fault injection, and downloadable raw media. Hardware format controls are disabled while the virtual source is active.
- On-screen device and video-state reporting. An absence of HDMI signal is shown as an ordinary state when optional hardware capture is selected.

Virtual frames are marked **SYNTHETIC · NOT CAPTURED**, including in default export filenames. Synthetic capture never counts as physical evidence.

## Location and reproducible build

The installed user-owned app is at:

    /Users/bene/Applications/Fountain Video.app

For a source-owned rebuild on the Sierra Mac mini (no root, third-party SDK, package manager or CI):

    sh apps/fountain-video-sierra/build.sh

Output:

    build/Fountain Video.app

This requires only Xcode 9.2 and the macOS system frameworks. The source tree does **not** distribute Blackmagic binaries or firmware; the vendor library is loaded only from its locally installed framework.

The full video lab button reads user-defaults key **FountainVideoLabURL**. If unset, the generic default is localhost port 8272. For the existing Fountain Coach WLAN installation, the account preference is already set to **http://benedikt-macmini.local:8272/lab**. The link only opens the simulation browser; it never opens HDMI or performs a hardware scan.

## Tests passed on the physical Sierra host

    --self-test
    stage=fountain_video result=pass suite=converter,virtual_bars,preview

    --probe
    stage=app_probe result=device_found locked=0

    --self-test-movie
    stage=movie_test result=pass frames=12 file_bytes=30305 source=synthetic codec=jpeg

    --self-test-window
    stage=app_window result=pass virtual_frame=1

    --self-test-autoscan  (HISTORICAL hardware baseline, not run in software-only lane)
    Scanning Nikon HDMI · format 1 of 6
    Scanning Nikon HDMI · format 2 of 6
    ...
    Scanning Nikon HDMI · format 6 of 6
    No signal detected in six Nikon HDMI formats.
    stage=auto_hdmi_scan result=pass finished=1 inactive=1 no_source=1

The movie test actually encoded twelve generated frames, verified the file and deleted the temporary test movie. The previous camera-on auto scan exercised the vendor input API and found no valid frames; **no HDMI test was performed after the user turned the Nikon off and selected software-first development**. Synthetic video never counts as physical capture.

The first command-line AppKit GUI test revealed an artificial autorelease-pool teardown crash because it did not run the normal NSApplication lifecycle. The CLI harness was corrected; software-only tests now pass with no hardware access. The application itself was separately launched into the graphical user session and inspected visually; its normal Quit Apple Event completed without a crash.

## Safety and known gaps

- Blackmagic Desktop Video 10.4 kernel components remain loaded; the independent IOKit kernel-extension prototype remains unloaded.
- The crash-prone system-wide CoreMediaIO plug-in stays isolated.
- No firmware, EEPROM, PCI BAR, DMA, startup volume, system display setting or modern MacBook setup was changed.
- **Real hardware HDMI preview physically validated** by native AppKit test; movie export is implemented but a separately encoded **full physical-source movie** remains to be accepted. Still no device-native timestamps or sustained Docker live frame-rate acceptance.
- HDMI **output** playback, audio recording, analog connector switching and a fully independently owned kernel driver are not implemented in version 0.1.

The app can control a connected device directly. No application can electrically supply an HDMI input signal without an actual connected source. That limit is represented by a clear NO HDMI INPUT SIGNAL state rather than a workflow requiring the user to manipulate another Mac.

## Owned components

- FountainVideo.mm — native AppKit GUI, dynamic DeckLink input client, YUV video preview, virtual frames, PNG and AVFoundation MOV export.
- Info.plist — bundle identity and Sierra platform boundary.
- build.sh — source-owned user-space build; no privilege escalation.

Source in this repository is authoritative. No GitHub CI or unsupervised installation.
