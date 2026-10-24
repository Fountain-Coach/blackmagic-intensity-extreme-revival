#!/bin/sh
# A124 HDExtreme Linux hardware-interface preflight, without a real card.
# Verifies the *actual source-owned* C register models and the Ubuntu PCI
# attachment contract. No privileged commands, module changes, config/BAR
# reads, Thunderbolt authorization, DMA, firmware or HDMI I/O.
set -eu
if [ "$(uname -s)" != Linux ]; then
    echo "stage=a124_linux_port result=unsupported_os"
    exit 20
fi
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d /tmp/fc-a124-port.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cd "$root"

echo "stage=a124_linux_port step=pci_sysfs_and_replay_regressions"
python3 -m unittest discover -s tests -p test_a124_ubuntu_hardware_port.py -q

echo "stage=a124_linux_port step=c_register_firmware_gate"
cc -std=c11 -Wall -Wextra -Werror \
    src/intensity-firmware-gate.c \
    tests/test-intensity-firmware-gate.c -o "$tmp/firmware-gate"
"$tmp/firmware-gate"

echo "stage=a124_linux_port step=c_input_status_gate"
cc -std=c11 -Wall -Wextra -Werror \
    src/intensity-input-status.c \
    tests/test-intensity-input-status.c -o "$tmp/input-status"
"$tmp/input-status"

echo "stage=a124_linux_port step=c_injected_register_port"
cc -std=c11 -Wall -Wextra -Werror \
    src/intensity-register-contract.c \
    src/intensity-input-status.c \
    src/intensity-firmware-gate.c \
    tests/test-intensity-register-contract.c -o "$tmp/read-contract"
"$tmp/read-contract"

echo "stage=a124_linux_port step=real_kernel_inventory"
python3 -m hardware.ubuntu.a124_pci_port

echo "stage=a124_linux_port result=pass source_owned_hardware_model=1"
echo "stage=a124_linux_port hardware_frame_capture=NOT_TESTED device_access=0 authorization=0 dma=0 pci_register_writes=0"
