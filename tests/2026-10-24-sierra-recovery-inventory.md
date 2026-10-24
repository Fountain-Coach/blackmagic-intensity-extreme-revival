# Sierra test-boot inventory — 24 October 2026

This is a read-only inspection of the physical 2011 Intel Mac mini, macOS 10.12.6. No disks were modified.

- Active startup volume: internal `disk0s2`, HFS+ `Data`, with `Recovery HD` (`disk0s3`) present.
- Second internal disk: `disk1s2`, HFS+ `Fresh`, approximately 73.3 GB used, containing existing user files.
- Fresh is mounted but the conventional `SystemVersion.plist` and `System/Library/CoreServices/boot.efi` were absent on inspection: it is **not a validated alternative boot**.
- No external USB mass-storage device was visible in the inventory.
- `tmutil destinationinfo`: **No destinations configured**.
- The live machine reports System Integrity Protection enabled.

**Result:** There is currently no independently boot-tested and recoverable environment for loading a competing experimental PCI driver. Do not change the startup disk or the two existing data-bearing partitions as a shortcut. The presence of Recovery HD alone does not establish full recovery of the present installation or remote access.

**Follow-on:** keep live probes registry-only; test the independent driver through offline linking. Any future experimental boot requires a separate validated destination, a restoration plan usable without SSH, and a successful return to the present Sierra installation. The original Blackmagic drivers remain the control.
