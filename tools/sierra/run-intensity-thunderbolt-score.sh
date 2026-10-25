#!/bin/sh
# Manual, bounded, Thunderbolt-only creative score for the original Sierra Mac.
# Read-only hardware status. Does not activate any Blackmagic input or output.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
LOCK="$HOME/Library/Application Support/Fountain Coach/Video Instrument/run/OUTPUT_DISABLED"
if [ "$(uname -m)" != "x86_64" ] || [ "$(sw_vers -productVersion)" != "10.12.6" ] || [ "$(sysctl -n hw.model)" != "Macmini5,3" ]; then
    echo "REFUSED: expected the original silver Mac mini / Sierra" >&2
    exit 20
fi
if [ ! -f "$LOCK" ]; then
    echo "REFUSED: OUTPUT_DISABLED guard missing" >&2
    exit 21
fi
if ! /usr/sbin/ioreg -r -n "Intensity Extreme@0" -l -w0 | grep -q '"device-id" = <24a10000>'; then
    echo "REFUSED: original Thunderbolt A124 not enumerated" >&2
    exit 22
fi
DEST=/tmp
cp "$ROOT/tools/sierra/intensity-thunderbolt-score.py" "$DEST/fc-intensity-thunderbolt-score-20261025.py"
cp "$ROOT/tools/sierra/intensity-thunderbolt-score.html" "$DEST/fc-intensity-thunderbolt-score-20261025.html"
clang++ -std=c++11 -O0 -Wall -Wextra "$ROOT/src/decklink-hdmi-sink-status.cpp" -framework CoreFoundation -o "$DEST/fc-intensity-status-readonly-20261025"
echo "SAFE_READ_ONLY_SCORE: open http://127.0.0.1:18743/ on the silver Mac"
exec /usr/bin/python -u "$DEST/fc-intensity-thunderbolt-score-20261025.py"
