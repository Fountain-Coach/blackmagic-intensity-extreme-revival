# D3 executable evidence — register dispatch and input-status interpretation

**Date:** 24 October 2026. No kernel extension installation or hardware memory access occurred.

## Real Sierra host, compile-only ABI evidence

Using the repository sources from a fresh `git pull` on `bene@Uwes-Mac-mini.local` (macOS 10.12.6, Xcode 9.2):

```sh
sh driver/sierra/passive/verify-abi-layout.sh
```

Observed:

```text
stage=compiler_abi result=pass pci_slot=291 pci_dispatch_offset=0x908 memory_map_slot=37 memory_map_dispatch_offset=0x118
note=compiler_header_abi_only_not_a_live_runtime_vtable
abi_exit=0
```

Clang's C++ vtable dump has a two-pointer prefix before the virtual address point. The absolute compiler indexes 291 and 37 therefore correspond to dispatch byte offsets `0x908` and `0x118`. The matching methods are `IOPCIDevice::mapDeviceMemoryWithRegister` and `IOMemoryMap::getVirtualAddress`.

## Independently authored pure status classifier

On the Sierra Mac mini:

```sh
clang -std=c11 -Wall -Wextra -Werror -pedantic \
  src/intensity-input-status.c tests/test-intensity-input-status.c \
  -o /tmp/fc-input-status-tests
/tmp/fc-input-status-tests
```

Observed:

```text
stage=input_status_classifier result=pass tests=11 hardware_access=0
test_exit=0
```

The same sources compiled with `cc -std=c11 -Wall -Wextra -Werror -pedantic` on the Ubuntu analysis host and produced exactly `result=pass tests=11 hardware_access=0` (exit 0).

## Private-binary verification, Ubuntu analysis host

```sh
python3 scripts/inspect_private_status_dispatch.py /path/to/authorized/DeckLink_Driver
python3 -m unittest discover -s tests -p 'test_private*dispatch.py' -v
```

Observed against the SHA-pinned original binary:

```text
status=static_vtable_match
class=HardwareAbstractionDeckLinkHDExtreme
read_slot=0x7a0
read_method=HDProPCIExpress::ReadRegisterEndian
write_slot=0x7a8
write_method=HDProPCIExpress::WriteRegisterEndian
hardware_access=false
runtime_class_verified=false

Ran 10 tests ... OK
dispatch_suite_exit=0
```

A first test run failed because the newly added test did not set up its sibling Python module path; that harness defect was corrected and the complete ten-test suite then passed. It did not affect the authenticated private-binary verification.

**All results are software/ABI evidence**; no code accessed a PCI device BAR or initiated video DMA. The original Blackmagic vendor drivers continue to own the hardware. See [A124 status-register evidence](../docs/a124-input-status-call-chain-2026-10-24.md).
