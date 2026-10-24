#!/bin/sh
# Final fixed-rig optical HDMI roundtrip test (original hardware only).
# Silver Mac mini Sierra HDMI OUT -> original Intensity Extreme HDMI IN.
# Original Intensity Extreme HDMI OUT -> external reference monitor.
# Original Thunderbolt 1 returns to the SAME Sierra silver Mac.
#
# --check: verify exact 1080p30 UYVY output support; do not activate video.
# --prepare: compile local user-space roundtrip; no system installation.
# --run: capture 30 real frames and replay exact private recording via HDMI.
set -eu
ROOT=${SILVER_VIDEO_GUEST_HOME:-/Volumes/Fresh/FountainVideoDockerLab}
SOURCE="$ROOT/src/decklink-hdmi-record-replay.cpp"
BIN="$ROOT/bin/fc-decklink-hdmi-record-replay"
APP="$HOME/Applications/Fountain Video.app"
action=${1:---check}
case "$action" in --check|--prepare|--run) ;; *)
  echo 'usage: sierra-hdmi-roundtrip.sh --check|--prepare|--run' >&2; exit 64 ;;
esac
if [ "$(uname -s)" != Darwin ] ||
   [ "$(sw_vers -productVersion)" != 10.12.6 ] ||
   [ "$(sysctl -n hw.model)" != Macmini5,3 ]; then
  echo 'stage=hdmi_roundtrip result=wrong_silver_sierra_host' >&2; exit 20
fi
test -r "$SOURCE" || { echo 'roundtrip source not staged' >&2; exit 21; }
if [ "$action" = --prepare ]; then
  mkdir -p "$ROOT/bin"
  chmod 700 "$ROOT/bin"
  clang++ -std=c++11 -Wall -Wextra -Werror -O1 \
    "$SOURCE" -framework CoreFoundation -framework CoreGraphics -o "$BIN"
  chmod 700 "$BIN"
  echo 'stage=hdmi_roundtrip result=built user_space_only=1'
fi
test -x "$BIN" || { echo 'prepare the roundtrip binary first' >&2; exit 22; }
"$BIN" --probe-output-1080p30
if [ "$action" != --run ]; then
  echo 'stage=hdmi_roundtrip result=output_preflight_only monitor_optical_result=NOT_MEASURED'
  exit 0
fi
test -d "$APP" || { echo 'installed Fountain Video application missing' >&2; exit 23; }
for app in 'Black Syphon' 'Blackmagic Media Express'; do
  if pgrep -f "/$app.app/Contents/MacOS/" >/dev/null 2>&1; then
    echo "stage=hdmi_roundtrip result=conflicting_capture_client name=$app" >&2
    exit 24
  fi
done
restore() {
  open -a "$APP" >/dev/null 2>&1 || :
}
trap restore EXIT HUP INT TERM
osascript -e 'tell application id "coach.fountain.video" to quit' >/dev/null 2>&1 || :
sleep 2
if pgrep -f "^$APP/Contents/MacOS/FountainVideo$" >/dev/null 2>&1; then
  echo 'stage=hdmi_roundtrip result=gui_still_active no_concurrent_device_access=1' >&2
  exit 25
fi
echo 'stage=hdmi_roundtrip optical_monitor_notice=Blackmagic_HDMI_OUT_to_external_monitor'
echo 'stage=hdmi_roundtrip recording=30_full_1920x1080_UYVY_frames confidential=1'
perl -e 'alarm 26; exec @ARGV or die "exec failed"' -- \
   "$BIN" --record-replay-1080p30
echo 'stage=hdmi_roundtrip result=finished digital_match_independent_optical_witness_pending=1'
