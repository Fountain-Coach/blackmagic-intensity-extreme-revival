# A124 hardware fidelity on current Ubuntu

**Decision, 24 October 2026:** Preserve the original silver Mac mini and original Intensity Extreme as *physical evidence*. Implement and test the hardware-interface contract on the **existing Ubuntu 24.04 Mac mini**, virtualizing only unobserved/temporarily disconnected interface behavior as explicitly SYNTHETIC replay. Do not replace native hardware observations with tests of a simulated finished application.

**Correction:** Sierra OS rehabilitation, a Desktop Video 10.11.2 installation and its recovery procedure are NOT prerequisites to the independent Ubuntu hardware engineering path. Their evidence remains useful as historical research but does not own this project's development lane.

## Observed, real topology

2011 silver Mac mini (Macmini5,3), Sierra 10.12.6:

    Silver Mac mini -- Thunderbolt 1 -- Comdis THUNDERDOCK2
      -- ORIGINAL Intensity Extreme, PCI 0xBDBD:0xA124
           PCI BAR0 at config 0x10: 1,048,576 bytes
           PCI BAR1 at config 0x14: 1,048,576 bytes
           HDMI IN fed by silver HDMI OUT at 1920x1080/30

The silver's BMD HDMI display is currently the main display. Unplugging hardware without local oversight could alter its display configuration or the dock's other devices. The original Blackmagic 10.4 KEXT still controls this card. **No genuine input frame has been captured.**

Current Ubuntu machine:

    Apple Macmini8,1, Intel x86-64, Ubuntu 24.04
    Kernel 7.1.8-1-t2-noble
    TWO real Thunderbolt 3 host controllers / domains
    domain0 + domain1: security=user, iommu_dma_protection=0
    Kernel boot parameters include intel_iommu=on iommu=pt
    Only Thunderbolt host routers 0-0 and 1-0 are present
    NO external Thunderbolt device or PCI 0xBDBD:0xA124

**Ethernet cannot expose a PCIe device physically attached to silver as a local PCIe function on Ubuntu.** Sending frames over the LAN is a media transport, not remote PCIe, DMA or interrupts.

## Hardware protocol layers: evidence, not guesses

| Interface | Hardware evidence | Ubuntu contract now | Next independent witness |
| --- | --- | --- | --- |
| Thunderbolt | Real original chain with dock | Read-only sysfs domain/security survey | External card/router connected to Ubuntu |
| PCI endpoint | Vendor BDBD/device A124, two 1 MiB BARs | Source-owned PCI identity/resource inventory and fixture | Exact A124 enumerated on Ubuntu and resource/IOMMU group confirmed |
| BAR0 registers | Vendor code's candidate input status BAR0+0x40 and firmware low byte BAR0+0x224 | Only these addresses may be read from **injected, explicitly synthetic** data | Exclusive, bounded real register observation under a safe Linux driver |
| BAR1 | Existence and size observed; meaning unknown | No reads or fabricated semantics | Independent mapping/function verification |
| Firmware | Source-derived expected byte 0x0B, conditional gate | Existing pure C injected firmware gate | Real firmware state observed; NEVER auto-flash |
| DMA and interrupts | Partial static driver-path evidence | Deliberately unavailable; no bus master, buffers or IRQs | Validated IOMMU isolation, descriptors, ownership, IRQ completion |
| HDMI/frames | Silver framebuffer changes; legacy vendor input lock false | No physical capture claim | Real locked receiver, timestamped changing pixels |
| Browser | Synthetic frame/JPEG transport works | Downstream consumer | Must show the real card's frame provenance |

Current source-owned files:
- hardware/ubuntu/a124_pci_port.py — Linux sysfs PCI and Thunderbolt inventory; fail-closed replay of two known BAR0 read locations, with *no default register values* and no writes, DMA, IRQ or firmware mutations.
- tests/test_a124_ubuntu_hardware_port.py — 15 synthetic sysfs and safety tests.
- src/intensity-register-contract.c, intensity-firmware-gate.c, intensity-input-status.c — existing native C interface-specific models; no need for a named creative-coding abstraction.
- scripts/verify-a124-ubuntu-contract.sh — compile/test the actual pure C firmware, input-status and register-read functions, then survey real Ubuntu /sys **without privilege**.

### Current acceptance

On the actual Ubuntu Mac mini, the above script **exited 0**:
- 15/15 PCIe/sysfs/replay tests;
- 6/6 native C firmware-gate cases;
- 11/11 native C input-status cases;
- 12/12 native C injected register-read cases.
- Live host inventory: **0 physical A124 devices**, both TB domains user-authorized mode with IOMMU DMA protection attribute 0, **no external Thunderbolt routers**.

A 44-test software interface suite is **NOT** 44 physical hardware tests, nor a QEMU PCI endpoint implementation.

## Hardware connection and virtualization choices

**Preferred (physical PCI on modern Ubuntu):**

    Ubuntu Mac mini Thunderbolt 3
         |
    compatible TB3 to TB2 bidirectional adapter
         |
    original Thunderbolt cable / Comdis dock if appropriate
         |
    this same Intensity Extreme, PCI 0xBDBD:0xA124
         |
    unprotected known HDMI source, e.g. silver HDMI OUT

Apple documents that its Thunderbolt 3-to-Thunderbolt 2 adapter can connect Thunderbolt 1/2 peripherals and docks to Thunderbolt 3 Macs: https://support.apple.com/de-de/111753 . This exact combination with the original bus-powered Intensity Extreme is **unverified**, including power provision and existing dock dependencies. No adapter or physical rewiring was attempted.

Before authorization or DMA, check the specific Thunderbolt device, the PCIe function and the full IOMMU group. The Ubuntu host reports iommu_dma_protection=0 on both TB domains even though its kernel boot options include intel_iommu=on; **do not assume device DMA isolation**. No auto-authorize rule or VFIO binding is installed. Linux reference: https://www.kernel.org/doc/html/latest/admin-guide/thunderbolt.html .

**Secondary (card remains physically on silver):** Ubuntu can develop replay/models and receive read-only inventories over SSH. A live register/IRQ/DMA hardware bridge would still need a *safe silver-side PCI owner*; a normal network connection cannot forward arbitrary PCIe transparently. Do not call a DeckLinkAPI-over-SSH proxy operating-system-independent: it would keep the crashing vendor layer.

**Virtualization rule:** Only known PCI IDs/BAR extents, known candidate register offsets and verified protocol transitions may be simulated. Unknown register results fail closed; neither a generic synthetic video source nor a QEMU-shaped fake claims real firmware, DMA or HDMI.

## Next acceptance order

1. **Completed:** Ubuntu model/interface tests and actual read-only kernel inventory.
2. **Physical step not yet performed:** deliberately connect the original card to Ubuntu through a verified adapter/dock chain with someone present to observe cable/power/display consequences. Alternative: explicitly retain it on silver as a separate physical host.
3. Run a nonprivileged Linux PCI/TB inventory and obtain local vendor BDBD/device A124, BAR geometry and IOMMU group. Run python3 -m hardware.ubuntu.a124_pci_port --require-physical; nonzero exit **2** currently means no physical target.
4. Only after isolating device ownership and DMA security, implement bounded PCI BAR reads with verified offsets and event traces; never guess a write offset.
5. Reconstruct model-specific firmware, descriptor, IRQ and DMA semantics from trustworthy evidence, build a strictly limited Linux physical driver/backend, then witness actual HDMI lock, pixels, timestamps and crash-free shutdown.
6. Browser Scratch becomes a real-device consumer only after physical capture proof.

**No claim:** physical card connected to Ubuntu, QEMU endpoint implemented, proprietary driver bypassed, bus mastering enabled, real register read, HDMI signal, DMA, or original hardware revival.

The prior Sierra rescue/bootstrap effort is **deprioritized** and not a gate to Ubuntu source development. No user disk, kernel, driver, cable or firmware settings were changed in this step.
