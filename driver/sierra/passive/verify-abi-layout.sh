#!/bin/sh
# Build-only compiler ABI evidence for the x86-64 Sierra device.
# Does not install code, load drivers, or open hardware.
set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
sdk=$(xcrun --sdk macosx --show-sdk-path)
headers="$sdk/System/Library/Frameworks/Kernel.framework/Headers"
scratch=$(mktemp -d "${TMPDIR:-/tmp}/fc-intensity-abi.XXXXXX")
trap 'rm -rf "$scratch"' 0
trap 'exit 1' 1 2 3 15

clang++ -std=c++11 -arch x86_64 -mmacosx-version-min=10.12 \
  -fno-exceptions -fno-rtti -DKERNEL -D__KERNEL__ \
  -Wall -Wextra -Werror -Wno-inconsistent-missing-override \
  -I "$headers" -Xclang -fdump-vtable-layouts \
  -c "$here/abi-layout-probe.cpp" -o "$scratch/abi-layout-probe.o" \
  > "$scratch/abi-layout.out"

pci_slot=$(grep 'IOPCIDevice::mapDeviceMemoryWithRegister(UInt8, IOOptionBits)' \
  "$scratch/abi-layout.out" | awk '{ print $1 }')
map_slot=$(grep 'IOMemoryMap::getVirtualAddress()' \
  "$scratch/abi-layout.out" | awk '{ print $1 }')
[ "$pci_slot" = "291" ] || {
  echo "stage=compiler_abi result=fail pci_slot=$pci_slot" >&2
  exit 20
}
[ "$map_slot" = "37" ] || {
  echo "stage=compiler_abi result=fail map_slot=$map_slot" >&2
  exit 21
}
printf 'stage=compiler_abi result=pass pci_slot=%s pci_dispatch_offset=0x908 memory_map_slot=%s memory_map_dispatch_offset=0x118\n' "$pci_slot" "$map_slot"
printf 'note=compiler_header_abi_only_not_a_live_runtime_vtable\n'
