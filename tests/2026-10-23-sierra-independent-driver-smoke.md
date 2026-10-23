# Sierra independent IOKit driver — native smoke test

Date: 2026-10-23. Hardware host: Intel Mac mini (2011), macOS 10.12.6 (build 16G2136), Xcode 9.2 using the MacOSX10.13 SDK. Blackmagic Intensity Extreme connected via THUNDERDOCK2 and recognized by the Thunderbolt bus.

## D1: source-owned, user-space PCI enumeration — PASS

Source: [`src/intensity-pci-inventory.c`](../src/intensity-pci-inventory.c), transferred to the hardware host and compiled there (not simulated, not dependent on DeckLinkAPI).

```text
stage=device result=found vendor=0xbdbd device=0xa124 class=IOPCIDevice
stage=inventory result=found scanned=40 matched=1
probe_exit=0
```

The program only read the macOS I/O Registry; it did not read hardware memory, acquire the device, modify configuration, or touch loaded drivers.

## D2: passive kernel-extension source/build — OFFLINE PASS, RUNTIME UNTESTED

Source tree: [`driver/sierra/passive/`](../driver/sierra/passive/). The components are a minimal `IOService` subclass, kernel-module metadata, configuration, and a build-only shell script. There is **no IOKit matching personality**. No PCI bus mastering, firmware upload, MMIO, DMA, interrupts or hardware-output implementation.

A fresh checkout of the repository on the Sierra hardware host (commit `226f246`) was built by running its committed `driver/sierra/passive/build-passive.sh` (no installs or privileged commands). The build produced:

```text
Info.plist: OK
Mach-O 64-bit kext bundle x86_64
stage=passive_bundle_built
note=not_installed_not_loaded
```

Offline-only validation command:

```sh
kextutil -n -t -z /path/to/temporary/FCIntensityPCIDiagnostic.kext
```

Observed:

```text
Code Signing Failure: not code signed
... appears to be loadable (including linkage for on-disk libraries).
offline_status=0
```

The unsigned status is expected for this noninstalled bundle, not a passing code-signing or live-loading test. On the first packaging attempt `kmod_info` was absent and the validator rejected it. After linking `libkmod` and declaring module metadata, a subsequent run identified a Sierra incompatibility in the newer PCI config call and a missing IOPCIFamily dependency. Both were fixed before the successful offline validation.

**Outstanding compatibility warning:** Xcode 9.2's `libkmod.a` object files identify 10.13 as their minimum OS while the target is 10.12. The linker emitted a warning; offline `kextutil` passes. This remains a runtime acceptance risk, not a claim of full Sierra compatibility.

## Safety/regression — PASS

- The two original Blackmagic Desktop Video 10.4 kernel components remained loaded after the tests.
- The independent `coach.fountain.intensity.PCIDiagnostic` driver **was not loaded** and did not register a competing PCI personality.
- No root install, firmware flash, kernel-driver replacement, video DMA test, or capture operation occurred.
- No physical HDMI frame has been captured by an independent driver.

Next: independently document the known PCI resources and hardware initialization protocol from evidence, maintain a tested rollback path, and only then plan a controlled nonproduction kernel-attachment experiment.

## Additional D3 compile-only resource gate — same day

The source-owned [passive driver class](../driver/sierra/passive/FCIntensityPCIDiagnostic.cpp) was extended to call **`IOPCIDevice::getDeviceMemoryWithRegister`** for BAR0 and BAR1, then validate both provider-owned descriptor lengths against the **1,048,576 bytes** measured by the independent live I/O Registry probe. It does not call `mapDeviceMemoryWithRegister` and does not read or write the BAR content.

A fresh Git checkout on the Sierra host successfully rebuilt the source with Xcode 9.2, `-Wall -Wextra -Werror` and the explicit legacy-SDK warning suppression. Offline validation of that exact compiled bundle again reported:

```text
Mach-O 64-bit kext bundle x86_64
Info.plist: OK
Code Signing Failure: not code signed
... appears to be loadable (including linkage for on-disk libraries).
offline_validation_exit=0
own_driver_not_loaded
```

The original Blackmagic 10.4 kernel drivers were still loaded after this check. **This verifies source compatibility and offline linkage only**, not runtime access to the descriptors. The existing Xcode `libkmod` 10.13-vs-10.12 minimum-version warning remains unresolved.
