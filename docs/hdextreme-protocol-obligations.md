# HDExtreme hardware protocol obligation ledger

Date: 24 October 2026. Target: Thunderbolt Intensity Extreme PCI vendor `0xBDBD`, device `0xA124`, model code `0x36` according to the SHA-pinned static model-table inspector.

**Purpose:** define evidence requirements before any kernel module is permitted to claim, map, read or write physical PCI resources. This is an independently authored specification, not vendor code and not a completed hardware driver.

| Contract | What is directly established | What is unresolved | Acceptance before live access |
|---|---|---|---|
| Device identity | Live I/O Registry gives `0xBDBD:0xA124` and an attached `IOPCIDevice`; model table index 54 maps to code `0x36` in the checked vendor binary. | Loaded runtime's selected class not independently instrumented. | Consistent identifier and device model on both cold and warm boot. |
| PCI resources | Two 1 MiB assigned resources at BAR configuration offsets `0x10`, `0x14`. | Register semantics and role of BAR1. | Verified resource ownership and mapping semantics with a reversible experiment. |
| BAR0 candidate | HDExtreme `Init` calls a map-like `IOPCIDevice` method with `0x10` and derives control pointers. | The indirect method's identity and device-side register behavior. | Resolve method and dataflow statically and corroborate register meaning against safe observations. |
| Firmware readiness | Hardware initialization and firmware-downloaded paths gate subsequent support initialization. | Actual observed readiness state, reset paths and firmware dependency. | Independent observation plus verified transition, without flashing or guessed state writes. |
| HDMI detection | Model `0x36` uses base detected-input-format implementation, whose gates include readiness and a status flag/timing lookup. | Input cable/source state and live hardware status. | A compatible, unprotected source verified at the capture input; lock and timing checked against it. |
| Video DMA | Private Ghidra analysis identifies queue, memory descriptor, scatter/gather, launch, interrupt and completion pathways; HDExtreme capture-address handling is capability-dependent. | Ownership, addressing, descriptor layout, cancellation, interrupt acknowledge, bounds and fault recovery. | A complete bounded, independently validated protocol with a rollback path. |
| macOS integration | Original kernel driver is loaded; the defective CoreMediaIO plug-in stays isolated. | Independent frame interface and correct Sierra compatibility. | First valid timestamped frame from hardware, then separate video integration. |
| Recoverability | Data has Sierra, Fresh has existing files and lacks expected OS boot files; Recovery HD present, SIP enabled, no Time Machine destination. | Tested alternative boot and recovery without SSH. | Independently bootable, tested and restorable environment before own PCI match. |

## Next safe implementation sequence

1. Resolve the mapping method and pointer dataflow of HDExtreme initialization (not the generic PCIExpress subclass).
2. Trace input status read and video timing decode in the base class with unmodified private binary evidence.
3. Build source-owned decoder/tests for those static findings, keeping all vendor bytes out of Git.
4. Preserve the current boot and original Blackmagic drivers; continue read-only I/O Registry checks.
5. Obtain a distinct, validated recovery system before any live kernel driver operation.

**Non-claims:** no independently verified HDMI lock, firmware readiness, DMA, MMIO read/write, tested alternative boot or physical captured frame.
