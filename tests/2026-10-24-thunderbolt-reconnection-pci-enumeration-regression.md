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

**Next most discriminating physical observation:** without substituting any HDMI source or moving the existing HDMI cables, verify the **dock-to-Intensity Thunderbolt cable is fully seated at both ends**, the original Intensity's own power/LED state (if visible), and that the dock has its specified AC adapter connection. Check inventory again after any deliberate physical inspection. Do not remote-reboot the silver, change driver/kernel, or reset firmware to compensate for an absent downstream Thunderbolt endpoint. If physical handling is necessary, avoid hot-unplugging a dock carrying active storage; first ensure safe shutdown of dependent workflows.
