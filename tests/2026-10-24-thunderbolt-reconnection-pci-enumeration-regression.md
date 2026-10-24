# 24 October 2026 — Blackmagic PCI disappearance after Thunderbolt reconnection

## Fixture and user observation

The physical fixture is unchanged by the software investigation: original silver Macmini5,3 HDMI OUT -> original Blackmagic Intensity Extreme HDMI IN; original Blackmagic HDMI OUT -> reference monitor; original Intensity connected via the Comdis THUNDERDOCK2 Thunderbolt path to the **same** silver Mac running Sierra 10.12.6 and original Blackmagic Desktop Video 10.4.

The operator unplugged and reconnected the existing Thunderbolt cable. The previously observed monitor on/off cycling stopped; the monitor then behaved **as if disconnected**. This is an operator-observed optical/sink-state report, not evidence that the reference monitor output became stable or that HDMI capture was restored.

No HDMI source substitution, cable rearrangement by the agent, driver or firmware replacement, Linux host switch, sudo, reboot or PCI register writes were performed.

## Live system readback after reconnection (17:37–17:38 CEST)

SSH into the actual silver host and `system_profiler SPThunderboltDataType SPPCIDataType SPDisplaysDataType` reported:

- Comdis **THUNDERDOCK2** still enumerated on Thunderbolt (UID `0x016B900600014FB0`, route 3, link `0x2`, up to 10 Gb/s x2).
- The dock's **Intel Ethernet** PCI function `8086:1533` and **USB xHCI** PCI function `1b73:1100` enumerated.
- Original Blackmagic device PCI `BDBD:A124` was **absent** from the host's current PCI list.
- Original `BMD HDMI` source-side sink no longer appeared in Graphics/Displays. Sierra instead reported a generic `Display` at 1920x1080 @ 60 Hz; **this is not proof of actual HDMI electrical output or any substitute display path**.
- Original Desktop Video 10.4 kernel modules were **still loaded**, but that cannot establish that their corresponding PCI hardware is present.
- Vendor DeckLink read-only enumeration also failed: the newly authored input-mux capability probe returned exit 22 (no usable first DeckLink device), and the existing output-idle configuration inventory returned exit 67 (same missing device class). These are secondary indications; system PCI enumeration is the authoritative evidence.

The macOS kernel unified log includes the Thunderbolt removal/hot-plug message at **17:34:14.399112 CEST**:

```text
(IOThunderboltFamily) IOThunderboltSwitch<0x0>(0x3)::listenerCallback -
Thunderbolt HPD packet for route = 0x3 port = 1 unplug = 1
```

This is evidence of a Thunderbolt disconnect event; it does **not** establish which physical card component caused the earlier signal-lock failures. The dock has subsequently returned, but the Blackmagic PCI function has not.

## A/B/A input-lock comparison immediately before the PCI disappearance was confirmed

A source-owned bounded differential compared original volatile HDMI output idle standards while keeping the silver Mac 1080p30 HDMI source and actual cabling fixed. After cleanly quitting the native app for exclusive device access:

| Phase | Original Blackmagic default output mode | Original HDMI IN signal lock |
| --- | --- | --- |
| A1 | `Hi50` 1080i50 | 0/12 polls locked |
| B | `Hp30` 1080p30, volatile change only | 0/12 polls locked |
| A2 | original `Hi50` restored | 0/12 polls locked |

All three input enable/disable calls returned S_OK. `voio=blac` remained constant. Volatile B-phase change was independently restored before AppKit re-launch. CLI returned **42** `NO_LOCK_IN_B`. The native Fountain Video application was reopened, and while running restored its known in-memory `Hp30/black` configuration. No full capture stream, recorder, output session or private frames were touched.

The card was still enumerable when the A/B/A test completed. Shortly thereafter independent DeckLink user-space queries and PCI discovery found it missing. This sequence does not prove whether the HDMI receiver loss and eventual PCI disappearance share one physical cause.

## Fail-closed hardware-presence gate — executed

The source-owned gate `scripts/sierra-hdmi-physical-roundtrip-gate.sh` was updated to reject absence of the original `BDBD:A124` PCI function **before** HDMI-sink, input-lock or recording gates. The updated script was staged separately at `/tmp/fc-roundtrip-pci-guard.sh` on silver, syntax-checked and executed with `--check` against the real host.

```text
stage=roundtrip_gate result=NO_BLACKMAGIC_PCI_DEVICE vendor=0xbdbd device=0xa124
stage=roundtrip_gate capture=NOT_STARTED playback=NOT_STARTED
exit=34
```

This was a live hardware fail-closed acceptance result, **not** a simulator or recovery. The canonical source is committed on draft PR #10; the guard was tested from a temporary silver-local copy, not silently installed over an existing on-host script.

## Next boundary and recovery policy

1. **Stop trying to renegotiate video modes or run capture/playback while the A124 PCI function is absent.** Loaded kernel modules and a still-enumerated dock are not hardware-presence proof.
2. Keep all original physical HDMI connections, Sierra installation, driver version, Linux guest and source architecture unchanged.
3. Confirm the original Blackmagic's actual power/PCIe attachment within the dock and whether a controlled power-up sequence is needed. This requires a physical or independently measured witness; do not assert that software can reset an absent device.
4. Do not initiate a remote host reboot or dock power-off while xhyve/Linux Docker is running; the Linux guest's clean shutdown and long-run stability are separately unaccepted.
5. Once `BDBD:A124` reappears, verify DeckLink iterator enumeration, the original `BMD HDMI` EDID sink and 1080p30 mode, then the exact original 2-consecutive-input-lock gate, **then** the fresh 30-frame capture -> 60-frame HDMI output API replay -> independent raw SHA verification. Optical monitor output still needs an independent witness.
6. Retain historical **real** physical 2-frame capture and the **17-frame** same-silver Linux Docker ingest as past accepted evidence. The current state is **not** accepted as recovered.

**Verdict:** new upstream device-enumeration blocker after physical Thunderbolt reconnection, stronger than the prior HDMI input `NO_LOCK`. PR #10 remains DRAFT.

## Downstream Thunderbolt hypothesis (external model documentation; no physical witness yet)

The original **Blackmagic Intensity Extreme** is a Thunderbolt bus-powered endpoint; it does not use its own external power adapter. The **i-tec / Comdis THUNDERDOCK2** provides two Thunderbolt 2 ports (upstream to the Mac, other for downstream Thunderbolt devices). Sources:

- https://www.tvtechnology.com/be-blogs/2836 — original Intensity Extreme launch announcement, bus-powered.
- https://www.multitronic.fi/en/products/2121608 — model THUNDERDOCK2, two Thunderbolt 2 ports.
- https://www.manualslib.de/manual/758117/I-Tec-Thunderbolt-2.html — dock instruction manual.

The current Sierra Thunderbolt inventory says for the **THUNDERDOCK2 downstream Port**: `Status: No device connected`. Given the fixed single-Thunderbolt-port 2011 silver Mac, a downstream bus-powered Intensity Extreme would have to enumerate beyond the dock. Its absence together with the missing PCI `BDBD:A124` is consistent with loss of the downstream Thunderbolt link or bus power, **not proof of a failed HDMI decoder**. It remains unverified which exact Thunderbolt cable the operator unplugged and reconnected.

**Operator clarification at 17:46 CEST:** All existing physical connections are connected and the fixed fixture is final. Do not repeatedly ask for cable reseating. The then-open uncertainty was electrical link/power negotiation or endpoint enumeration, *not* whether the operator completed the wiring. A controlled Sierra restart was subsequently authorized and executed; see the post-restart witness below. Continue not to change the existing HDMI fixture, kernel, firmware, or drivers.


## Controlled Sierra restart — executed, verified, and unsuccessful at restoring Blackmagic (18:04:31 CEST)

The operator authorized a Sierra restart as the next reversible recovery test, without any operating-system reinstall, driver replacement, new HDMI source or recabling.

**Preflight and guest handling.**
- Remote control confirmed the exact 2011 silver Macmini5,3, Sierra 10.12.6, original internal SATA Fresh volume, Wi-Fi SSH route independent of the dock, FileVault OFF and AC power.
- One offline **xhyve Alpine Linux/Docker guest** was running with persistent 12,884,901,888-byte docker-guest.raw and private 4,194,304-byte fountain-hdmi-mailbox.raw. The guest's serial console was unresponsive to a bounded clean poweroff attempt. **Clean Linux shutdown is not witnessed.**
- The mailbox was copied privately and its original/backup SHA-256 **agreed**: 96051895b40b1a3351c0bc1bd6092c770b7e39f841b92c9db16e2e871297e829. Existing mailbox frame provenance and guest disk preserved.
- A private gzip-compressed recovery copy of the 12 GiB virtual disk was created at /Volumes/Fresh/FountainVideoDockerLab/private-restart-20261024/docker-guest.raw.gz; its completed size was **797,087,705 bytes**, file permissions **0600**. gzip -t passed (exit 0). Compressed archive SHA-256 verified before and after reboot: 41c5212e3d6123a063c04f3c67bf5c6024bc11748f3b6fe928d5cbffdb391b4d.
- An attempted full original-vs-decompressed-disk SHA-256 equality pass was **interrupted** by a concurrent remote command before acceptance; the original guest disk's on-host modification timestamp remained **15:18:54 CEST** throughout checks. Do **not** call the recovery archive independently proven byte-identical or the guest filesystem clean. It is a gzip-integrity-checked, *crash-consistent recovery copy*, not an application-consistent Linux backup. The unmodified original remains primary.
- The xhyve host process ultimately exited after SIGTERM; no xhyve process held its raw disk when Sierra restarted. No forced physical power cut or guest autostart.

**Reboot and independent system boot evidence.**
- A regular Sierra GUI restart (AppleScript / System Events) was initiated from the authorized remote Mac. SSH disappeared and subsequently returned.
- Original kernel boot time: **2026-10-22 05:58:59 CEST** (kern.boottime seconds 1792641539).
- New kern.boottime: **2026-10-24 18:04:31 CEST** (seconds 1792857871, verified at 18:05:49), machine uptime ~1 minute. This is **a full kernel/Thunderbolt reboot**, not merely a GUI logout.
- SPThunderboltDataType: the **Comdis THUNDERDOCK2** re-enumerated (upstream link 0x2), but its **downstream Thunderbolt port still shows Status: No device connected, Link Status: 0x7**.
- SPPCIDataType: the dock's USB controller **1b73:1100** and Ethernet controller **8086:1533** enumerate, but original Blackmagic **BDBD:A124 remains absent**.
- The original Desktop Video **10.4 framebuffer kext** appeared in kextstat, while the core Blackmagic device driver did **not** appear in that postboot output; driver loading alone cannot make the missing PCI device physically present.
- SPDisplaysDataType showed no active BMD HDMI sink. In the just-rebooted headless login state, missing display rows should not be interpreted independently as a receiver diagnosis; the PCI absence is decisive.
- The Alpine guest remains **stopped** (pgrep -x xhyve found no process after reboot). Original guest disk and mailbox retain their correct sizes and 0600 permissions, and the separate private recovery archive survived reboot with unchanged SHA.

**Verdict: RESTART EXECUTED; HARDWARE NOT RECOVERED.** The silver remains the canonical Sierra host, the dock is electrically/live enumerable, but its bus-powered downstream Blackmagic Intensity Extreme has not reappeared. No input-lock, capture, or HDMI output test was attempted postboot. The original NO_BLACKMAGIC_PCI_DEVICE gate remains the correct block, and PR #10 stays **DRAFT**.

**Next discriminating boundary:** examine downstream Thunderbolt bus-power/link training and the Blackmagic endpoint's actual power state independently, *without* repeating basic "everything connected" checks. A further Sierra reboot, software reinstall, kernel driver reset or video-mode experiment is **not** justified by this outcome. The host-side simulator and historical 17-frame delivery evidence are preserved and can be developed independently.
