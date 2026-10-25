#!/bin/sh
# Install the source-owned Fountain Video Instrument on the original Sierra Mac.
# Does NOT install a third-party application, touch kernel/firmware, or start
# any hardware input/output stream. Compiles with local Xcode/clang.
#
# Syphon is a documented third-party BSD-licensed interprocess transport:
# we reuse the already installed framework, NOT Black Syphon's GUI/logic.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
if [ "$(sw_vers -productVersion)" != "10.12.6" ] || [ "$(uname -m)" != "x86_64" ]; then
  echo 'install=BLOCKED reason=not_original_Sierra_x86_64' >&2
  exit 20
fi
if [ "$(sysctl -n hw.model)" != "Macmini5,3" ]; then
  echo 'install=BLOCKED reason=not_silver_Macmini5_3' >&2
  exit 21
fi
# Never replace the hardware-output executable during an active
# launchd-owned Panasonic performance, even if rename would be atomic.
SERVICE="gui/$(id -u)/coach.fountain.video.panasonic"
if launchctl print "$SERVICE" 2>/dev/null | grep -q 'state = running'; then
  echo 'install=BLOCKED reason=active_panasonic_show' >&2
  exit 30
fi
FRAMEWORK="/Applications/Syphon Recorder.app/Contents/Frameworks/Syphon.framework"
if [ ! -f "$FRAMEWORK/Syphon" ]; then
  echo 'install=BLOCKED reason=existing_Syphon_framework_missing' >&2
  exit 22
fi
SOURCE="$ROOT/src"
if [ ! -r "$SOURCE/fountain-video-score.h" ] ||
   [ ! -r "$SOURCE/fountain-syphon-instrument.m" ] ||
   [ ! -r "$SOURCE/fountain-video-output.cpp" ] ||
   [ ! -r "$ROOT/tools/sierra/fountain-syphon-frame-witness.m" ] ||
   [ ! -r "$ROOT/tests/test_fountain_video_score.c" ] ||
   [ ! -r "$ROOT/tools/sierra/fountain-panasonic-show.sh" ]; then
   echo 'install=BLOCKED reason=source_tree_incomplete' >&2
   exit 23
fi
PREFIX="$HOME/Library/Application Support/Fountain Coach/Video Instrument"
DEST="$HOME/Applications/Fountain Video Instrument.app"
mkdir -p "$HOME/Applications" "$PREFIX/bin" "$PREFIX/tmp"
STAGE=$(mktemp -d "$PREFIX/tmp/stage.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT
APP="$STAGE/Fountain Video Instrument.app"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources"
SYH_PARENT=$(dirname "$FRAMEWORK")
clang -fobjc-arc -Wall -Wextra -Werror -std=gnu11 -I "$SOURCE" \
    "$SOURCE/fountain-syphon-instrument.m" -F "$SYH_PARENT" \
    -framework Syphon -framework Cocoa -framework OpenGL \
    -o "$APP/Contents/MacOS/FountainVideoInstrument"
ditto "$FRAMEWORK" "$APP/Contents/Frameworks/Syphon.framework"
cat > "$APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>coach.fountain.video.instrument</string>
<key>CFBundleName</key><string>Fountain Video Instrument</string>
<key>CFBundleExecutable</key><string>FountainVideoInstrument</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleVersion</key><string>0.1.0</string>
<key>CFBundleShortVersionString</key><string>0.1.0</string>
<key>LSMinimumSystemVersion</key><string>10.12</string>
<key>NSHighResolutionCapable</key><true/>
</dict></plist>
PLIST
plutil -lint "$APP/Contents/Info.plist"
cat > "$APP/Contents/Resources/ABOUT.txt" <<'NOTICE'
Fountain Video Instrument / Die Maschine
Source: Fountain-Coach/blackmagic-intensity-extreme-revival

Frame generation and scheduling code are Fountain-owned.
The included Syphon.framework is a third-party BSD-licensed transport;
its license text is retained in the original framework's public headers.
No Black Syphon application logic or vendor capture implementation is used.

The separately built Fountain HDMI playback adapter currently calls the
original installed Blackmagic DeckLink 10.4 runtime. It is NOT a
replacement Thunderbolt/PCI driver. Hardware pixels on the Panasonic
have NOT yet been optically verified.
NOTICE
clang++ -O2 -std=c++11 -Wall -Wextra -Werror -I "$SOURCE" \
    "$SOURCE/fountain-video-output.cpp" -framework CoreFoundation -ldl \
    -o "$STAGE/fountain-video-output"
clang -O2 -std=c11 -Wall -Wextra -Werror -I "$SOURCE" \
    "$ROOT/tests/test_fountain_video_score.c" \
    -o "$STAGE/test_fountain_video_score"
"$STAGE/test_fountain_video_score"
WITNESS="$STAGE/FountainSyphonWitness.app"
mkdir -p "$WITNESS/Contents/MacOS" "$WITNESS/Contents/Frameworks"
clang -fobjc-arc -Wall -Wextra -Werror -fblocks -std=gnu11 \
    "$ROOT/tools/sierra/fountain-syphon-frame-witness.m" \
    -F "$SYH_PARENT" -framework Syphon -framework Cocoa \
    -framework OpenGL -o "$WITNESS/Contents/MacOS/FountainSyphonWitness"
ditto "$FRAMEWORK" "$WITNESS/Contents/Frameworks/Syphon.framework"
if [ -e "$DEST" ]; then
  BACKUP="$PREFIX/tmp/last-good-app.$(date +%Y%m%d%H%M%S)"
  mv "$DEST" "$BACKUP"
  echo "install=previous_bundle_retained_for_rollback path=$BACKUP"
fi
mv "$APP" "$DEST"
if [ -x "$PREFIX/bin/fountain-video-output" ]; then
   cp "$PREFIX/bin/fountain-video-output" "$PREFIX/tmp/fountain-video-output.last-good.$(date +%Y%m%d%H%M%S)"
fi
mv -f "$STAGE/fountain-video-output" "$PREFIX/bin/fountain-video-output"
sh -n "$ROOT/tools/sierra/fountain-panasonic-show.sh"
cp "$ROOT/tools/sierra/fountain-panasonic-show.sh" "$PREFIX/bin/fountain-panasonic-show.sh"
chmod 755 "$PREFIX/bin/fountain-panasonic-show.sh"
mv -f "$STAGE/test_fountain_video_score" "$PREFIX/bin/test_fountain_video_score"
if [ -e "$PREFIX/bin/FountainSyphonWitness.app" ]; then
   rm -rf "$PREFIX/bin/FountainSyphonWitness.app"
fi
mv "$WITNESS" "$PREFIX/bin/FountainSyphonWitness.app"
printf "install=PASS app=%s\n" "$DEST"
printf "install=PASS physical_output_adapter=%s\n" "$PREFIX/bin/fountain-video-output"
printf "install=PASS witness=%s\n" "$PREFIX/bin/FountainSyphonWitness.app"
printf 'install=UNCHANGED blackmagic_driver=1 firmware=1 physical_cables=1\n'
