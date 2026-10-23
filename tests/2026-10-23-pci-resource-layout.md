# PCI resource descriptor observation — Sierra, 2026-10-23

We ran an independently written IOKit **read-only registry** inspector on `Uwes-Mac-mini` (2011 Intel Mac mini, macOS 10.12.6) against the physical Intensity Extreme, vendor `0xBDBD`, device `0xA124`.

Build: `clang -std=c11 -Wall -Wextra -Werror src/intensity-pci-resources.c -framework IOKit -framework CoreFoundation -o /tmp/fc-intensity-pci-resources`.

```text
stage=target vendor=0xbdbd device=0xa124
stage=resources descriptor_count=2 (registry_only)
stage=pci_resource index=0 config_offset=0x10 type=0x82 size_bytes=1048576
stage=pci_resource index=1 config_offset=0x14 type=0x82 size_bytes=1048576
stage=complete matched=1 status=0
probe_exit=0
```

**Meaning:** I/O Registry advertises two PCI memory resource descriptors, one at conventional configuration BAR offset `0x10` and one at `0x14`, each with a 1 MiB resource extent. The `type=0x82` value is the descriptor's upper address-space tag byte, not a proven chipset identity or mapped bus-access permission. Physical mapped addresses are deliberately not recorded in published output.

The source decodes only `assigned-addresses` in the public I/O Registry, never accesses the memory itself. BAR function and internal register addresses are **unknown**. The observation does not show firmware readiness, HDMI receiver lock, DMA function, or a successful video frame.

An additional read-only I/O Registry inventory showed that the existing Blackmagic `com_blackmagic_design_iokit_DeckLink_Driver` service remains the attached `IOPCIDevice` child; its user-client class is `com_blackmagic_design_iokit_DeckLink_DaisyCutterUserClient`. These are discovered names, not copied vendor implementations.

**Next engineering gate:** determine which BAR implements control registers and which is a transfer/aperture region using static evidence and reversible measurements. Never probe unknown MMIO offsets by blind writes. The independent driver remains offline and intentionally has no matching personality.
