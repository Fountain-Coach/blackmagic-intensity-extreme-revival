# Silver Sierra / Docker capture gateway

**Decision, 2026-10-24:** The silver 2011 Macmini5,3 **stays on Sierra 10.12.6**. No Ubuntu reinstallation, startup-volume changes, disk repartitioning, Blackmagic driver replacement or firmware flashing. Its original Blackmagic Intensity Extreme (PCI 0xBDBD:0xA124) remains physically attached over Thunderbolt through the Comdis THUNDERDOCK2.

## Actual execution boundary

**Fixed physical rig: silver Mac HDMI OUT → original Intensity Extreme HDMI IN → same silver Mac's Thunderbolt 1/Comdis dock → native Sierra capture adapter.** The user has declared this arrangement FINAL; alternate cameras, HDMI generators and second source computers are NOT substitutes.

The source-owned adapter now successfully captures real 1920×1080 UYVY frames from that rig, timestamps with **Sierra host monotonic time**, and can privately JPEG-encode them. The **separate Ubuntu Linux Docker** scratch receiver has accepted two authenticated real-source JPEGs with independently matched SHA-256. The **silver Mac's own offline Alpine Linux Docker guest** is a separate validated compute environment and has so far ingested **synthetic**, not actual captured, frames through slow virtual UART. Sustained full-rate local-guest physical transfer is NOT accepted.

**The distinction is essential:** Linux/Docker owns video ingest, buffering, processing and consumers. The Sierra kernel owns the physical PCI/Thunderbolt device. VirtualBox's PCI passthrough works on Linux hosts, not on the Sierra/macOS host, and Docker cannot conjure a Linux hardware-driver connection to the original card. If direct Linux PCI/DMA/interrupt ownership is required, it is a separate hardware/host architecture, NOT provided by this setup. A virtual Linux video device fed by streamed frames would be a useful Linux-facing interface, not a native A124 driver.

## Existing evidence

The original 8273 Ubuntu service retains its synthetic private-LAN history and browser controls. **Physical HDMI acquisition is now ACCEPTED** using the final silver-to-Intensity loopback, the original Sierra 10.4 driver and the repaired native callback. The old vendor `GetStreamTime` caused an accessor-associated frame destruction crash; it is excluded from the repaired capture path. The native AppKit GUI physically passed, was installed and launched with real preview selected only for the exact BMD 1080p30 display and network publication OFF by default. The **separate Ubuntu Docker staging receiver at 127.0.0.1:8373** accepted two actual pixel-derived JPEGs with authentication, confirmed content hashes and source `decklink-observed`; this was a controlled private transfer, not continuous live streaming. [Complete witnessed physical result](../tests/2026-10-24-fixed-silver-hdmi-physical-capture-and-native-app-accepted.md).

## Container stage: no takeover of working service

The container packages the existing Python 3.12 standard-library scratch receiver, not proprietary Blackmagic libraries. Read-only filesystem, no-new-privileges, all Linux capabilities dropped, bounded memory, token from a private Docker secret file. Local Docker Compose bind-mounts the secret with its existing host UID 1000 and mode 0600; the process therefore uses the matching unprivileged UID 1000. This UID must be adjusted to the real secret-file owner on another host without weakening the token file. Default port 8373 keeps the existing receiver on 8273 untouched. The viewer is unauthenticated, so default binding is 127.0.0.1 only.

On a Linux Docker host, with an existing private token file (32+ characters):

~~~sh
export FOUNTAIN_SCRATCH_TOKEN_FILE=/absolute/private/path/scratch-ingest-token
docker compose -f compose.scratch.yaml build scratch
docker compose -f compose.scratch.yaml up -d scratch
docker compose -f compose.scratch.yaml ps
# http://127.0.0.1:8373/scratch
~~~

To accept the Sierra sender across a private LAN, explicitly set FOUNTAIN_SCRATCH_BIND_IP to the Linux VM's real trusted-LAN address (prefer a bridged VM interface), and set its existing POST target to http://<guest-private-IP>:8373/api/scratch/ingest. The same token must be configured on sender and receiver. No exposed public port or untrusted-Wi-Fi transport: this prototype uses plaintext HTTP and does not authenticate viewers. Add TLS and viewer authorization before sensitive or external use.

**Historical guest-runtime witness (earlier 24 October; physical adapter now working):** A source-built xhyve virtualizer booted Alpine Linux 3.24.3 and Linux Docker Engine 29.9.2 on the silver Mac. On Fresh, new `/Volumes/Fresh/FountainVideoDockerLab/docker-guest.raw` is an isolated 12 GiB guest-only ext4 virtual disk; Sierra still boots from Data. The Docker persistent store survived a Linux guest reboot. A `hello-world` container succeeded, and the actual Fountain Video browser-scratch image started and passed 18 receiver tests. Sierra vmnet rejected an unsigned guest; an ad-hoc signed executable was rejected by runtime code-signature enforcement. No root service or network entitlement workaround was performed in that virtual guest runtime test. It stays **offline** until a legitimate high-throughput network arrangement is established; separate physical capture has subsequently been demonstrated on the Sierra host. [Full witness](../tests/2026-10-24-silver-sierra-docker-runtime.md) · [read-only-by-default launcher](../scripts/silver-video-docker-guest.sh).

## Acceptance gates

1. Docker image builds; secretless service fails closed; valid token allows synthetic JPEG ingest and frame retrieval with timestamps and provenance; Linux browser scratching operates.
2. Existing Ubuntu receiver and Sierra native sender keep working unchanged while container is tested on separate local port.
3. **Physical acquisition ACCEPTED** for the user's **only fixed silver HDMI loopback**: independently confirmed signal lock, two different full pixel frames, clean frame-memory release, host-monotonic timestamps. Two derived JPEGs traversed a private authorized file transfer into actual Ubuntu Docker staging, retaining exact SHA-256. No simulated or fake HTTP frame counts. **Sustained real-time capture → automatic Ubuntu Docker streaming remains OPEN.**
4. **Clean bounded physical device release ACCEPTED** on exact 1080p30 after removing vendor GetStreamTime. Sustained streaming, signal-loss/recovery, private movie capture and audio remain OPEN. This project does not require changing the final physical test source.
5. Independent low-level A124 PCI/DMA driver research remains a separate milestone requiring exclusive direct hardware ownership.

**Current status:** Original Blackmagic Intensity Extreme **revived for fixed-rig HDMI input, true pixel capture, private JPEG preview, and native app preview**, with no driver replacement. Separate Ubuntu Docker accepted two real frames. A continuous, authenticated, high-bandwidth **physical source → Docker live** transport remains to be accepted; the local silver guest UART is too slow for live video.
