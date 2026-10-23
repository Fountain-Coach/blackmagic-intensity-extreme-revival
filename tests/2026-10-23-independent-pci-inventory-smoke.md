# Read-only PCI inventory probe smoke test — 23 October 2026

**Source:** [`src/intensity-pci-inventory.c`](../src/intensity-pci-inventory.c)  
**Host:** macOS 26.5.1 development Mac (not the legacy 2011 Intensity Extreme host).  
**Purpose:** Validate an independently authored PCI enumeration probe without any Blackmagic runtime or hardware changes.

```sh
clang -std=c11 -Wall -Wextra -Werror src/intensity-pci-inventory.c \
  -framework IOKit -framework CoreFoundation \
  -o /tmp/intensity-pci-inventory
/tmp/intensity-pci-inventory
```

Observed output:

```text
stage=inventory result=not_found scanned=5 matched=0
```

Process exit: `2`, explicitly defined as a missing target device; the program compiled with `-Werror` and enumerated I/O Kit PCI entries without error.

**This is not a repair acceptance test.** The actual Intensity Extreme was not connected to this development Mac. Testing on the historical Sierra host is still required before asserting that it can enumerate the device. The program performs only registry property reads; it does not attach a replacement kernel driver, register a CoreMediaIO device, address PCI registers, load firmware or capture media.

A separate connection check found the historical test host's SSH port reachable, but automated login could not authenticate. No host test was performed.
