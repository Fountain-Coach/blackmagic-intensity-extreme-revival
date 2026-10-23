# Independent Sierra hardware boundary — live acceptance and offline driver package
**Date:** 2026-10-23
**Host:** owner-authorized `Uwes-Mac-mini`, Macmini5,3 / Intel x86-64, macOS 10.12.6 (16G2136).
**Connection:** existing authenticated `bene@Uwes-Mac-mini.local`; the host was reachable. Earlier attempted login under a different username was not evidence of lost access.

## D1 — independent user-space PCI discovery: PASSED

Built and executed [`src/intensity-pci-inventory.c`](../src/intensity-pci-inventory.c) on the actual Sierra host, using Apple's IOKit and CoreFoundation only, *not* the DeckLink API.

```text
stage=device result=found vendor=0xbdbd device=0xa124 class=IOPCIDevice
stage=inventory result=found scanned=40 matched=1
probe_exit=0
```

Build: `clang -std=c11 -Wall -Wextra -Werror ... -framework IOKit -framework CoreFoundation`.
Result: exactly one matching PCI device. No PCI writes, firmware changes, DMA or kernel modifications.

The physical Thunderbolt topology is still **Mac mini → THUNDERDOCK2 → Intensity Extreme**. Blackmagic Desktop Video **10.4** original kernel components remained loaded before and after the work.

## D2 — passive kernel extension, source-owned: OFFLINE LINK PASSED

The independent [driver source](../driver/sierra/passive/FCIntensityPCIDiagnostic.cpp) is an `IOService` class that can recognize the PCI vendor/device through Sierra's older `IOPCIDevice::configRead16(UInt8)` API. Its `Info.plist` deliberately includes an **empty `IOKitPersonalities` dictionary**, so the bundle does not advertise a match against real hardware. The build recipe creates a temporary artifact only.

Using Sierra host Xcode 9.2, Apple LLVM 9.0.0, macOS 10.13 SDK targeting 10.12, the source compiled with `-Wall -Wextra -Werror -Wno-inconsistent-missing-override`; the `kext` linked as a **Mach-O 64-bit x86_64 kernel-extension bundle**. The explicit `kmod_info` declaration was necessary for the legacy loader. The `OSBundleLibraries` also needed `com.apple.iokit.IOPCIFamily` 2.9, and the obsolete 8-bit PCI register access signature avoided an unresolved 10.13-only `extendedConfigRead16` symbol.

Final offline validation:

```text
Info.plist: OK
Mach-O 64-bit kext bundle x86_64
kextutil -n -t -z: Code Signing Failure: not code signed
appears to be loadable (including linkage for on-disk libraries)
offline_structure_exit=0
```

Note that the `-z` flag bypasses authentication **for offline diagnostics only**; this is *not* an installation permission, an instruction to disable system security, or evidence of a runtime load. The bundle is unsigned, and the system rightly flags it. Xcode's `libkmod.a` start/stop objects generated a **10.13 > 10.12 deployment warning**; runtime safety must not be inferred from the successful offline link. No installation or loading was attempted.

## Unchanged environment

```text
com.blackmagic-design.desktopvideo.iokit.framebufferdriver (10.4) [loaded]
com.blackmagic-design.desktopvideo.iokit.driver (10.4) [loaded]
coach.fountain.intensity.PCIDiagnostic [not loaded]
```

No device driver was displaced; no kernel extension or CoreMediaIO plug-in was installed. The previously isolated faulty vendor DAL plug-in was not restored. Source and temporary binaries are separate.

## Remaining blockers, in order

1. **Hardware protocol:** The new driver does not yet initialize firmware, handle interrupts, access device memory, allocate DMA or capture a frame. Reconstruct only a testable and documented subset of the hardware protocol from authorized local evidence.
2. **Reproducible isolated test host:** Establish a recoverable alternative boot/setup before ever enabling a matching personality or disabling vendor driver ownership.
3. **Build provenance:** Resolve the 10.13-derived `libkmod` warning and real code-signing/install requirements before considering runtime loading.
4. **Physical capture:** Independently lock a known *unprotected* external HDMI source and validate a timestamped real input frame. The prior native vendor-API capture reported no signal lock; this remains unresolved.

**Acceptance statement:** D1 and the build/link portion of D2 passed. D2 runtime binding and D3–D6 are *not* passed. Neither App Store stability nor virtual frames constitute capture repair.
