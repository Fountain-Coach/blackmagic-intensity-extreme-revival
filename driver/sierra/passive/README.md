# Sierra IOKit passive PCI diagnostics — source-owned

**Status:** compiled and structurally link-checked from Git on the original macOS Sierra 10.12.6 hardware host. This is **not** a loaded, signed, or functional capture driver.

The `FCIntensityPCIDiagnostic` I/O Kit `IOService` implementation checks a matching `IOPCIDevice` for PCI ID `0xBDBD:0xA124` using Sierra's `configRead16(UInt8)`. It then obtains the **provider-owned** resource descriptors for BAR0 (`0x10`) and BAR1 (`0x14`) through `getDeviceMemoryWithRegister` and fails closed unless both report the independently measured 1 MiB size. It does **not** call `mapDeviceMemoryWithRegister`, access mapped device registers, enable bus mastering, allocate DMA buffers, acknowledge interrupts, flash firmware or write EEPROM.

The Info.plist deliberately has an **empty `IOKitPersonalities` dictionary**, preventing automatic hardware matching. It must not be installed over Blackmagic Desktop Video 10.4 on the normal boot.

On the Sierra hardware host:

```sh
sh driver/sierra/passive/build-passive.sh
# Apply only to the resulting temporary bundle:
kextutil -n -t -z /path/to/temporary/FCIntensityPCIDiagnostic.kext
```

The last command is **offline validation only**. `-z` skips authentication for that diagnostic; it does not waive signing rules for installation. In the verified run, `kextutil` reported the bundle's symbols and dependencies appeared linkable and returned zero, while also reporting **unsigned**. Xcode 9.2's `libkmod` objects still warn that they were built for 10.13 rather than the 10.12 target. Runtime compatibility is not established.

The original Blackmagic kernel drivers remained loaded. No independent kernel-driver binding, BAR memory access or captured physical frame is claimed. See [the A124-specific static dispatch evidence](../../../docs/a124-hdextreme-dispatch-2026-10-23.md), [physical PCI resource inventory](../../../tests/2026-10-23-pci-resource-layout.md) and [independent-driver evidence](../../../tests/2026-10-23-sierra-independent-driver-smoke.md).

**Acceptance for further deployment:** a recoverable separate boot, verified code-signing/build provenance and a model-specific register/DMA protocol. Until then, this driver remains a deliberately inactive source and link artifact.
