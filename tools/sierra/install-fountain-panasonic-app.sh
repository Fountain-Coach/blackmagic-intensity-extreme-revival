#!/bin/sh
# Build and install a real AppKit owner for Panasonic video playout, using
# only original Sierra system frameworks and the source-owned adapter.
# No persistent launch agent is installed. Nothing is started on login.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
if [ "$(sw_vers -productVersion)" != "10.12.6" ] ||
   [ "$(sysctl -n hw.model)" != "Macmini5,3" ]; then
   echo "stage=panasonic_app_install result=WRONG_ORIGINAL_HOST"
   exit 20
fi
SOURCE="$ROOT/src/fountain-panasonic-app.m"
BASE="$HOME/Library/Application Support/Fountain Coach/Video Instrument"
OUTPUT="$BASE/bin/fountain-video-output"
DEST="$HOME/Applications/Fountain Panasonic Show.app"
[ -r "$SOURCE" ] && [ -x "$OUTPUT" ] || {
   echo "stage=panasonic_app_install result=SOURCE_OR_OUTPUT_MISSING"
   exit 21
}
mkdir -p "$BASE/tmp" "$HOME/Applications"
STAGE=$(mktemp -d "$BASE/tmp/panasonicapp.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT HUP INT TERM
APP="$STAGE/Fountain Panasonic Show.app"
mkdir -p "$APP/Contents/MacOS"
clang -O2 -fobjc-arc -std=gnu11 -Wall -Wextra -Werror \
    "$SOURCE" -framework Cocoa -framework Foundation \
    -o "$APP/Contents/MacOS/FountainPanasonicShow"
cat > "$APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>coach.fountain.video.panasonic.show</string>
<key>CFBundleName</key><string>Fountain Panasonic Show</string>
<key>CFBundleExecutable</key><string>FountainPanasonicShow</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleVersion</key><string>0.1.0</string>
<key>CFBundleShortVersionString</key><string>0.1.0</string>
<key>LSMinimumSystemVersion</key><string>10.12</string>
<key>LSUIElement</key><true/>
<key>NSHighResolutionCapable</key><true/>
</dict></plist>
PLIST
plutil -lint "$APP/Contents/Info.plist"
if [ -e "$DEST" ]; then
   BACKUP="$BASE/tmp/last-good-panasonic-app.$(date +%Y%m%d%H%M%S)"
   mv "$DEST" "$BACKUP"
   echo "stage=panasonic_app_install rollback_app=$BACKUP"
fi
mv "$APP" "$DEST"
printf "stage=panasonic_app_install result=PASS app=%s\n" "$DEST"
printf "stage=panasonic_app_install autostart=0 system_changes=0\n"
