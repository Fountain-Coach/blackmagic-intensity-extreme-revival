#!/bin/sh
# Silver Sierra controlled-restart preflight; preserve the offline Linux/Docker
# virtual disk and private 17-frame mailbox. Never overwrite existing backups.
# Only invoke on the original Macmini5,3 Sierra 10.12.6 after a user-authorized
# reboot request; this script DOES NOT reboot or terminate the guest.
set -eu
ROOT=/Volumes/Fresh/FountainVideoDockerLab
BACKUP="$ROOT/private-restart-20261024"
DISK="$ROOT/docker-guest.raw"
MAILBOX="$ROOT/fountain-hdmi-mailbox.raw"
umask 077
if [ "$(uname -s)" != Darwin ] ||
   [ "$(sw_vers -productVersion)" != 10.12.6 ] ||
   [ "$(sysctl -n hw.model)" != Macmini5,3 ]; then
  echo "stage=sierra_restart_guard result=WRONG_HOST"; exit 20
fi
test -f "$DISK" && test -f "$MAILBOX" || {
  echo "stage=sierra_restart_guard result=ORIGINAL_GUEST_FILES_MISSING"; exit 21;
}
test "$(stat -f %z "$DISK")" = 12884901888 || {
  echo "stage=sierra_restart_guard result=UNEXPECTED_GUEST_DISK_SIZE"; exit 22;
}
test "$(stat -f %z "$MAILBOX")" = 4194304 || {
  echo "stage=sierra_restart_guard result=UNEXPECTED_MAILBOX_SIZE"; exit 23;
}
test "$(stat -f %Lp "$DISK")" = 600 && test "$(stat -f %Lp "$MAILBOX")" = 600 || {
  echo "stage=sierra_restart_guard result=ORIGINAL_FILES_NOT_PRIVATE"; exit 24;
}
pid="$(pgrep -x xhyve || true)"
case "$pid" in
  ''|*[!0-9]*) echo "stage=sierra_restart_guard result=GUEST_PID_NOT_UNIQUE";exit 25 ;;
esac
ps -p "$pid" -o command= | grep -F "$DISK" >/dev/null || {
  echo "stage=sierra_restart_guard result=WRONG_XHYVE_PROCESS"; exit 26;
}
if [ -e "$BACKUP" ]; then
  echo "stage=sierra_restart_guard result=BACKUP_ALREADY_EXISTS refuse_overwrite=1"
  exit 27
fi
mkdir -m 700 "$BACKUP" || exit 28
printf "date=%s\n" "$(date)" > "$BACKUP/manifest.txt"
printf "xhyve_pid=%s\n" "$pid" >> "$BACKUP/manifest.txt"
printf "source_guest_disk_bytes=12884901888\nsource_mailbox_bytes=4194304\n" >> "$BACKUP/manifest.txt"
echo "stage=sierra_restart_guard result=BACKUP_START private=1 guest_disk_bytes=12884901888"
cp -p "$MAILBOX" "$BACKUP/fountain-hdmi-mailbox.raw" || exit 29
shasum -a 256 "$MAILBOX" "$BACKUP/fountain-hdmi-mailbox.raw" >> "$BACKUP/manifest.txt" || exit 30
paused=0
resume() {
  if [ "$paused" -eq 1 ]; then
    kill -CONT "$pid" 2>/dev/null || :
    paused=0
    echo "stage=sierra_restart_guard guest=RESUMED"
  fi
}
trap resume 0 1 2 3 15
kill -STOP "$pid" || exit 31
paused=1
echo "stage=sierra_restart_guard guest=PAUSED copying_guest_disk=1"
cp -p "$DISK" "$BACKUP/docker-guest.raw" || exit 32
if ! cmp -s "$DISK" "$BACKUP/docker-guest.raw"; then
  echo "stage=sierra_restart_guard result=BACKUP_MISMATCH"; exit 33
fi
printf "guest_disk_copy=BYTE_IDENTICAL guest_frozen_during_copy=1\n" >> "$BACKUP/manifest.txt"
sync
resume
trap - 0 1 2 3 15
echo "stage=sierra_restart_guard result=BACKUP_ACCEPTED guest_resumed=1 original_disk_unchanged=1"
