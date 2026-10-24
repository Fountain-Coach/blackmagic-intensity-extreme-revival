#!/bin/sh
# Source-owned bounded physical HDMI witness. Run on the source Mac.
# A BMD HDMI endpoint and exact supported source mode are mandatory.
# Video is never stored; only bounded callback, status and timing evidence.
set -eu
mode="${1:-1080p30}"
case "$mode" in
  1080p25|1080p30|720p60) ;;
  *) echo "usage: $0 [1080p25|1080p30|720p60]" >&2; exit 64 ;;
esac

here=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
target="bene@Uwes-Mac-mini.local"
source_file="$here/src/decklink-input-observer.cpp"
[ -r "$source_file" ] || { echo "stage=runner result=missing_source"; exit 20; }
command -v python3 >/dev/null || { echo "stage=runner result=missing_python3"; exit 21; }

echo "stage=runner phase=source_preflight mode=$mode"
if python3 "$here/scripts/hdmi-source-preflight.py" --mode "$mode"; then
  :
else
  status=$?
  echo "stage=runner result=blocked_by_source_preflight status=$status"
  exit "$status"
fi

# EDID recognition is not proof of signal or HDCP-free capture.
remote_dir=""
cleanup() {
  if [ -n "$remote_dir" ]; then
    ssh -o BatchMode=yes -o ConnectTimeout=5 "$target" \
      "rm -rf '$remote_dir'" >/dev/null 2>&1 || :
  fi
}
trap cleanup 0
trap 'exit 130' 2
trap 'exit 143' 15

remote_dir=$(ssh -o BatchMode=yes -o ConnectTimeout=5 "$target" \
  'mktemp -d /tmp/fc-intensity-capture.XXXXXX')
case "$remote_dir" in
  /tmp/fc-intensity-capture.*) ;;
  *) echo "stage=runner result=invalid_remote_temp" >&2; exit 22 ;;
esac
printf '%s\n' "$remote_dir" |
  grep -Eq '^/tmp/fc-intensity-capture\.[A-Za-z0-9]{6}$' ||
  { echo "stage=runner result=unsafe_remote_temp" >&2; exit 22; }

scp -q "$source_file" "$target:$remote_dir/observer.cpp"
echo "stage=runner phase=bounded_capture mode=$mode"
if captured=$(ssh -o BatchMode=yes -o ConnectTimeout=5 "$target" \
    "clang++ -std=c++11 -Wall -Wextra -Werror -Wno-unused-parameter '$remote_dir/observer.cpp' -framework CoreFoundation -o '$remote_dir/observer' && '$remote_dir/observer' '--observe-$mode'"); then
  printf '%s\n' "$captured"
else
  status=$?
  printf '%s\n' "$captured"
  echo "stage=runner result=observer_failed status=$status" >&2
  exit 23
fi

# Require physical receiver lock, pixel buffer, frame timestamp and teardown.
printf '%s\n' "$captured" |
  python3 "$here/scripts/validate-capture-evidence.py" --mode "$mode"
