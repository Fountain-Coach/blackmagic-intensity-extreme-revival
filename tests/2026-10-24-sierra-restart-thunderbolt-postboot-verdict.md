# 24 October 2026 — controlled Sierra reboot after Blackmagic Thunderbolt disappearance

## Authorization and fixed fixture

The operator explicitly proposed restarting the original silver Sierra Mac after the Blackmagic Intensity Extreme stopped enumerating even though all cables were connected. This is the **same** silver Macmini5,3, macOS Sierra 10.12.6, HDMI OUT -> original Intensity Extreme HDMI IN, Intensity HDMI OUT -> monitor; card behind original Comdis THUNDERDOCK2. All HDMI and Thunderbolt wiring remained in place. No alternative HDMI source, OS/driver/firmware reinstall, PCI passthrough, or Linux conversion.

## Before reboot

- Original i-tec/Comdis THUNDERDOCK2 enumerated, including dock Ethernet and USB PCI controllers.
- Original Intensity PCI function `BDBD:A124` absent. Dock's Thunderbolt downstream port reported **No device connected** despite operator-confirmed cabling. The native Desktop Video driver 10.4 had remained installed.
- Alpine 3.24.3 + Docker 29.9.2 Linux guest was running in Sierra's source-owned xhyve, with 12 GiB persistent ext4 image `docker-guest.raw`. Earlier independently accepted 17 real HDMI mailbox frames remained in the 4 MiB private mailbox. The Alpine serial console was unresponsive on further Docker requests; typing `poweroff` did not result in a confirmed clean shutdown.
- The silver host's `/Volumes/Fresh` is an **internal SATA** volume with ample free space; network access is via Wi-Fi, not the dock. FileVault was **Off**.
- Private, mode 0700 backup directory under Fresh: `private-restart-20261024`. User-owned original guest image and mailbox were mode 0600, and left unmodified.
- Byte-identical private copy of the 4 MiB HDMI mailbox was verified by matching SHA-256: `96051895b40b1a3351c0bc1bd6092c770b7e39f841b92c9db16e2e871297e829`.
- The guest was paused with `SIGSTOP` while a lossless `gzip -1` archive of the complete 12,884,901,888-byte guest image was written. Output archive is **797,087,705 bytes**, mode 0600. The gzip decompression/integrity test **passed**. SHA-256 of the **compressed archive**: `41c5212e3d6123a063c04f3c67bf5c6024bc11748f3b6fe928d5cbffdb391b4d`.
- **Limitation:** a complete byte-by-byte source-to-decompressed-archive SHA-256 equivalence was **not completed**. The compression integrity CRC and compressed-file SHA were verified. The guest's in-memory filesystem state could not be asserted clean because Alpine did not acknowledge `poweroff`.
- After archive construction and verification, xhyve was resumed, asked to exit with `SIGTERM` (did not exit), then exited after `SIGINT`. The original xhyve PID disappeared and no live xhyve process held the disk. No `SIGKILL` was needed. We explicitly **do not claim a guest-clean poweroff**. A copy-back restore was not attempted.

## Actual Sierra reboot

A normal logged-in Sierra user AppleEvent was issued remotely using `osascript` and `System Events restart`; the command exited **0**, Sierra's SSH connection was closed, and the original host then came back on the same WLAN under SSH.

**Independently confirmed postboot kernel uptime:** new `kern.boottime` **Saturday 24 October 2026 at 18:04:31 CEST**. Original boot time was Thursday 22 October 05:58:59 CEST. Sierra version remains **10.12.6**.

## Postboot hardware inventory (18:05–18:06 CEST)

- Comdis **THUNDERDOCK2** once again enumerated: original UID `0x016B900600014FB0`, route 3, upstream link connected, 10 Gb/s.
- Dock's Thunderbolt downstream Port: still `Status: No device connected`, `Link Status: 0x7`.
- Dock PCI Ethernet `8086:1533` and USB xHCI `1b73:1100` enumerated again.
- Blackmagic Intensity Extreme **PCI `BDBD:A124` is STILL ABSENT** in independent `system_profiler SPPCIDataType` observations at 18:05:46 and 18:06:28.
- `SPDisplaysDataType` shows the Intel HD Graphics 3000 chipset but **no live HDMI display** (no `BMD HDMI` sink); monitor optical signal remains unverified after reboot.
- Kernel reported original Blackmagic **framebufferdriver 10.4** loaded. The full Blackmagic DeckLink main driver was not shown in that brief postboot KEXT grep; loading a driver, either way, is not evidence that a missing PCI endpoint is present.
- Linux xhyve guest is **stopped** after restart and has **not been automatically restarted**. Original guest disk and private recovery archive remain on internal Fresh.
- No fresh recorded input frames or HDMI output display signal were accepted after restart.

## Verdict

**Sierra restart accepted and independently witnessed. Blackmagic device not restored.** This is a postboot physical/Thunderbolt downstream enumeration blocker upstream of HDMI signal lock, frame format, receiver video code, Docker and user-space vendor API. A restart of the OS alone does not resolve it. The present evidence does not establish which of device power, dock downstream port negotiation, downstream cable electronics, or the original Intensity hardware is at fault, even though cabling is operator-confirmed correctly connected.

Keep PR #10 **DRAFT** and keep hardware acceptance fail-closed: `NO_BLACKMAGIC_PCI_DEVICE` (exit 34). Do not run input-lock/capture/replay while `BDBD:A124` is absent. The next meaningful recovery is an **independently witnessed downstream Thunderbolt device power/link reset** (for example a controlled dock power cycle with due regard for its other connected devices); it cannot be effected through Sierra's DeckLink API while the endpoint is absent. Do not ask the operator to keep moving HDMI cables or reinstall software.
