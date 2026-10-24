# 24 October 2026 — root cause of failed xhyve COM2 media

**Status: exclusive COM2 byte transport verified; JPEG into Linux Docker NOT yet verified. Physical HDMI capture NOT accepted.**

## Verified competing reader, not just emulated UART corruption

In Alpine 3.24.3 booted on the silver Sierra Macmini5,3, /etc/inittab contained an active line:

    ttyS1::respawn:/sbin/getty -L 0 ttyS1 vt100

At the same time a Linux Docker container named fc-serial-bridge ran Python serial_frame_guest, reading the **same guest /dev/ttyS1**. Sierra sees the other endpoint as xhyve -l com2,autopty (ephemeral /dev/ttys001). Direct inspection of the Linux process table showed:

    /bin/login -- FCSV1 1 123456790 4 2 718 ... silver-paced-fixture

The **FCSV1 media header had been taken as a login name**. A Sierra-only Python 2.7 HTTP probe received `Login incorrect` and `localhost login:` from the UART in place of a valid H frame acknowledgement, returning HTTP 503. This is definitive evidence of the wrong process reading the media port.

## Exclusive-reader retest

In the disposable guest /etc/inittab only, the ttyS1 getty respawn line was commented, and PID 1 received SIGHUP to reread the configuration. Guest process inventory then showed **no ttyS1 getty**. The serial-bridge container process was stopped for the raw byte test. With the Sierra PTY in raw mode:

- Exact 16 bytes `SILVER-EXCLUSIVE` crossed from host to guest and appeared as matching hexadecimal values.
- After the Linux guest explicitly reported `exclusive_digest_ready`, a 512-byte deterministic host binary payload was sent in one-byte writes paced at 4 milliseconds. Sierra SHA-256 and independent Linux SHA-256 were **identical**:

    08ac48e649b513d133de8324a7c75f166f3347490afcbe447e8df6debf09208b

This supersedes the prior assumption that the emulated UART **intrinsically** loses these bytes. Earlier mismatches occurred in the presence of competing getty/login processes and uncontrolled writes.

Software-only five PTY tests continue to pass. These are not physical hardware capture evidence.

## Container lifecycle fault after the earlier collision

The old Docker serial bridge and its containerd shim became unresponsive following the conflicting-reader tests. Its Linux process was terminated but Docker commands remained blocked. The guest was powered off by terminating xhyve after normal shutdown did not complete. **This may have left the experimental ext4 guest disk dirty.** The silver's original macOS Sierra, Blackmagic Desktop Video 10.4 KEXT, Data/Fresh partitions and original Fountain Video sender application were not changed. No Blackmagic input or vendor driver test was run during this serial investigation.

The isolated 12 GiB guest disk is a user-owned file on Fresh. It should be validated offline (e2fsck) before another writable guest boot. A read-only compressed backup copy is being staged outside Git on the separate Ubuntu host.

## Correct next iteration

1. Treat /dev/ttyS1 as a **dedicated media endpoint**, never a serial login. Run `scripts/alpine-guest-exclusive-com2.sh` inside the disposable Alpine guest **before** starting Docker or giving /dev/ttyS1 to a container. Its effect is confined to the guest.
2. Ensure one and only one process consumes guest /dev/ttyS1; no getty, no competing cat/head, no orphaned container instance.
3. Keep host single-flight framing and checksum/acknowledgement. The current host transport uses a paced write variant; 512 bytes matched, but no authenticated whole JPEG transaction in the live guest yet.
4. Do not enable physical DeckLink origin labels in this experimental serial proxy. Genuine HDMI signal lock, input pixels, source timestamp and crash-free native driver remain unverified.
5. Preserve the already-working Sierra → separate Ubuntu LAN scratch service as the fallback. Do not install new KEXTs, reflash device firmware, replace Sierra or change boot volumes without a full boot-tested recovery.

For reference, the existing signed Blackmagic 10.11.2 Sierra/A124 vendor installer and the exact 10.4 rollback package were already audited, but a separate boot-tested full-system rollback has not been established. This remains a **NO-GO** for a disruptive driver upgrade.
