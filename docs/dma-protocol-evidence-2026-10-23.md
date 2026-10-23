# D3: Read-only driver attachment and DMA evidence, 23 October 2026

**Physical validation:** Intensity Extreme (`0xBDBD:0xA124`) on a 2011 Intel Mac mini, macOS 10.12.6, Blackmagic Desktop Video 10.4. The vendor's original driver remains loaded. **No firmware or PCI write, MMIO mapping, DMA allocation, kernel-driver replacement or kernel-extension load was attempted in this investigation.**

## Source-owned user-space registry witness

[`src/intensity-kernel-status.c`](../src/intensity-kernel-status.c) uses Apple's I/O Kit registry APIs only. It does not open a proprietary user client or call DeckLinkAPI, and explicitly reports firmware/DMA readiness as unknown unless separately demonstrated.

It was compiled **from a fresh Git checkout on the actual Sierra host** with

```sh
clang -std=c11 -Wall -Wextra -Werror src/intensity-kernel-status.c \
  -framework IOKit -framework CoreFoundation -o /tmp/fc-intensity-kernel-status
/tmp/fc-intensity-kernel-status
```

Observed:

```text
stage=vendor_service attached=1 parent_pci=1 vendor=0xbdbd device=0xa124
stage=driver_published power_current_known=1 current=3 power_max_known=1 max=3 instance_known=1 instance=0
stage=published_children audio=1 user_clients=1 other=0
stage=pci_match selected=1
stage=conclusion services=1 firmware_ready=unknown dma_ready=unknown receiver_locked=unknown
git_status_probe_exit=0
```

The `current=3/max=3` values are the *vendor service's published I/O Power Management state*, not a separate proof of boot firmware completion. The single user-client child reports a system helper; neither its registration nor the audio child verifies HDMI receiver lock, correct DMA setup, or transfer of genuine source pixels. The probe is read-only and reproducible.

## Reverse-engineering evidence — original binary, private analysis host

The authorized legacy kernel binary was inspected privately on the local Ubuntu analysis host with REA 6.7.0 / Ghidra 12.1.4. SHA-256 of the inspected binary: `c4ec945aa46d07123788e66c219203026d1dcddffad20d1a55b8b5b4943d3780`.

New structured evidence files retained **only on the private analysis host**, not uploaded:

- `evidence/2026-10-23-dma-procedure-search.json`: 48 named `DaisyCutterDMASupport` procedures, including initialization, queueing, allocation, prime/cancel, start, completion, frame transactions, interrupt and cleanup.
- `evidence/2026-10-23-dma-init.json`: class initialization at image-relative `0x19262`.
- `evidence/2026-10-23-dma-start.json`: start-next-transaction path at `0x19856`.
- `evidence/2026-10-23-dma-irq.json`: interrupt path at `0x1b2a0`.
- `evidence/2026-10-23-dma-complete.json`: completion path at `0x19892`.
- `evidence/2026-10-23-dma-new-frame.json`: frame DMA transaction creation at `0x1a292`.
- `evidence/2026-10-23-hardware-abstraction-procedures.json`: 509 matching hardware-abstraction procedure symbols, including several hardware-specific variants.
- `evidence/2026-10-23-pcie-abstraction-init.json`: candidate PCI Express implementation initialization at `0x266e2`.
- `evidence/2026-10-23-pcie-init-xrefs.json` and `evidence/2026-10-23-pcie-constructor-xrefs.json`: virtual method and constructor-reference evidence; no confirmed `A124` dispatch yet.

### Reconstructed behavior, not a hardware register contract

| Stage | Evidence-derived behavior | What is still missing |
| --- | --- | --- |
| Firmware gate | Earlier `firmwareHasDownloaded` analysis conditionally checks a hardware-usable result before initializing DMA and video support components. | No independently measured hardware-usable result or validated write sequence |
| DMA preparation | The vendor DMA-support class allocates a transaction table and kernel memory descriptors, and prepares memory-backed buffers. Frame-transaction construction distinguishes queued/reusable transaction elements and persistent mappings; paths can construct `IOMemoryDescriptor` objects from supplied address ranges. Allocation and preparation have explicit failures. | Exact ABI, verified map ownership, buffer scatter/gather layout, and negotiated transfer geometry for this hardware |
| Start-next | The driver checks for a queued transaction and a not-already-started state before calling a hardware abstraction method. | Target of the indirect call and resulting PCI register/address mapping |
| Interrupt | The handler reads status indirectly, may detect a completed transaction, and propagates events separately to video output and input services. | Validated interrupt source, acknowledgement, completion flags and sequence units |
| Completion | An existing active transaction is handed to a disposal/completion routine. | Exact release, reuse and cancellation guarantees |

**Interpretation boundary:** Ghidra produced pseudo-code with unrecovered indirect branches. A decompiled call position is not a verified memory-mapped register offset. No DMA transfer has been performed by the independent driver.

## PCI resource context

The separately validated, read-only [PCI descriptor probe](../src/intensity-pci-resources.c) found **two 1 MiB** PCI memory resource descriptors at configuration offsets `0x10` and `0x14`. Their individual functions have **not** been established. The independent driver must not read or write unknown BAR memory or enable bus mastering as an exploratory shortcut.

## Candidate PCI Express memory map — *not yet device-specific*

The privately inspected `HardwareAbstractionDeckLinkPCIExpress::Init` procedure (image-relative `0x266e2`) makes a map-like indirect call on an `IOPCIDevice` with register argument `0x10`. It obtains an address from the returned object and derives a collection of control/status pointer locations from that base (including offsets within the first `0x8000` bytes). That is consistent with the independently observed PCI resource beginning at configuration offset `0x10` and is **evidence for a BAR0 register-mapping hypothesis**.

It is **not yet proof** of the exact I/O Kit method being called, the underlying PCI address translation, or that the `A124` hardware actually chooses this particular subclass. Multiple hardware-abstraction subclasses exist in this binary. The second PCI BAR at `0x14` remains functionally unassigned. No derived address is permissible for live access based only on this observation.

## Evidence gates for the next real driver operation

1. Trace the vendor driver's `IOPCIDevice` resource selection and the hardware abstraction's calls that launch DMA. Prove which PCI aperture carries control/status and which, if any, maps transfer memory.
2. Document bounded transaction ownership and completion semantics, including fault/reset and cancellation. Do not presume that callback frames marked `no-source` indicate successful physical DMA.
3. Compare firmware-ready and signal-lock states against a physically confirmed, compatible **unprotected HDMI signal**.
4. Prepare an explicitly recoverable alternative boot before attaching a new PCI driver. Preserve Blackmagic 10.4 as the operational control and the old CoreMediaIO plugin in isolation.

**Current acceptance:** independent PCI inventory and passive offline build passed; registry witness passed. *Independent MMIO, firmware control, DMA, interrupts and real video input remain unimplemented and unverified.*
