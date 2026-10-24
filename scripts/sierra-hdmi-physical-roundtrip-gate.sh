#!/bin/sh
# Canonical staged acceptance: original fixed silver HDMI capture THEN output.
# No substitute camera / Mac, no concurrent full duplex, no source recabling.
# Old Desktop Video 10.4 input must report two consecutive genuine 'visl'
# locks before starting ANY capture/record/replay; otherwise fail CLOSED.
#
#   --prepare: build exact accepted input no-stream and roundtrip
#   --check: check unchanged Sierra, Macmini5,3, BMD display/driver
#   --run: 30-frame actual capture -> 60-frame vendor replay ->
#          independent private raw bytes/SHA verification; GUI restored.
set -eu
ROOT=${SILVER_VIDEO_GUEST_HOME:-/Volumes/Fresh/FountainVideoDockerLab}
LAB="$ROOT/private-roundtrip"
APP="$HOME/Applications/Fountain Video.app"
GUARD="$ROOT/bin/fc-mode-armed-original-1080p30"
SCRIPT="$ROOT/scripts/sierra-hdmi-roundtrip.sh"
VERIFY="$ROOT/tools/sierra/verify_hdmi_roundtrip_recording.py"
action=${1:---check}
case "$action" in --check|--prepare|--run) ;; *)
  echo "usage: sierra-hdmi-physical-roundtrip-gate.sh --prepare|--check|--run" >&2
  exit 64 ;;
esac
if [ "$(uname -s)" != Darwin ] ||
   [ "$(sw_vers -productVersion)" != 10.12.6 ] ||
   [ "$(sysctl -n hw.model)" != Macmini5,3 ]; then
   echo "stage=roundtrip_gate result=wrong_original_sierra_host" >&2; exit 20
fi
kextstat | grep -F "com.blackmagic-design.desktopvideo.iokit.driver (10.4)" >/dev/null || {
  echo "stage=roundtrip_gate result=original_driver_missing" >&2; exit 21;
}
system_profiler SPDisplaysDataType | grep -A3 "BMD HDMI:" |
  grep -F "1920 x 1080 @ 30 Hz" >/dev/null || {
  echo "stage=roundtrip_gate result=wrong_hdmi_source" >&2; exit 22;
}
test -r "$ROOT/src/decklink-mode-armed-no-stream.cpp" || exit 23
test -r "$VERIFY" || exit 24
test -r "$SCRIPT" || exit 25
test -d "$APP" || exit 26
if [ "$action" = --prepare ]; then
  mkdir -p "$ROOT/bin"
  clang++ -std=c++11 -Wall -Wextra -Werror -O1 \
    "$ROOT/src/decklink-mode-armed-no-stream.cpp" \
    -framework CoreFoundation -o "$GUARD"
  chmod 700 "$GUARD"
  sh "$SCRIPT" --prepare
  python2.7 -m py_compile "$VERIFY"
  echo "stage=roundtrip_gate result=prepared original_driver_preserved=1"
  exit 0
fi
test -x "$GUARD" || { echo "stage=roundtrip_gate result=not_prepared"; exit 27; }
sh "$SCRIPT" --check
if [ "$action" = --check ]; then
  echo "stage=roundtrip_gate result=ready_only physical_lock_not_yet_verified=1"
  exit 0
fi
test -d "$LAB" || exit 28
mode=$(stat -f %Lp "$LAB")
case "$mode" in 700) ;; *) echo "stage=roundtrip_gate result=private_dir_required"; exit 29;; esac
umask 077
outputlog=$(mktemp "$LAB/roundtrip-output-XXXXXXXX")
locklog=$(mktemp "$LAB/input-lock-XXXXXXXX")
restore() {
  open -a "$APP" >/dev/null 2>&1 || :
}
trap restore EXIT HUP INT TERM
if pgrep -f "/Black Syphon.app/Contents/MacOS/" >/dev/null 2>&1 ||
   pgrep -f "/Blackmagic Media Express.app/Contents/MacOS/" >/dev/null 2>&1; then
  echo "stage=roundtrip_gate result=conflicting_vendor_client" >&2; exit 30
fi
osascript -e 'tell application id "coach.fountain.video" to quit' \
    >/dev/null 2>&1 || :
sleep 2
if pgrep -f "^$APP/Contents/MacOS/FountainVideo$" >/dev/null 2>&1; then
  echo "stage=roundtrip_gate result=native_gui_still_running" >&2; exit 31
fi
"$GUARD" --armed-1080p30-no-stream > "$locklog" 2>&1
cat "$locklog"
if ! awk '
  /stage=mode_armed_status/ {
    if ($0 ~ /lock=yes/) {n++;if(n>=2) accepted=1}
    else n=0
  }
  END {exit accepted?0:1}
' "$locklog"; then
  echo "stage=roundtrip_gate result=NO_INPUT_LOCK"
  echo "stage=roundtrip_gate capture=NOT_STARTED record=NOT_CREATED"
  echo "stage=roundtrip_gate playback=NOT_STARTED output_api=NOT_CLAIMED"
  exit 42
fi
echo "stage=roundtrip_gate input_lock=PASS starting_real_source_record_replay=1"
if ! sh "$SCRIPT" --run > "$outputlog" 2>&1; then
  grep -E "stage=(recording_capture|roundtrip|replay)" "$outputlog" || :
  echo "stage=roundtrip_gate result=CAPTURE_OR_OUTPUT_FAILED"
  exit 43
fi
grep -E "stage=(recording_capture|private_recording|replay|roundtrip)" "$outputlog"
recording=$(sed -n 's/^stage=roundtrip.*recording_folder=\([^ ]*\).*$/\1/p' "$outputlog" | tail -1)
case "$recording" in
  "$LAB"/recording-*) ;;
  *) echo "stage=roundtrip_gate result=INVALID_PRIVATE_RECORDING_PATH"; exit 44;;
esac
python2.7 "$VERIFY" "$recording" "$outputlog"
echo "stage=roundtrip_gate result=DIGITAL_ACCEPTED_ONLY"
echo "stage=roundtrip_gate monitor_wire_or_optical_pixels=NOT_MEASURED"
