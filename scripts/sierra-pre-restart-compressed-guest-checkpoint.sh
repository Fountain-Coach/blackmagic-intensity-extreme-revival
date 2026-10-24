#!/bin/sh
# One-time guarded compressed guest-disk checkpoint before restarting the
# original Sierra Macmini5,3. Does NOT restart or kill the original guest.
# The bounded hardware fixture and all original guest disks remain untouched.
set -eu
ROOT=/Volumes/Fresh/FountainVideoDockerLab
BACKUP="$ROOT/private-restart-20261024"
DISK="$ROOT/docker-guest.raw"
ARCHIVE="$BACKUP/docker-guest.raw.gz"
PARTIAL="$BACKUP/docker-guest.raw"
umask 077
if [ "$(uname -s)" != Darwin ] ||
   [ "$(sw_vers -productVersion)" != 10.12.6 ] ||
   [ "$(sysctl -n hw.model)" != Macmini5,3 ]; then
  echo "stage=compressed_guest_checkpoint result=WRONG_HOST"; exit 20
fi
test -d "$BACKUP" && test "$(stat -f %Lp "$BACKUP")" = 700 || exit 21
test -f "$DISK" && test "$(stat -f %z "$DISK")" = 12884901888 || exit 22
test -f "$BACKUP/fountain-hdmi-mailbox.raw" || exit 23
test ! -e "$ARCHIVE" || { echo "stage=compressed_guest_checkpoint result=ARCHIVE_EXISTS"; exit 24; }
pid=$(pgrep -x xhyve) || exit 25
case "$pid" in *[!0-9]*|'') echo "stage=compressed_guest_checkpoint result=INVALID_PID"; exit 26;; esac
ps -p "$pid" -o command= | grep -F "$DISK" >/dev/null || exit 27
# An earlier complete 12 GiB copy was cancelled after a partial write.
# Remove only that known incomplete derived artifact, never original disk.
if [ -e "$PARTIAL" ]; then
  [ "$(stat -f %z "$PARTIAL")" -lt 12884901888 ] || {
    echo "stage=compressed_guest_checkpoint result=REFUSE_DELETE_COMPLETE_COPY"; exit 28;
  }
  rm "$PARTIAL" || exit 29
fi
paused=0
resume() {
  if [ "$paused" -eq 1 ]; then
    kill -CONT "$pid" || :
    paused=0
    echo "stage=compressed_guest_checkpoint guest=RESUMED"
  fi
}
trap resume 0 1 2 3 15
kill -STOP "$pid" || exit 30
paused=1
echo "stage=compressed_guest_checkpoint guest=PAUSED creating_compressed_disk_backup=1"
gzip -1 -c "$DISK" > "$ARCHIVE" || exit 31
gzip -t "$ARCHIVE" || exit 32
original=$(shasum -a 256 "$DISK" | awk '{print $1}') || exit 33
copied=$(gzip -dc "$ARCHIVE" | shasum -a 256 | awk '{print $1}') || exit 34
if [ "$original" != "$copied" ]; then
  echo "stage=compressed_guest_checkpoint result=CHECKSUM_MISMATCH"; exit 35
fi
{
  printf "compressed_guest_disk_sha256=%s\n" "$original"
  printf "compressed_file_bytes=%s\n" "$(stat -f %z "$ARCHIVE")"
  printf "source_guest_disk_bytes=%s\n" "$(stat -f %z "$DISK")"
  printf "guest_paused_during_archive_and_hash=1\n"
} >> "$BACKUP/manifest.txt"
sync
resume
trap - 0 1 2 3 15
echo "stage=compressed_guest_checkpoint result=ACCEPTED digest=$original guest_resumed=1 original_files_unchanged=1"
