# Ubuntu A124 PCIe contract acceptance — 24 October 2026

**Lane:** Original Blackmagic Intensity Extreme hardware **protocol first**. Modern Ubuntu is the preferred native execution host. Original silver 2011 Mac mini remains an independently measured physical reference and keeps its working Sierra installation.

## Exact hosts

**Physical reference:** Uwes-Mac-mini, Macmini5,3, macOS Sierra 10.12.6. Original Intensity Extreme (PCI 0xBDBD:0xA124) remains connected through THUNDERDOCK2 over Thunderbolt. Original Blackmagic 10.4 kernel driver active; BMD HDMI is its online main display at 1920x1080/30. Nothing was disconnected, upgraded, installed or flashed.

**Native target:** benedikt-Macmini, Macmini8,1, Ubuntu 24.04 LTS, Linux 7.1.8-1-t2-noble x86-64. Two physical TB3 host domains: domain0 and domain1; both security=user, iommu_dma_protection=0; kernel command line includes intel_iommu=on iommu=pt but group isolation for a future A124 device is NOT measured. Only root routers 0-0 and 1-0 present. **No local PCI 0xBDBD:0xA124.**

## New source-owned Ubuntu hardware-facing contract

- hardware/ubuntu/a124_pci_port.py discovers original A124 via Linux /sys/bus/pci/devices, reads resource metadata (not BAR memory), witnesses expected **two 1 MiB memory BARs** only when actually locally present, reports TB device authorization and domain security separately.
- Injected register replay uses only two static-evidence BAR0 locations **+0x40** (input status) and **+0x224** (firmware low byte), with values supplied explicitly as synthetic test inputs. Unknown register offsets, absent values, every write, DMA, IRQ and firmware mutation fail closed. A reply is marked "synthetic-register-replay", not observed hardware.
- scripts/verify-a124-ubuntu-contract.sh compiles and tests the existing SOURCE-OWNED C firmware/status/read-contract implementation natively on Ubuntu, then checks actual Linux sysfs read-only.

## Tests independently executed on real Ubuntu

    sh scripts/verify-a124-ubuntu-contract.sh

Actual 24 October result: **exit 0**.

- 15/15 Python PCI sysfs/TB/replay tests.
- 6/6 C firmware gate cases.
- 11/11 C input status cases.
- 12/12 C injected read contract cases.
- **44/44 hardware-contract software cases passed**, with no actual A124 I/O.
- Real host Linux inventory: physical card **absent**; no external Thunderbolt devices. The command *never* reads PCI config, opens BAR resource files, authorizes Thunderbolt, binds drivers or performs DMA.
- The separate pre-existing downstream video media simulator also passed **88/88** Python regression tests on this same Ubuntu host.

The "real PCI required" command below currently returns **2** to unambiguously report absence:

    python3 -m hardware.ubuntu.a124_pci_port --require-physical

**Neither a passing 44-test replay contract nor 88 media tests declares a physical device revived.**

## Physical transport/ownership decision

Preferred next physical witness: with a person present, connect this original Thunderbolt 1 Intensity Extreme through a *compatible and sufficiently powered* TB3 ↔ TB1/2 adapter/dock chain to the Intel Ubuntu Mac mini, preserving any other connections at silver. Apple documents that its TB3-to-TB2 adapter connects TB1/TB2 docks/peripherals; the exact bus-powered Intensity Extreme path has **not been tested**. Original silver may continue as HDMI signal source.

Linux TB policy on Ubuntu is security=user with iommu_dma_protection=0; a future external router/device must be independently authorized under an explicit policy and its specific PCI IOMMU group/isolation must be examined **before any DMA or bus mastering**. No automatic authorization rules.

If the physical card must remain on silver, Ubuntu can still run the replay and downstream clients, while silver is a separate read-only hardware witness or future minimal PCI bridge. A LAN connection is **not PCIe passthrough**, and a Sierra-based hardware bridge does not constitute full OS independence.

[Hardware-fidelity architecture](../docs/a124-hardware-fidelity-ubuntu.md) · [physical Ubuntu PCI attachment issue #9](https://github.com/Fountain-Coach/blackmagic-intensity-extreme-revival/issues/9) · [still-open revival issue #8](https://github.com/Fountain-Coach/blackmagic-intensity-extreme-revival/issues/8).

**Result:** source-owned Ubuntu hardware-interface software ACCEPTED; actual original card locally enumerated on Ubuntu **NOT YET**; firmware, BAR0 live read, BAR1 semantics, DMA, interrupts and real HDMI capture **NOT ACCEPTED**. No Sierra recovery/driver upgrade is a prerequisite to continuing Ubuntu source development.
