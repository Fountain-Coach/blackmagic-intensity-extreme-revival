# Silver Sierra / Docker capture gateway

**Decision, 2026-10-24:** The silver 2011 Macmini5,3 **stays on Sierra 10.12.6**. No Ubuntu reinstallation, startup-volume changes, disk repartitioning, Blackmagic driver replacement or firmware flashing. Its original Blackmagic Intensity Extreme (PCI 0xBDBD:0xA124) remains physically attached over Thunderbolt through the Comdis THUNDERDOCK2.

## Actual execution boundary

Physical unprotected HDMI -> Intensity Extreme -> Sierra-native hardware adapter -> timestamped frame transport -> Linux guest with Docker Engine -> source-neutral receiver and browser scratching / recorder.

**The distinction is essential:** Linux/Docker owns video ingest, buffering, processing and consumers. The Sierra kernel owns the physical PCI/Thunderbolt device. VirtualBox's PCI passthrough works on Linux hosts, not on the Sierra/macOS host, and Docker cannot conjure a Linux hardware-driver connection to the original card. If direct Linux PCI/DMA/interrupt ownership is required, it is a separate hardware/host architecture, NOT provided by this setup. A virtual Linux video device fed by streamed frames would be a useful Linux-facing interface, not a native A124 driver.

## Existing evidence

The Sierra Fountain Video app already sends real *synthetic* JPEG bytes into the existing Ubuntu 192.168.8.50:8273 receiver; authenticated ingress and browser reverse/forward scratch passed. The Blackmagic's real HDMI capture is NOT accepted: prior attempts reported no locked input, no valid frames and an old vendor frame-lifetime crash. The native crash guard remains enabled. No synthetic frame proves that the hardware has been revived.

## Container stage: no takeover of working service

The container packages the existing Python 3.12 standard-library scratch receiver, not proprietary Blackmagic libraries. Nonroot user, read-only filesystem, all Linux capabilities dropped, bounded memory, token from private Docker secret file. Default port 8373 keeps the existing receiver on 8273 untouched. The viewer is unauthenticated, so default binding is 127.0.0.1 only.

On a Linux Docker host, with an existing private token file (32+ characters):

~~~sh
export FOUNTAIN_SCRATCH_TOKEN_FILE=/absolute/private/path/scratch-ingest-token
docker compose -f compose.scratch.yaml build scratch
docker compose -f compose.scratch.yaml up -d scratch
docker compose -f compose.scratch.yaml ps
# http://127.0.0.1:8373/scratch
~~~

To accept the Sierra sender across a private LAN, explicitly set FOUNTAIN_SCRATCH_BIND_IP to the Linux VM's real trusted-LAN address (prefer a bridged VM interface), and set its existing POST target to http://<guest-private-IP>:8373/api/scratch/ingest. The same token must be configured on sender and receiver. No exposed public port or untrusted-Wi-Fi transport: this prototype uses plaintext HTTP and does not authenticate viewers. Add TLS and viewer authorization before sensitive or external use.

A VM disk may eventually be stored as an ordinary file on silver's Fresh drive (after inspecting existing files), but this document DOES NOT authorize any VM installation or storage modification. Sierra remains the boot operating system.

## Acceptance gates

1. Docker image builds; secretless service fails closed; valid token allows synthetic JPEG ingest and frame retrieval with timestamps and provenance; Linux browser scratching operates.
2. Existing Ubuntu receiver and Sierra native sender keep working unchanged while container is tested on separate local port.
3. A known unprotected physical HDMI source is independently witnessed, the device locks, and a **genuine changing captured frame** crosses from the hardware into Docker with exact provenance and timing. No-source callbacks, EDID detection, a simulator and a successful HTTP POST do not count.
4. Repeat capture across formats, signal loss and recovery, safe device release, storage and optional audio; no frame lifetime crash.
5. Independent low-level A124 PCI/DMA driver research remains a separate milestone requiring exclusive direct hardware ownership.

**Current status:** Docker media-service packaging; not a fully revived capture device.
