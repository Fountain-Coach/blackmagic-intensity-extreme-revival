# A124 HDExtreme input status path — independently corroborated

Date: 24 October 2026. Target: the original Intensity Extreme, PCI `0xBDBD:0xA124`, Desktop Video 10.4 original kernel extension, running on macOS Sierra 10.12.6.

**Status:** reproducible STATIC software call-chain identification. No physical status register was mapped/read by our code; no independent driver binding or HDMI frame capture occurred.

## Model and dispatch provenance

The authenticated kernel image, held only on the private Ubuntu analysis host, has SHA-256 `c4ec945aa46d07123788e66c219203026d1dcddffad20d1a55b8b5b4943d3780`.

Our read-only [model-dispatch inspector](../scripts/inspect_private_model_dispatch.py) independently extracts `0xA124` as table index 54 / model code `0x36`. Private Ghidra evidence for `DeckLink_Driver::start` supports selection of `HardwareAbstractionDeckLinkHDExtreme`; HDExtreme `Init` has an explicit `0x36` branch. This selection is *static* and not a live introspection of the attached object's class.

## End-to-end static call chain

1. HDExtreme `Init`, image-relative address `0x2ab02`, invokes the IOPCIDevice virtual method at vtable byte offset `0x908` with configuration register `0x10` and options `0`. It stores the result and invokes the map object's method at vtable offset `0x118`.
2. Sierra Xcode 9.2 compiler vtable layout against its installed public kernel headers independently resolves those slots to `IOPCIDevice::mapDeviceMemoryWithRegister(UInt8, IOOptionBits)` and `IOMemoryMap::getVirtualAddress()` respectively. The address point starts after the two standard Itanium ABI prefix entries. Our [compile-only verifier](../driver/sierra/passive/verify-abi-layout.sh) returned zero on the Sierra host: `pci_slot=291`, `0x908`; `memory_map_slot=37`, `0x118`.
3. The HDExtreme initializer stores a pointer derived from this mapped BAR0 virtual address plus offset **`0x40`** in its internal `this+0x88` member. This derives from private Ghidra reconstruction, not a live MMIO measurement.
4. For model `0x36`, HDExtreme `VideoInputGetDetectedInputFormat` delegates to the base `HardwareAbstractionDeckLink::VideoInputGetDetectedInputFormat`. The base implementation checks private object readiness/feature fields, then passes the `this+0x88` status pointer to an indirect method at vtable byte offset **`0x7a0`**.
5. Our [SHA-pinned Mach-O symbol/vtable verifier](../scripts/inspect_private_status_dispatch.py) independently parses the **HDExtreme** vtable and resolves offset `0x7a0` to inherited `HardwareAbstractionDeckLinkHDProPCIExpress::ReadRegisterEndian` at `0x2a0ca`. The adjacent write dispatch (`0x7a8`) resolves to `WriteRegisterEndian`, but it is **not invoked or authorized** by the diagnostic.
6. Private Ghidra decompilation of `ReadRegisterEndian` (`0x2a0ca`) shows a 32-bit memory read while an internal guard permits it, returning `0xffffffff` when a register response is invalid and conditionally marking an internal guard. This is a reconstructed fault path, not a safe-read guarantee for a live device.

**Interpretation:** the original vendor input-format detector appears to examine a 32-bit status word at **BAR0 + `0x40`**. This is now a strongly corroborated, *model-specific, static* register-offset candidate. It does not certify that a read has no side effect, that a valid physical HDMI signal is present, or that the same binary behavior has been witnessed under runtime instrumentation.

## Source-owned classifier and bounded tests

The pure [input-status classifier](../src/intensity-input-status.c) models the gates visible in the base function without touching I/O Kit, DeckLinkAPI or BAR memory. It refuses to infer modes when the extended status requires the original mode lookup table; it never claims an actual signal lock.

Eleven synthetic C tests in [`tests/test-intensity-input-status.c`](../tests/test-intensity-input-status.c) passed with strict compiler warnings on **both the Sierra Mac mini and Ubuntu**. These exercise gated paths, unreadable status, no-source flag, simple mode indices, extended timing families, and reserved status. Tests only establish fidelity to the documented interpretation, not hardware functionality.

The private status vtable verifier and existing model-dispatch parser have **ten passing synthetic Python tests together** on Ubuntu. They also passed a real hash-pinned check against the authorized private binary; vendor executable bytes are not included in Git.

## Remaining preconditions for a live register experiment

1. Establish a **verified, bootable and independently recoverable** alternate Sierra environment. Live audit of 24 October found no configured Time Machine destination, no external USB storage, and no separate boot-tested system. `Fresh` contains user data and is not an available disposable test volume.
2. Independently confirm a physically connected, unprotected and compatible HDMI source. Existing vendor-API runs received only frames flagged `no-source`.
3. Design a bounded, recoverable read-only experiment under exclusive PCI device ownership, including loss-of-signal and bus-error handling. Do not let two drivers compete for the same PCI device. Do not flash firmware, write unknown BAR registers, or enable DMA on the present boot.
4. Interpret a real status word against the observed firmware readiness and physical source state; only then expand from the pure classifier to hardware capture.

**Explicitly not accomplished:** firmware-ready runtime observation, live BAR0+`0x40` access, independent PCI driver load, DMA, HDMI lock, or a physical frame capture.
