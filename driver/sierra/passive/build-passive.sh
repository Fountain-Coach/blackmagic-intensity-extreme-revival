#!/bin/sh
# Fountain Coach - source-owned Sierra kext BUILD ONLY. NEVER install/load.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
sdk=$(xcrun --sdk macosx --show-sdk-path)
headers="$sdk/System/Library/Frameworks/Kernel.framework/Headers"

if [ "$(uname -m)" != "x86_64" ]; then
  echo "This experimental build targets the Intel Sierra host." >&2
  exit 64
fi

stage=$(mktemp -d "${TMPDIR:-/tmp}/fc-intensity-passive.XXXXXX")
bundle="$stage/FCIntensityPCIDiagnostic.kext"
mkdir -p "$bundle/Contents/MacOS"

clang++ -std=c++11 -arch x86_64 -mmacosx-version-min=10.12 \
  -Wall -Wextra -Werror -Wno-inconsistent-missing-override \
  -fno-exceptions -fno-rtti -fapple-kext -DKERNEL -D__KERNEL__ \
  -I "$headers" -c "$here/FCIntensityPCIDiagnostic.cpp" \
  -o "$stage/FCIntensityPCIDiagnostic.o"

clang -arch x86_64 -mmacosx-version-min=10.12 \
  -Wall -Wextra -Werror -DKERNEL -D__KERNEL__ \
  -isysroot "$sdk" -c "$here/FCIntensityKmodInfo.c" \
  -o "$stage/FCIntensityKmodInfo.o"

ld -kext -arch x86_64 -macosx_version_min 10.12 \
  -L "$sdk/usr/lib" \
  -o "$bundle/Contents/MacOS/FCIntensityPCIDiagnostic" \
  "$stage/FCIntensityPCIDiagnostic.o" -lkmodc++ \
  "$stage/FCIntensityKmodInfo.o" -lkmod

cp "$here/Info.plist" "$bundle/Contents/Info.plist"
plutil -lint "$bundle/Contents/Info.plist"
file "$bundle/Contents/MacOS/FCIntensityPCIDiagnostic"
echo "stage=passive_bundle_built"
echo "bundle=$bundle"
echo "note=not_installed_not_loaded"
