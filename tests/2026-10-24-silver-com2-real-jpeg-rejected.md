# 24 October 2026: Sierra / xhyve COM2 serial media - live rejection

**NOT ACCEPTED for deployment.** Mock PTY transport passes, but the real silver Sierra -> xhyve -> Docker JPEG test failed. No captured Blackmagic frame or claimed physical acceptance.

## Preserved hardware and architecture

The physical silver Macmini5,3 still boots Sierra 10.12.6. Its original Intensity Extreme PCI BDBD:A124 remains on the Comdis THUNDERDOCK2 via native Thunderbolt 1; Desktop Video 10.4 stays loaded and the native crash guard stays active. Linux/Docker is a guest, not the hardware PCI owner. No root vmnet workaround, host KEXT, firmware, disk repartition or operating-system replacement.

## Directly witnessed tests

- Original xhyve COM2 autopty maps the silver Mac pseudo-terminal /dev/ttys001 to guest Linux /dev/ttyS1. An **18-byte** host-to-guest message crossed exactly, and a **17-byte** guest reply crossed exactly. This proves interactive control-channel liveness only.
- A 16384-byte deterministic host binary payload SHA-256 ce61cc72a84bd3526a8f955235c5f4f67439820d6ffb910b5ab07e54741262b8 did NOT match the received guest hash (one witnessed mismatch was 7b04e17f3307184be1faa3dfe5f2e1d764ad147a65ef32073d792b3d6e614ebd). Initial host PTY had line processing; further raw-mode trials still did not preserve all bytes.
- A 64-byte paced ASCII pattern in raw mode started at guest byte D rather than A and skipped at least Y and w; sending extra filler was required to complete an exact-length read. Therefore COM2 frame-byte integrity **is not established**.
- On separate Ubuntu the new Python 3 software-only PTY suite passed **5/5 tests**: exact JPEG receipt, CRC negative acknowledgement/recovery, whole-frame SHA tamper rejection, bounds and metadata rejection. Mock success is not physical-host acceptance.
- Real Fountain Video Docker scratch receiver was restarted in the silver Alpine guest. Restricted guest COM2 bridge container listened, sharing only the receiver container's network namespace, owning only guest serial device access and a private receiver token; guest network remains offline.
- On Sierra, Python 2.7 loopback-only serial HTTP proxy listening on 127.0.0.1:8383 returned /health 200 using its private 0600 token. An authenticated **718-byte synthetic JPEG** POST to its ingest route returned **HTTP 503**. The Docker receiver count remained **zero**. No valid serial frame commit or captured frame was observed.
- Sierra test proxy was stopped; the guest serial container was stopped; the Linux guest was shut down. Original Sierra app publisher configuration was **not** changed. Existing separate Ubuntu receiver and independent Docker staging remained intact.

## Retained implementation, still draft

PR #10 contains simulator/serial_wire.py (bounded CRC and SHA wire protocol), simulator/serial_frame_guest.py (guest offline ingest), tools/sierra/fountain_serial_proxy.py (Sierra Python 2.7 local authenticated proxy, synthetic-only), and simulator/tests/test_serial_frame.py (five software-only tests). These are **experimental and inactive**. Do not deploy or claim video transport success.

## Next responsible gate

Use authorized VM networking with legitimate permissions, or keep existing independent Ubuntu Docker receiver reachable over trusted LAN. Do not bypass Sierra vmnet controls. Physical HDMI acceptance is separate: retain crash guard and inspect an authentic Sierra-era Desktop Video 10.x update offline, with verified model match and rollback image before changing any vendor driver.

The source-owned noninvasive preflight was run again on the physical silver Mac in this turn: **exit 0**, BMD HDMI EDID valid, current output 1920x1080@30, changing macOS framebuffer, 12 passing ABI mock tests, Blackmagic driver 10.4 still loaded, original crash evidence retained, and physical capture explicitly **NOT_ACCEPTED**. Zero vendor capture API calls, PCI register accesses or driver modifications.
