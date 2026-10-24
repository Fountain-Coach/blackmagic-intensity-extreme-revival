# 2026-10-24 — first genuine Sierra → local Linux Docker JPEG

**ACCEPTED: source-neutral synthetic JPEG byte integrity across Sierra/xhyve/Docker. NOT ACCEPTED: actual Blackmagic HDMI capture or live video rate.**

## Physical computer and operating systems

- Silver Macmini5,3 still boots macOS Sierra 10.12.6 from Data. The original Intensity Extreme PCI BDBD:A124 remains attached to Sierra via Thunderbolt 1 / Comdis THUNDERDOCK2, with Desktop Video 10.4 unchanged.
- Source-built xhyve runs Alpine Linux 3.24.3 in a guest, with Docker Engine 29.9.2 and the existing Fountain Video browser-scratch receiver. Guest block device /dev/vda is the dedicated 12 GiB user-owned virtual disk file on Fresh. No PCI passthrough, no Ubuntu host replacement or root vmnet.
- Before Docker started, the Alpine login service competing for ttyS1 was removed from the ephemeral guest's /etc/inittab and PID 1 reloaded. Process inspection showed **no ttyS1 getty**.
- The guest disk had previously been backed up via read-only compression on the separate Ubuntu host. SHA validated gzip copy ~710,739,450 bytes, expanded into a sparse 12 GiB image. Ubuntu e2fsck 1.47.0 -fn returned **exit 0**, no filesystem errors. The original guest file was not replaced.
- After this clean boot, Docker Engine and fc-scratch-guest started successfully. The fc-serial-bridge sidecar started only after removing ttyS1 login and claimed just the serial device; it shares the receiver container's network namespace, with no guest virtual NIC.

## Real byte end-to-end acceptance

Host Sierra Python 2.7 loopback proxy:
  endpoint http://127.0.0.1:8383/api/scratch/ingest
  private 0600 token file outside Git, sender provenance forced SYNTHETIC

COM2 path:
  xhyve -l com2,autopty
  Sierra ephemeral /dev/ttys001 → guest /dev/ttyS1
  bounded, paced ASCII/base64/CRC + whole-frame SHA transport with receiver acknowledgements

A source-owned 718-byte synthetic JPEG file was submitted on Sierra by tools/sierra/serial_fixture_probe.py. Actual HTTP response:

    stage=serial_silver_fixture status=201 elapsed_s=6.216 bytes=718
    response={"origin":"synthetic","accepted":true,"physical_hdmi_verified":false,"seq":0}

Independent **Linux Docker receiver** query:

    stage=guest_scratch count=1 frame=0 bytes=718
    sha256=4982c239c16e03886c60eff9fb74d52371dcd9dee6a7dc3285c277841c4f6a70
    hardware=False

Independent **Sierra source file** SHA-256:

    4982c239c16e03886c60eff9fb74d52371dcd9dee6a7dc3285c277841c4f6a70

Thus an actual image byte stream from the physical silver computer was authenticated and stored **byte-for-byte identically** inside its local Linux Docker media receiver, with a retrievable frame sequence. This is not a simulated PTY test or a fake HTTP success.

## Explicit limits

- 718 bytes required 6.216 seconds. This transport is **proof of integration, not a viable uncompressed/real-time video transport**. The separate Ubuntu receiver already works at a far higher network rate; a proper high-bandwidth/authorized guest network remains recommended. Do NOT reroute the existing live Sierra application into this low-throughput proxy as its default.
- The original physical Intensity Extreme HDMI input has **not** independently locked or returned accepted changing pixels. The crash-prone Desktop Video 10.4 callback remains guarded. No Blackmagic input session, driver update, firmware update or dangerous callback was run in this acceptance.
- The guest uses Alpine install-media/offline serial console, not a hardened production Linux distribution. No guest LAN connection; source and receiving APIs are localhost-only and private-token guarded.
- This exact boot is manually configured; the source-owned guard is scripts/alpine-guest-exclusive-com2.sh. An unattended production boot path, encrypted/authorized viewer, guest network and sustained frame-rate acceptance remain outstanding.

**User-approved invariant:** Sierra remains the silver Mac's host OS, Linux Docker is its media-service guest, and the only physical Blackmagic Thunderbolt endpoint remains on Sierra.
