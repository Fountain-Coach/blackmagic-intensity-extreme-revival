#!/bin/sh
# Sierra stays the host OS. This starts an isolated Linux/Docker VM using ONLY
# the user-created guest disk file on Fresh. Never attaches a physical disk.
set -eu
ROOT=${SILVER_VIDEO_GUEST_HOME:-/Volumes/Fresh/FountainVideoDockerLab}
case "${1:---check}" in --check|--run) ;; *) echo "usage: $0 --check|--run" >&2; exit 64;; esac
if [ "$(uname -s)" != "Darwin" ] || [ "$(sw_vers -productVersion)" != "10.12.6" ] ||
   [ "$(sysctl -n hw.model)" != "Macmini5,3" ]; then
  echo "stage=silver_video_guest result=wrong_host expected=Macmini5,3_Sierra_10.12.6" >&2
  exit 20
fi
for name in xhyve vmlinuz-virt initramfs-virt alpine-virt-3.24.3-x86_64.iso docker-guest.raw; do
  if [ ! -f "$ROOT/$name" ]; then
    echo "stage=silver_video_guest result=missing_file name=$name" >&2
    exit 21
  fi
done
if [ ! -x "$ROOT/xhyve" ]; then
  echo "stage=silver_video_guest result=hypervisor_not_executable" >&2
  exit 22
fi
if [ "$1" = "--check" ]; then
  echo "stage=silver_video_guest result=ready host=Sierra guest=Alpine_3.24.3 linux_docker_disk=present network=disabled physical_capture=NOT_ACCEPTED"
  exit 0
fi
# Never open a second VM against the same writable disk image.
if pgrep -x xhyve >/dev/null 2>&1; then
  echo "stage=silver_video_guest result=existing_xhyve do_not_double_mount_disk=1" >&2
  exit 23
fi
echo "stage=silver_video_guest result=starting network=disabled physical_pci_passthrough=none"
echo "Guest console: log in as root (offline lab), mount /dev/vda, then start the Docker daemon."
exec "$ROOT/xhyve" -A -c 2 -m 2G \
  -s 0:0,hostbridge \
  -s "3,ahci-cd,$ROOT/alpine-virt-3.24.3-x86_64.iso" \
  -s "4,virtio-blk,$ROOT/docker-guest.raw" \
  -s 31,lpc -l com1,stdio \
  -f "kexec,$ROOT/vmlinuz-virt,$ROOT/initramfs-virt,console=ttyS0,115200 modules=loop,squashfs,sd-mod,ahci alpine_dev=cdrom"
