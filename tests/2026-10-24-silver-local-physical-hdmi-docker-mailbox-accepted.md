# 24 October 2026 — REAL silver HDMI → local Linux Docker over read-only virtio block accepted

**ACCEPTED: 17 real physical-source HDMI JPEGs delivered through Sierra → a disposable virtual block mailbox → the Linux Docker receiver running ON THAT SAME SILVER MAC.** This supersedes the earlier statement that only separate Ubuntu accepted physical JPEGs.

**NOT YET ACCEPTED:** reliable unattended 24/7 guest operation, a live browser endpoint accessible from macOS, full uncompressed 30fps video, hardware-native capture timestamps, audio or PCI passthrough. A later post-capture readback request via the guest's console became unresponsive; the positive transfer evidence below was independently obtained before that issue, which remains open.

## User's final, unaltered physical interface

- Original silver Macmini5,3 **HDMI OUT → original Blackmagic Intensity Extreme HDMI IN** and **Thunderbolt 1 / Comdis dock → the same silver Mac**.
- Original Sierra 10.12.6 **boot OS**, installed Blackmagic Desktop Video 10.4 untouched. The native hardware input was already proven to lock at **1920×1080@30**, deliver full UYVY frames, and pass native AppKit preview.
- The silver's own **xhyve Alpine Linux 3.24.3** guest runs Docker Engine 29.9.2. Sierra owns PCI and Thunderbolt; Linux/Docker receives media frames, **not** the PCI device.
- No changed HDMI source, alternative Mac or camera, Linux host reinstall, kernel-driver change, firmware update, root vmnet entitlement or external receiver dependency.

## New source-owned transport: Fountain HDMI Frame Mailbox v1

- Added `simulator/hdmi_frame_mailbox.py`, `tools/sierra/fountain_hdmi_mailbox_ingest.py`, `tools/linux/fountain_hdmi_mailbox_reader.py`, `scripts/silver-hdmi-mailbox-guest.sh`, `tools/sierra/fountain_hdmi_mailbox_bootstrap.py`.
- Created a new **4,194,304-byte**, file-mode **0600**, *disposable* regular file `/Volumes/Fresh/FountainVideoDockerLab/fountain-hdmi-mailbox.raw`. This is **NOT** a filesystem and is not Sierra's boot disk or the persistent guest's ext4 volume.
- Exactly **8 × 512-KiB slots**. Each record commits a bounded JPEG payload **before** a 4-KiB header, both followed by `fsync`; header contains sequence, host-monotonic PTS, original JPEG geometry, exact byte length, stream ID and full SHA-256. Each Linux read rechecks header and JPEG digest, rejecting torn, stale or reused slots. Max JPEG **262,144 bytes**.
- Host ingester bound **only** to `127.0.0.1:8483`, authenticating with the existing **private 0600 Sierra-local token** (never copied to a guest, Ubuntu, or Git). Only `decklink-observed` JPEG is accepted. `GET /health` reports head sequence but does not assert independent HDMI verification.
- Linux uses an **O_DIRECT**, 4-KiB-aligned, read-only virtual block device. No guest networking. Original receiver-side Docker secret remains **inside the Alpine guest** and is bind-mounted read-only to the sandboxed reader; token is not passed to Sierra.
- Source-owned guest boots from the exact same old Alpine kernel/ISO and persistent `docker-guest.raw`; the new read-only slot is `-s 5,virtio-blk,fountain-hdmi-mailbox.raw,ro,sync`. On guest Linux, `blockdev --getro /dev/vdb` initially returned **0 despite xhyve's host-file ro option**, so the guest explicitly executed `blockdev --setro /dev/vdb`, after which **getro=1**. **This enforcement is REQUIRED on each guest reboot**; do not infer guest RO from the host option alone.
- A source-only `tar.gz` archive was staged into reserved sectors 65 onward of the new mailbox, outside allowed image payloads; SHA-256 `2c0ddc4862c5ae7f4b85b719853a99bb024a6d7d3a5c7ce30d66bf773ea799a9`, **4,458 bytes**. Alpine independently read it through `/dev/vdb` and verified the exact SHA before extracting only the two public reader/protocol scripts into its **existing** persistent ext4 guest disk.
- The previous Alpine guest was **gracefully shut down**, both Docker containers stopped, Docker daemon terminated and guest ext4 unmounted successfully before reboot. No Sierra restart.
- Existing `fc-scratch-guest` Python 3 Docker Scratch server was started. A **separate reader container** uses the same network namespace, only `/dev/vdb` mapped with `:r`, unprivileged user 1000, supplementary group 6, read-only container filesystem, all capabilities dropped, no-new-privileges, memory/pid limits, and two read-only file mounts for source scripts. Docker authentication occurs **only inside Linux** on localhost port 8273.
- Pure protocol tests: **five passing Python 3 tests** on Ubuntu (private file, integrity, boundedness, ring wrap, restart sequence, corruption rejection). Linux direct-I/O reading from a test mailbox also passed with matching SHA. Sierra Python 2.7 successfully loaded the same protocol and opened the new private mailbox.

## PHYSICAL LIVE ACCEPTANCE — actual Sierra and its own Docker guest

The original installed GUI was gracefully quit to avoid concurrent Blackmagic capture clients. The separate **candidate** executable `Fountain Video Mailbox Candidate.app` (not silently substituted for the user-installed stable app) passed its converter, JPEG and GUI regression tests. Its explicit bounded source-acquisition command was executed with only a **PATH** to the existing private Sierra local bearer token:

    --self-test-fixed-silver-hdmi-live-local-docker

This commanded only the approved fixed silver Mac HDMI loopback, loaded the existing vendor 10.4 API, gated StartStreams on two consecutive genuine 1080p30 HDMI lock readings, used source-owned borrowed-frame copying and **Sierra monotonic** PTS, JPEG-encoded physical images and submitted a bounded ~5 fps stream to the private localhost mailbox ingress. **No public/LAN streaming** of the Mac's primary desktop.

Actual native test output:

    stage=native_docker_live result=pass
        physical_rgba=1
        local_mailbox_committed=17
        post_failed=0
        main_desktop_share_after_test=OFF
        destination=silver_virtio_block
        clean_capture_release=1
        docker_receipt_independently_required=1

Original native logs repeatedly report `Real video frame received` throughout this capture. The localhost mailbox independently returned `last_host_commit: 17`. This alone would only prove the host-side writes — therefore we **independently inspected Linux Docker**:

    stage=guest_hdmi_mailbox status=ready
        last_existing_seq=0 device=readonly_direct
        authenticated_receiver=localhost

    stage=guest_hdmi_mailbox accepted=1 host_seq=1 scratch_seq=0
        jpeg_bytes=85137
        sha256=7a56202f4cbcbf78d42be8bee89d545c62710078bee7b2b554f469e953ea90b9

    stage=guest_hdmi_mailbox accepted=15 host_seq=15 scratch_seq=14
        jpeg_bytes=72499
        sha256=fdb91d6a177d162a3c62fa2c1a01c7071b7d6316e6c27d6d4d46190522003b61

The **actual silver-local Docker Scratch receiver** queried from `docker exec fc-scratch-guest`, through its own localhost:

    accepted_count=17 oldest=0 latest=16
    origin=decklink-observed simulated=False

In Sierra's actual 4 MiB mailbox, every slot's current complete-frame SHA-256 validation passed. Example host seq **15** has **the exact SHA-256** `fdb91d6a177d162a3c62fa2c1a01c7071b7d6316e6c27d6d4d46190522003b61`, matching the independent Docker-side logger for seq 15. The most recent ring slot at host seq 17 also verified digest `811abff319a90f30a33f2c7cf4e3f9ba31257b02999f59dca61495f2fce92d9c`. Thus the guest bridge validated every accepted image itself; there is **no simulated HTTP or synthetic frame** in this acceptance.

**Result:** bounded *live* physical HDMI frame transport from Sierra through xhyve's file-backed virtual block into Docker on the **same silver Mac**, 17 genuine frames, no failed uploads. This is materially faster than the retired serial path (~6s / 718 bytes); exact sustained maximum frame rate has **not** been benchmarked beyond this short test.

## Privacy and restoration

- The explicitly opt-in bounded live test stopped by itself; native result confirms **sharing OFF after test** and clean vendor input release.
- The established installed `~/Applications/Fountain Video.app` was **reopened** successfully after the isolated test (observed as PID 9229). Sierra driver remained original 10.4 and BMD HDMI 1920×1080@30. The separately compiled new mailbox candidate remains staged, not installed.
- The localhost mailbox service remains user-owned on `127.0.0.1:8483` requiring the existing Sierra private credential. No network listener is opened for the Alpine guest; the Scratch viewer is **inside Linux's network namespace** and not yet exposed to a Mac browser.
- The sidecar reader was started as a **bounded** 300-second test, not configured to restart forever. Following acceptance the Alpine guest's interactive serial console became unresponsive during attempts to fetch multiple saved JPEGs via `docker exec`; this unresponsiveness is **unresolved**, despite successful earlier Docker state query and independent reader logs. Do **not** claim always-on production readiness. Do not automatically activate the Mac's desktop publication or remove the read-only boundary.

## Next acceptance for unattended operation

1. Diagnose/post-fix guest console responsiveness after the live stream (possibly too aggressive 80 ms direct block polling; do not assume root cause).
2. Reduce idle polling or implement reader backoff, test Docker frame readback, verify clean stop/shutdown and repeated startup.
3. Bind the native GUI checkbox to this **Sierra-local mailbox** (rather than an unrelated LAN endpoint), preserving **OFF by default** because the HDMI source is the silver Mac's own screen.
4. When stable, explicitly authorize guest-side reader lifecycle and a controlled, private viewer path; never require outside HDMI inputs or an Ubuntu host replacement.

**PR #10 remains DRAFT pending these longevity/privacy gates.**
