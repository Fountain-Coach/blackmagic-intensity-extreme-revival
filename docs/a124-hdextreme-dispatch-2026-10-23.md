# A124 driver dispatch and HDExtreme memory-mapping evidence

**Date:** 2026-10-23.  
**Status:** static reconstruction supported by model-table verification; not a live driver protocol.  
**Subject:** original Blackmagic Intensity Extreme, PCI vendor `0xBDBD`, device `0xA124`, tunneled over Thunderbolt to a Sierra Mac mini.

## Independently reproducible dispatch evidence

The authorized, privately held Desktop Video 10.4 driver is an x86-64 Mach-O kernel extension with SHA-256:

```text
c4ec945aa46d07123788e66c219203026d1dcddffad20d1a55b8b5b4943d3780
```

Privately retained REA/Ghidra 12.1.4 evidence for `DeckLink_Driver::start` (image-relative `0x1a04`) shows that the driver reads the PCI device ID and, for some model ranges, consults a fixed-stride model table before choosing its hardware abstraction. The matching table branch constructs the **`HardwareAbstractionDeckLinkHDExtreme`** class.

A separate [source-owned, SHA-pinned and read-only Mach-O parser](../scripts/inspect_private_model_dispatch.py) extracts only the chosen model-table index and code. It does not publish driver bytes, firmware, pseudocode, serial numbers or memory addresses. The parser uses the verified device-ID field at table VA `0x56be2` and 77 entries of 16 bytes each, reading the model code in the preceding two-byte field.

Run against a **separately authorized local copy only** (never an installed bundle or a downloaded binary distributed by this repository):

```sh
python3 scripts/inspect_private_model_dispatch.py /path/to/authorized/DeckLink_Driver
python3 -m unittest discover -s tests -p test_private_model_dispatch.py -v
```

Actual output against the privately held binary:

```json
{
  "hardware_access": false,
  "model_code": "0x36",
  "model_table_index": 54,
  "model_table_va": "0x56be2",
  "pci_device_id": "0xa124",
  "runtime_class_verified": false,
  "sha256": "c4ec945aa46d07123788e66c219203026d1dcddffad20d1a55b8b5b4943d3780",
  "status": "static_table_match"
}
```

**All four synthetic parser tests passed on the Ubuntu analysis host**, and the parser returned the above result against the original hash-matched binary. No proprietary fixture is committed.

Independent Ghidra analysis of `HDExtreme::Init` (image-relative `0x2ab02`) contains an **explicit case for model code `0x36`**, corroborating the dispatch table interpretation. We have not instrumented the internal runtime object type.

## Actual model's first PCI memory aperture

Earlier analysis of the *generic* `HardwareAbstractionDeckLinkPCIExpress::Init` suggested a memory mapping at configuration register `0x10`; **that observation alone was not valid evidence for device A124**.

The device-specific `HardwareAbstractionDeckLinkHDExtreme::Init` also issues a map-like, indirect call on its `IOPCIDevice` argument with `0x10` and then obtains the mapped virtual address. It derives a family of register/control pointers from that address. This is **model-specific static evidence consistent with the first PCI BAR (`0x10`) serving as a register aperture**.

Separately, a source-owned [live I/O Registry probe](../src/intensity-pci-resources.c) established two 1 MiB PCI memory resources at configuration offsets `0x10` and `0x14` on the physical device. The second region's role is still unknown.

We have **not independently resolved the exact indirect I/O Kit mapping method, mapped or read either aperture, or established live BAR register semantics**. Even the stronger HDExtreme-specific evidence must not be treated as permission for speculative memory-mapped writes.

## Video DMA implications

The HDExtreme class has its own `DMAGetAddressForCapture` (image-relative `0x2f720`) and `DMASizeofScatterGatherBuffer` (`0x2f786`) entries. Private decompilation of **both** confirms **capability-dependent branches**: the first returns different capture address/selector values depending on capture format and mode, with a fallback to an inherited HDPro implementation; the second calls into the HD base implementation and conditionally raises scatter/gather buffer requirements. Thus neither the capture address nor scatter/gather buffer size is an unconditional hardware constant. The meaning and bus/register space of returned address values have not been established experimentally.

This means the independent driver must explicitly negotiate and validate DMA descriptor geometry and buffer ownership. It is unsafe to hardcode a scatter/gather size based solely on a nominal video resolution or one decompiler output. DMA completion and interrupt behaviors were separately documented in [D3 DMA evidence](dma-protocol-evidence-2026-10-23.md).

## Exact remaining engineering gates

1. Determine the identified `HDExtreme` capture-DMA address routine's semantics and verify the applicable descriptor layout, mapping method, and interrupt/status register roles.
2. Separately confirm model-specific firmware initialization and the actual HDMI signal-lock input path; a valid physical frame is still missing.
3. Preserve a bootable, recoverable alternative environment before installing or binding our own PCI driver. The existing Desktop Video 10.4 kernel driver remains the live control; the crashing vendor CoreMediaIO plugin remains isolated.

**No driver installation, firmware upload, EEPROM write, bus-master enable, BAR read/write or DMA execution occurred.**
