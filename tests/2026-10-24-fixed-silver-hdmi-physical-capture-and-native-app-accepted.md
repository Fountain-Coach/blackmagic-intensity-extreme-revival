# 24 October 2026 — ORIGINAL INTENSITY EXTREME HARDWARE HDMI CAPTURE ACCEPTED ON FIXED SILVER LOOPBACK

**ACCEPTED (physical acquisition, independently owned pixel memory, host timestamps, private JPEG output, native Fountain Video app and separate Ubuntu Linux Docker ingest).**

**NOT CLAIMED:** sustained full-rate LAN or local guest streaming; Blackmagic device-native presentation timestamps; broader source compatibility; new driver or firmware; publicly verified picture content; independent kernel driver implementation.

## Single user-authorized physical fixture

- **Silver 2011 Mac mini**, Macmini5,3, Sierra 10.12.6, remains actual boot OS.
- **THE SAME SILVER MAC HDMI OUT directly to ORIGINAL Blackmagic Intensity Extreme HDMI IN**, unchanged. Blackmagic returns to silver via **Thunderbolt 1 / Comdis THUNDERDOCK2**, PCI 0xBDBD:0xA124.
- Existing original Blackmagic Desktop Video **10.4** KEXT loaded, without replacement or reboot. Mac's output display "BMD HDMI" is live at exact **1920×1080@30**.
- No alternate external camera, independent HDMI generator, HDMI cable rearrangement, MacBook HDMI substitution, firmware update, KEXT installation, PCI BAR writes, privileged networking change or driver replacement.
- This is the **FINAL physical test fixture**. Remove all suggestions to test different HDMI sources.

## Verified physical hardware lock — 1080p30, 2vuy

Source-owned `src/decklink-mode-armed-no-stream.cpp` ran on real silver:

    stage=enable_video_input rc=0x00000000 requested=1080p30 pixel_format=2vuy
    stage=mode_armed_status step=0 lock_rc=0x00000000 lock=no
    stage=mode_armed_status step=1 lock_rc=0x00000000 lock=yes
    stage=mode_armed_status step=2 lock_rc=0x00000000 lock=yes
    stage=mode_armed_status step=3 lock_rc=0x00000000 lock=yes
    stage=disable_video_input rc=0x00000000
    stage=complete exit=0 stream_started=0 physical_frame_captured=0

Independent lock-gated observer also confirmed `locked=1` AFTER StartStreams. Original legacy observer still SEGFAULTed; new crash report inside vendor library:

    CDeckLinkVideoInputFrame::~CDeckLinkVideoInputFrame()
    CDeckLinkVideoInputFrame::Release()
    CDeckLinkInput::inputFrameArrivalCallack()

The original fixed HDMI loopback **does lock**, independent of display EDID interpretation. Root cause narrowed to callback operations.

## Differential vendor-frame accessor tests on physical Sierra Mac

All tests arm exact 1080p30, wait for **two consecutive successful lock signals**, then run a bounded capture and cleanly stop if possible.

| Isolated operation | Observed result |
| --- | --- |
| No vendor frame access | 9 frame callbacks; clean stop/release; exit 0 |
| `GetFlags` | flags=0; **no_source=0**, 9 callbacks, clean exit 0 |
| `GetWidth/Height/RowBytes` | 1920 × 1080, rowBytes=3840, 10 callbacks, clean exit 0 |
| `GetBytes` without dereference | rc=0, non-null pixel pointer, 9 callbacks, clean exit 0 |
| Read first 256 pixel bytes | rc=0, FNV32=0xc449e924; 10 callbacks, clean exit 0 |
| `GetStreamTime` | unexpected rc=0x00001d03; SIGSEGV in old Blackmagic library during subsequent frame release, exit 139 |

**Repair:** remove vendor frame `GetStreamTime` calls from callback entirely, copy the vendor-borrowed pixel buffer **inside** callback lifetime, and create presentation timestamps using **Sierra's mach_absolute_time** (host monotonic clock, NEVER described as the device's native timestamp). Decouple expensive RGBA conversion/preview from the callback and bound pending UI work. The unsafe original frame accessor is not called in repaired code.

## Two COMPLETE, DISTINGUISHABLE real physical frames captured without crash

`src/decklink-lock-gated-owned-frame-copy.cpp` accepted after successful lock and captured **two independent complete 1920×1080 UYVY frames**, each **4,147,200 bytes**, directly copied from Blackmagic's `GetBytes` into separately owned memory.

| Callback | Host monotonic time in nanoseconds | Full raw UYVY SHA-256 |
| --- | ---: | --- |
| 1 | 203556705383643 | `68aaba136af707a668ca6c147b0dd98ca1ff00fa876e82f7c902dba857268cb7` |
| 30 | 203557671837976 | `a4de7436a3fb9d73fda5a7b68de16ba596a98337a4ff48af23cbb613e70f06be` |

Both are `GetFlags`-valid, and content hashes differ, by approximately **966.454 ms**. The vendor input stream stopped, callback removed, device disabled; application exited 0 with no crash, no vendor `GetStreamTime`.

## Encoded PHYSICAL JPEGs — native Sierra

`src/decklink-lock-gated-jpeg-preview.cpp` independently repeated this capture, copied full-size physical frames and, AFTER clean capture shutdown, converted UYVY to 320×180 RGB/JPEG via native CoreGraphics/ImageIO.

- Frame 0 private JPEG: `/tmp/fc-intensity-extreme-physical-frame0.jpg`, **30,578 bytes**, SHA-256 `ea1f51836311a496d91f493f82bb9e380f41efff76992ef8438cea6511c562d3`.
- Frame 1 private JPEG: `/tmp/fc-intensity-extreme-physical-frame1.jpg`, **30,532 bytes**, SHA-256 `ab1abfddb80eb4a7d1404272aefed88d55373fd344dbb1debdc63a19c0c32634`.
- Physical raw frame content SHA differed and host monotonic timestamps differed. Both private JPEG files are mode **0600**, never published to Git or opened to the public.

## Real physical frames actually accepted in Linux Docker

Via ordinary authorized encrypted SSH file transfer, staged the two private JPEGs to the **separate Ubuntu host** in a private directory (`/tmp/fc-private-blackmagic-physical/`), and inside an existing local-only Docker container `fountain-video-scratch-scratch-1` authenticated JPEG POST to its **localhost** `/api/scratch/ingest` with private local token (token never printed) and source `decklink-observed`. Both POSTs received HTTP 201:

    dockeringest_status=201 seq=0 origin=decklink-observed jpegbytes=30578 ... simulated=False
    dockeringest_status=201 seq=1 origin=decklink-observed jpegbytes=30532 ... simulated=False

Receiver `http://127.0.0.1:8373/api/scratch/state` returned:

    count=2 oldest=0 latest=1 origin=decklink-observed simulated=False physical_hdmi_verified=False

Receiver per-sequence JPEG GET returned **byte-for-byte original source hashes**, both confirmed independently, and its `/scratch` browser view returned HTTP 200.

**Scope:** this is a successful controlled physical-capture → JPEG → encrypted private transfer → actual Linux Docker ingest pipeline. It is **NOT continuous live-rate network streaming**, and there is no claim that the separately booted **silver-local Linux guest** received these physical frames. Its experimental COM2 UART transport is far too slow (6 seconds/718 bytes). Existing separate Ubuntu staging localhost port 8373 and public-LAN original 8273 were kept distinct; the latter retained its synthetic history and was not altered by physical tests. `physical_hdmi_verified=False` in receiver is intentional: the receiver cannot physically witness the original source; independent hardware witnesses are in this report.

## Actual native Fountain Video application repaired AND tested physically

`apps/fountain-video-sierra/FountainVideo.mm` now:

1. Prohibits legacy vendor `GetStreamTime` call; uses host monotonic ticks.
2. Copies pixel bytes while callback holds borrowed frame; schedules bounded 5fps RGBA conversion/rendering in own memory on AppKit main thread.
3. Selects exact **1080p30**, requires BMD HDMI 1920×1080@30 mode and **two stable hardware lock polls before StartStreams**.
4. Forces private default: when hardware source selected, **"Stream frames to private browser" is OFF**, as the user-fixed source is the Mac's main display.
5. Preserves synthetic simulation as a fallback, without touching hardware if BMD source isn't detected.
6. CLI `--self-test-fixed-silver-hdmi` actually invokes native AppKit hardware preview, checks real 1920×1080 frame image, clean shutdown, no network publication.

**Actual silver hardware native application test** (source at commit `6c92aca`):

    stage=native_fixed_hdmi result=pass actual_rgba_preview=1 dimensions=1920x1080 private=1 clean_teardown=1 device_timestamp_called=0
    exit=0
    logs: Real video frame received (repeated)

Synthetic converter, JPEG and window regression tests also passed. Candidate built in Sierra Xcode 9.2, no root/kernel changes.

The original installed app was gracefully quit, original backup stored privately in **both** `/Volumes/Fresh/FountainVideoDockerLab/native-app-backups/Fountain Video.original-20261024.app` and `~/Applications/.FountainVideo-original-20261024.app`. After a first repaired candidate was accepted, a final build (`6c92aca`, SHA-256 `f4536470ad92f9df17ec759f52f3f4322b59413dcf8ccf99101b46a52348f758`) was copied atomically with an additional rollback `~/Applications/.FountainVideo-working-repair-20261024.app` into **`~/Applications/Fountain Video.app`**. Installed binary synthetic self-test passed. `open -a ~/Applications/Fountain\ Video.app` succeeded and PID 8814 remained live, loaded existing DeckLinkAPI and showed significant process activity. Source code now **automatically selects private Blackmagic HDMI input at startup, only when the exact fixed silver BMD 1080p30 mode is detected**, otherwise simulation fallback.

**No additional OS, driver, firmware, HDMI source, Thunderbolt topology, or PCI changes.**

## Remaining acceptance tasks

- Confirm stable, sustained live native GUI capture, CPU and privacy state across a longer bounded session, not only ~2 seconds. Although installed app launched and loaded DeckLink runtime, the separate CLI AppKit physical test was the independently logged proof of real hardware preview.
- Integrate a consent-controlled private **live** physical-frame delivery to **Ubuntu Docker** as opposed to the successful but offline-controlled two-JPEG file transfer. The silver's default LAN sharing remains off because its own main display would otherwise be exposed.
- Explicitly separate original device timestamp fidelity from host-monotonic stamp: no unsupported claim that recorded timing derives from DeckLink hardware.
- Do not repeat the known-crashing `GetStreamTime`, reinstall Desktop Video 10.11.2, flash firmware, replace Sierra or rewire HDMI.

**Conclusion: Hardware revival at acquisition and private preview level ACCEPTED. Continuous external Docker live capture pipeline is separately OPEN.**

## Post-install live window and source-pixel inspection

The final installed `~/Applications/Fountain Video.app` was launched as real GUI process **PID 8814** on the silver Mac. Approximately **4m14s** after launching, `ps` confirmed it still running with DeckLinkAPI framework loaded and Sierra/Desktop Video 10.4 unchanged. No newly timestamped FountainVideo crash reports appeared during that observation. A native read-only CoreGraphics window-list query (without capturing any desktop pixel data) reported **one onscreen window owned by PID 8814**, titled **Fountain Video**, dimensions **1120 × 722**, positioned at X400 Y88. The separate native hardware AppKit test was the direct confirmation of real pixels.

Private decoded JPEG previews were independently analyzed on Ubuntu (no images published). Both were valid **320×180 RGB** JPEGs. Their decoded contents differed at **1,064 of 57,600 pixels**, with mean absolute RGB differences approximately (0.172, 0.170, 0.318), and different complete-frame and JPEG SHA-256 hashes. This provides more evidence than different hashes alone: a stable physical desktop captured at two different moments can change in a small subset of pixels.

## Hardened fixed-rig acceptance runner

`scripts/accept-bmd-hdmi-capture.sh` now canonically uses **only** the fixed silver HDMI source. Its default `--check` queries actual Sierra 10.12.6 host/model, original driver10.4, BMD HDMI 1920×1080@30, and presence of installed native application. Live run returned `source=ready`, `native_capture_app=running`, **without starting any second DeckLink client**.

Its explicit `--run` is fail-closed when the installed GUI is already running. That guard was independently tested live and returned exit **40** (`client_already_running`, `no_concurrent_vendor_access=1`). The command no longer uses a MacBook BMD EDID, old Nikon mode scan or crashing `GetStreamTime` observer. `scripts/sierra-hdmi-loopback-preflight.sh` also distinguishes its no-vendor-call inspection from the separate **already accepted** physical capture witness, avoiding a false `NOT_ACCEPTED` summary.

## Confidential live Docker publishing — separately unaccepted

An encrypted, two-hop **loopback-only SSH forward** was temporarily established and actually verified from the silver Mac to `127.0.0.1:8373` in Ubuntu Docker, without granting Sierra `vmnet` entitlements or exposing a LAN listener. A new **explicit opt-in** `--self-test-fixed-silver-hdmi-live-docker` candidate was compiled. With no private receiver credential made available to that candidate, it correctly **failed closed before hardware I/O** (`result=missing_private_secret`, exit **71**). No secret was copied to Sierra, no private desktop frame published by this candidate, and both temporary SSH tunnels were closed by their control sockets.

This means the user-owned native **capture and preview are functional and installed**; **unattended live HDMI-to-Docker streaming has NOT been accepted and was NOT enabled**. The two earlier physical JPEGs accepted inside Ubuntu Docker remain the valid bounded real-frame transport witness. Avoid silently enabling network sharing of the silver Mac's primary display.
