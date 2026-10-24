#!/bin/sh
# Sierra independent synthetic window / subregion / owner acceptance.
# No desktop, Blackmagic, HDMI, network, or recorded application pixels.
set -eu
ROOT=/Volumes/Fresh/FountainVideoDockerLab
BIN="$ROOT/bin/fountain_window_tap"
DIR="$ROOT/private-window-tap"
if [ "$(uname -s)" != Darwin ] ||
   [ "$(sw_vers -productVersion)" != 10.12.6 ] ||
   [ "$(sysctl -n hw.model)" != Macmini5,3 ]; then
    echo "stage=external_window result=WRONG_HOST"; exit 20
fi
test -x "$BIN" && test -d "$DIR" || {
    echo "stage=external_window result=NOT_PREPARED"; exit 21
}
umask 077
HOLD=$(mktemp "$DIR/external-window-holder.XXXXXXXX")
ONE=$(mktemp "$DIR/external-window-probe-one.XXXXXXXX")
TWO=$(mktemp "$DIR/external-window-probe-two.XXXXXXXX")
CROP=$(mktemp "$DIR/external-window-probe-crop.XXXXXXXX")
NEG=$(mktemp "$DIR/external-window-probe-negative.XXXXXXXX")
holder=
cleanup() {
    [ -n "$holder" ] && kill "$holder" 2>/dev/null || :
    [ -n "$holder" ] && wait "$holder" 2>/dev/null || :
}
trap cleanup EXIT HUP INT TERM
"$BIN" --fixture-hold >"$HOLD" 2>&1 &
holder=$!
line=
tries=0
while [ "$tries" -lt 30 ]; do
    line=$(grep '^stage=window_fixture owner_pid=.* mode=HOLD' "$HOLD" | head -1 || :)
    [ -n "$line" ] && break
    if ! kill -0 "$holder" 2>/dev/null; then
        echo "stage=external_window result=HOLDER_EXITED"; exit 23
    fi
    tries=$((tries+1))
    sleep .3
done
[ -n "$line" ] || { echo "stage=external_window result=HOLDER_NOT_READY"; exit 24; }
pid=$(printf '%s\n' "$line" | sed -n 's/.*owner_pid=\([0-9]*\) window_id=.*/\1/p')
wid=$(printf '%s\n' "$line" | sed -n 's/.*window_id=\([0-9]*\) mode=.*/\1/p')
[ "$pid" = "$holder" ] || { echo "stage=external_window result=PID_MISMATCH"; exit 25; }
[ -n "$wid" ] || exit 26
"$BIN" --probe --id "$wid" --pid "$pid" >"$ONE" 2>&1 || {
    echo "stage=external_window result=FIRST_PROBE_FAILED"; exit 27
}
sleep .7
"$BIN" --probe --id "$wid" --pid "$pid" >"$TWO" 2>&1 || {
    echo "stage=external_window result=SECOND_PROBE_FAILED"; exit 28
}
hash1=$(sed -n 's/.* sha256=\([0-9a-f]*\) syphon.*/\1/p' "$ONE")
hash2=$(sed -n 's/.* sha256=\([0-9a-f]*\) syphon.*/\1/p' "$TWO")
if [ "$(printf %s "$hash1" | wc -c | tr -d ' ')" != 64 ] ||
   [ "$(printf %s "$hash2" | wc -c | tr -d ' ')" != 64 ] ||
   [ "$hash1" = "$hash2" ]; then
    echo "stage=external_window result=NO_CHANGING_PIXELS"; exit 29
fi
"$BIN" --probe --id "$wid" --pid "$pid" --rect 32,32,128,96 >"$CROP" 2>&1 || {
    echo "stage=external_window result=VALID_CROP_FAILED"; exit 30
}
grep ' width=128 height=96 crop=1 ' "$CROP" >/dev/null || {
    echo "stage=external_window result=WRONG_CROP"; exit 31
}
if "$BIN" --probe --id "$wid" --pid "$pid" --rect 620,370,80,80 >"$NEG" 2>&1; then
    echo "stage=external_window result=ESCAPING_CROP_ALLOWED"; exit 32
fi
grep 'result=IMAGE_UNAVAILABLE_OR_RECT_OUT_OF_BOUNDS' "$NEG" >/dev/null || {
    echo "stage=external_window result=WRONG_CROP_REJECTION"; exit 33
}
if "$BIN" --probe --id "$wid" --pid "$pid" --rect bogus >"$NEG" 2>&1; then
    echo "stage=external_window result=MALFORMED_CROP_ALLOWED"; exit 34
fi
grep 'result=INVALID_RECT' "$NEG" >/dev/null || {
    echo "stage=external_window result=WRONG_MALFORMED_REJECTION"; exit 35
}
wrong=$((pid+1))
if "$BIN" --probe --id "$wid" --pid "$wrong" >"$NEG" 2>&1; then
    echo "stage=external_window result=WRONG_OWNER_ALLOWED"; exit 36
fi
grep 'result=WINDOW_IDENTITY_NOT_VISIBLE' "$NEG" >/dev/null || {
    echo "stage=external_window result=WRONG_OWNER_REJECTION"; exit 37
}
echo "stage=external_window result=PASS source=SEPARATE_SYNTHETIC_WINDOW changing_rgba_hashes=2 valid_crop_128x96=1 escaping_crop_rejected=1 malformed_crop_rejected=1 wrong_owner_rejected=1 disk_pixels_saved=0 syphon_published=0 wlan_sent=0"
