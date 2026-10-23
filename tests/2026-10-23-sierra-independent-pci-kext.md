# 2026-10-23 independent Sierra PCI / passive KEXT milestone

All observations below were made on the **actual 2011 Intel Mac mini running macOS 10.12.6** with the original Thunderbolt Intensity Extreme attached through the Comdis THUNDERDOCK2. The system's existing Blackmagic Desktop Video 10.4 kernel drivers remained loaded and unchanged throughout.

## D1: direct IOKit inventory: PASS

The source-owned `src/intensity-pci-inventory.c` was transferred to the Sierra host and compiled locally against Apple IOKit/CoreFoundation:

```sh
clang -std=c11 -Wall -Wextra -Werror \
  src/intensity-pci-inventory.c -framework IOKit -framework CoreFoundation \
  -o /tmp/fc-intensity-pci-inventory
/tmp/fc-intensity-pci-inventory
```

Actual output:

```text
stage=device result=found vendor=0xbdbd device=0xa124 class=IOPCIDevice
stage=inventory result=found scanned=40 matched=1
probe_exit=0
```

No Blackmagic application framework was required. No physical video frames were captured.

## D2: passive kernel driver build and offline validation: PARTIAL PASS

On the same Sierra host using Xcode 9.2 (Apple LLVM 9.0, macOS 10.13 SDK), an independently authored IOKit `IOService` subclass was compiled and linked as an x86_64 Mach-O KEXT bundle. It reads PCI vendor/device configuration IDs only. The test bundle has an **empty `IOKitPersonalities` dictionary** and cannot automatically match hardware. It was assembled under the account's temporary directory **without installation or load**.

Early offline checks exposed and resolved three issues:
- missing `kmod_info` metadata, then added via the Sierra `libkmod` startup/shutdown linkage;
- an SDK-selected `extendedConfigRead16` symbol not exported by the live Sierra PCI stack; replaced with the legacy `configRead16(UInt8)` method;
- missing `com.apple.iokit.IOPCIFamily` dependency, added at required 2.9.

Final offline output (`kextutil -n -t -z`):

```text
Code Signing Failure: not code signed
... appears to be loadable (including linkage for on-disk libraries).
offline_structure_exit=0
```

The bundle remains unsigned and uninstalled. `-z` intentionally skips authentication during **offline** diagnostics; it does not authorize or demonstrate runtime loading. The linker warned that `libkmod.a` objects in Xcode's 10.13 SDK are newer than the 10.12 deployment target. No runtime compatibility claim is made.

The existing Blackmagic kernel drivers 10.4 were still present in `kextstat`, and the independent driver's bundle ID was **not** loaded.

**Next gate:** establish a recoverable, explicitly isolated boot/test system and the minimal hardware protocol before any independent driver is allowed to match/control PCI resources. A complete driver, physical HDMI lock and real frame capture remain unproven. Do not replace a working production driver or re-enable the crashing vendor CoreMediaIO plug-in as a shortcut.
