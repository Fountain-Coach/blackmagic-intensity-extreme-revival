#!/bin/sh
# Fixed-silver output-standard versus HDMI IN lock A/B/A differential.
# No new HDMI sources, recabling, Sierra/driver/firmware edits, root, stream
# callbacks, frame reads, output playback or preferences persistence.
set -u
ROOT=/Volumes/Fresh/FountainVideoDockerLab
if [ -n "$(printenv SILVER_VIDEO_GUEST_HOME)" ]; then ROOT="$(printenv SILVER_VIDEO_GUEST_HOME)"; fi
APP="$HOME/Applications/Fountain Video.app"
SRC="$ROOT/src/decklink-fixed-silver-output-default-input-differential.cpp"
CAPSRC="$ROOT/src/decklink-original-input-mux-capabilities-readonly.cpp"
BIN="$ROOT/bin/fc-fixed-silver-idle-input-differential"
CAPBIN="$ROOT/bin/fc-original-input-mux-caps"
LAB="$ROOT/private-roundtrip"
action=--check
if [ $# -gt 0 ]; then action=$1; fi
case "$action" in --prepare|--check|--run|--capabilities) ;; *) echo "usage: $0 --prepare|--check|--run" >&2; exit 64;; esac
if [ "$(uname -s)" != Darwin ] ||
   [ "$(sw_vers -productVersion)" != 10.12.6 ] ||
   [ "$(sysctl -n hw.model)" != Macmini5,3 ]; then
  echo "stage=idle_input_differential result=WRONG_SILVER" >&2; exit 20
fi
kextstat | grep -F 'com.blackmagic-design.desktopvideo.iokit.driver (10.4)' >/dev/null || {
  echo "stage=idle_input_differential result=ORIGINAL_DRIVER_MISSING" >&2; exit 21;
}
system_profiler SPDisplaysDataType | grep -A3 'BMD HDMI:' |
  grep -F '1920 x 1080 @ 30 Hz' >/dev/null || {
  echo "stage=idle_input_differential result=WRONG_FIXED_HDMI_SOURCE" >&2; exit 22;
}
test -d "$APP" || { echo "stage=idle_input_differential result=APP_MISSING"; exit 23; }
test -r "$SRC" || { echo "stage=idle_input_differential result=SOURCE_MISSING"; exit 24; }
test -r "$CAPSRC" || { echo "stage=idle_input_differential result=CAPABILITY_SOURCE_MISSING"; exit 24; }
if [ "$action" = --prepare ]; then
  mkdir -p "$ROOT/bin"
  clang++ -std=c++11 -Wall -Wextra -Werror -O1 \
    "$SRC" -framework CoreFoundation -framework CoreGraphics -o "$BIN" || exit 25
  clang++ -std=c++11 -Wall -Wextra -Werror -O1 \
    "$CAPSRC" -framework CoreFoundation -o "$CAPBIN" || exit 25
  chmod 700 "$BIN" "$CAPBIN"
  echo "stage=idle_input_differential result=PREPARED no_device_changes=1"
  exit 0
fi
test -x "$BIN" && test -x "$CAPBIN" || { echo "stage=idle_input_differential result=NOT_PREPARED"; exit 26; }
echo "stage=idle_input_differential result=CHECKED no_device_changes=1"
[ "$action" = --check ] && exit 0
if pgrep -f '/Black Syphon.app/Contents/MacOS/' >/dev/null 2>&1 ||
   pgrep -f '/Blackmagic Media Express.app/Contents/MacOS/' >/dev/null 2>&1; then
  echo "stage=idle_input_differential result=CONFLICTING_CLIENT" >&2; exit 27
fi
GUIEXE="$APP/Contents/MacOS/FountainVideo"
if ! pgrep -f "^$GUIEXE$" >/dev/null 2>&1; then
  echo "stage=idle_input_differential result=GUI_NOT_RUNNING changed=0" >&2; exit 28
fi
umask 077
test -d "$LAB" || { echo "stage=idle_input_differential result=LAB_MISSING"; exit 29; }
test "$(stat -f %Lp "$LAB")" = 700 || {
  echo "stage=idle_input_differential result=PRIVATE_LAB_REQUIRED"; exit 30;
}
LOG=$(mktemp "$LAB/idle-input-aba-XXXXXXXX") || exit 31
restore() {
  result=$?
  trap - 0 1 2 3 15
  echo "stage=idle_input_differential action=RESTORE_GUI differential_exit=$result"
  if [ -x /tmp/fc-output-state-readonly ]; then
    /tmp/fc-output-state-readonly --read-only | grep -E 'key=default_output_mode|key=idle_output_operation' || :
  fi
  open -a "$APP" >/dev/null 2>&1 || :
  sleep 2
  if pgrep -f "^$GUIEXE$" >/dev/null 2>&1; then
    echo "stage=idle_input_differential gui=RESTORED"
  else
    echo "stage=idle_input_differential gui=RESTORE_NOT_VERIFIED" >&2
    [ "$result" -ne 0 ] || result=32
  fi
  if [ -x /tmp/fc-output-state-readonly ]; then
    /tmp/fc-output-state-readonly --read-only | grep -E 'key=default_output_mode|key=idle_output_operation' || :
  fi
  exit "$result"
}
trap restore 0 1 2 3 15
osascript -e 'tell application id "coach.fountain.video" to quit' >/dev/null 2>&1 || :
sleep 2
if pgrep -f "^$GUIEXE$" >/dev/null 2>&1; then
  echo "stage=idle_input_differential result=GUI_STILL_RUNNING" >&2; exit 33
fi
echo "stage=idle_input_differential action=START log=$LOG"
if [ "$action" = --capabilities ]; then
  "$CAPBIN" --read-only >"$LOG" 2>&1
else
  "$BIN" --compare-volatile-idle-standards >"$LOG" 2>&1
fi
result=$?
cat "$LOG"
echo "stage=idle_input_differential action=COMPLETE exit=$result"
exit "$result"
