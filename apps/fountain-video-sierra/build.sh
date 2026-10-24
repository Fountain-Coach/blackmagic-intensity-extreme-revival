#!/bin/sh
# Native Fountain Video.app build with Apple's preinstalled Sierra toolchain.
# User-owned output. No installation, root, kernel action or SDK download.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
folder="$root/apps/fountain-video-sierra"
output="${1:-$root/build/Fountain Video.app}"
case "$output" in
  *.app) ;;
  *) echo "Output must end with .app" >&2; exit 64 ;;
esac
if [ "$(uname -s)" != "Darwin" ]; then
  echo "This native AppKit target requires macOS." >&2; exit 20
fi
if [ "$(uname -m)" != "x86_64" ]; then
  echo "This build targets the original Intel Sierra host." >&2; exit 21
fi
mkdir -p "$output/Contents/MacOS"
cp "$folder/Info.plist" "$output/Contents/Info.plist"
plutil -lint "$output/Contents/Info.plist"
clang++ -std=c++11 -x objective-c++ -fobjc-arc -fblocks \
  -arch x86_64 -mmacosx-version-min=10.12 \
  -Wall -Wextra -Werror -Wno-unused-parameter \
  "$folder/FountainVideo.mm" \
  -framework Cocoa -framework CoreFoundation -framework CoreGraphics \
  -framework AVFoundation -framework CoreMedia -framework CoreVideo \
  -o "$output/Contents/MacOS/FountainVideo"
file "$output/Contents/MacOS/FountainVideo"
echo "stage=native_app result=built bundle=$output"
echo "note=user_space_only; kernel_drivers_untouched=1"
