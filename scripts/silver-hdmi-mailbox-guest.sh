#!/bin/sh
# Silver Sierra -> xhyve Alpine/Linux Docker with a SECOND, read-only
# high-bandwidth virtual block mailbox. The original guest ext4 disk remains
# on slot 4; the real Blackmagic PCI/Thunderbolt remains owned by Sierra.
# NO Ubuntu boot, root vmnet, operating system install, or physical disk I/O.
set -eu
ROOT=${SILVER_VIDEO_GUEST_HOME:-/Volumes/Fresh/FountainVideoDockerLab}
mailbox="$ROOT/fountain-hdmi-mailbox.raw"
case "${1:---check}" in
  --check|--prepare|--run) ;;
  *) echo "usage: $0 --check|--prepare|--run" >&2; exit 64 ;;
esac
if [ "$(uname -s)" != Darwin ] ||
   [ "$(sw_vers -productVersion)" != 10.12.6 ] ||
   [ "$(sysctl -n hw.model)" != Macmini5,3 ]; then
  echo "stage=hdmi_mailbox_guest result=wrong_host" >&2; exit 20
fi
for name in xhyve vmlinuz-virt initramfs-virt alpine-virt-3.24.3-x86_64.iso docker-guest.raw; do
  test -f "$ROOT/$name" || { echo "stage=hdmi_mailbox_guest result=missing_$name" >&2; exit 21; }
done
test -x "$ROOT/xhyve" || exit 22
if [ "${1:---check}" = --prepare ]; then
  if [ -e "$mailbox" ]; then
    echo "stage=hdmi_mailbox_guest result=refuse_overwrite_mailbox" >&2; exit 24
  fi
  python2.7 -c 'import os,sys; sys.path.insert(0,sys.argv[1]); from simulator.hdmi_frame_mailbox import initialize; initialize(sys.argv[2]); print("stage=hdmi_mailbox_guest result=mailbox_created_only bytes=%d"%os.stat(sys.argv[2]).st_size)' \
    "$ROOT" "$mailbox"
  exit
fi
test -f "$mailbox" || { echo "stage=hdmi_mailbox_guest result=missing_mailbox" >&2; exit 25; }
test "$(stat -f %z "$mailbox")" = 4194304 || { echo "stage=hdmi_mailbox_guest result=unexpected_mailbox_size" >&2; exit 26; }
case "$(stat -f %Lp "$mailbox")" in 600) ;; *) echo "stage=hdmi_mailbox_guest result=not_private" >&2; exit 27;; esac
if [ "${1:---check}" = --check ]; then
  echo "stage=hdmi_mailbox_guest result=ready real_host=Sierra_10.12.6 linux=Alpine_Docker private_mailbox=4MiB guest_device=readonly"
  exit
fi
if pgrep -x xhyve >/dev/null 2>&1; then
  echo "stage=hdmi_mailbox_guest result=guest_already_running NO_DUPLICATE" >&2; exit 28
fi
echo "stage=hdmi_mailbox_guest result=starting slot4=existing_guest_ext4 slot5=private_hdmi_mailbox_readonly vmnet=none"
echo "Login root; disable ttyS1 getty; mount /dev/vda; start Docker. /dev/vdb is RAW and MUST NEVER be mounted as a filesystem."
exec "$ROOT/xhyve" -A -c 2 -m 2G \
  -s 0:0,hostbridge \
  -s "3,ahci-cd,$ROOT/alpine-virt-3.24.3-x86_64.iso" \
  -s "4,virtio-blk,$ROOT/docker-guest.raw" \
  -s "5,virtio-blk,$mailbox,ro,sync" \
  -s 31,lpc -l com1,stdio -l com2,autopty \
  -f "kexec,$ROOT/vmlinuz-virt,$ROOT/initramfs-virt,console=ttyS0,115200 modules=loop,squashfs,sd-mod,ahci alpine_dev=cdrom"
