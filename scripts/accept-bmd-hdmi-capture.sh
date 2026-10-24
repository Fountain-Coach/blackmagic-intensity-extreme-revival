#!/bin/sh
# Physical acceptance contract for the ONE FINAL user-owned topology:
# silver Macmini5,3 HDMI OUT -> ORIGINAL Intensity Extreme HDMI IN -> same
# Macmini5,3 running macOS Sierra 10.12.6 via Thunderbolt 1/Comdis dock.
#
# DO NOT substitute an external camera, another Mac, an HDMI generator, or
# the old Nikon scan. DO NOT re-run the crashing GetStreamTime observer.
#
# Default --check is read-only and does NOT stop a running GUI capture.
# --run explicitly starts the native AppKit self-test ONLY if the GUI is idle.
# Proprietary driver 10.4 and firmware are never modified.
set -eu
operation="${1:---check}"
case "$operation" in
  --check|--run) ;;
  *) echo "usage: $0 [--check|--run]" >&2; exit 64 ;;
esac
target="bene@Uwes-Mac-mini.local"
echo "stage=fixed_fixture silver_hdmi_out=1 original_intensity_hdmi_in=1"
echo "stage=hardware_claim current_driver=10.4 host=Sierra_10.12.6 alternate_sources=forbidden"
if [ "$operation" = "--check" ]; then
  ssh -o BatchMode=yes -o ConnectTimeout=5 "$target" \
    'set -eu
     test "$(sw_vers -productVersion)" = "10.12.6"
     test "$(sysctl -n hw.model)" = "Macmini5,3"
     kextstat | grep -F "com.blackmagic-design.desktopvideo.iokit.driver (10.4)" >/dev/null
     system_profiler SPDisplaysDataType | grep -A2 "BMD HDMI:" | grep -F "1920 x 1080 @ 30 Hz" >/dev/null
     test -x "/Users/bene/Applications/Fountain Video.app/Contents/MacOS/FountainVideo"
     echo "stage=fixed_hdmi_source result=ready mode=1080p30"
     if pgrep -f "^/Users/bene/Applications/Fountain Video.app/Contents/MacOS/FountainVideo$" >/dev/null; then
       echo "stage=native_capture_app result=running"
     else
       echo "stage=native_capture_app result=not_running"
     fi
     echo "stage=physical_capture result=not_tested_in_readonly_check see=tests/2026-10-24-fixed-silver-hdmi-physical-capture-and-native-app-accepted.md"'
  exit
fi
ssh -o BatchMode=yes -o ConnectTimeout=5 "$target" \
  'set -eu
   test "$(sw_vers -productVersion)" = "10.12.6"
   test "$(sysctl -n hw.model)" = "Macmini5,3"
   kextstat | grep -F "com.blackmagic-design.desktopvideo.iokit.driver (10.4)" >/dev/null
   system_profiler SPDisplaysDataType | grep -A2 "BMD HDMI:" | grep -F "1920 x 1080 @ 30 Hz" >/dev/null
   if pgrep -f "^/Users/bene/Applications/Fountain Video.app/Contents/MacOS/FountainVideo$" >/dev/null; then
     echo "stage=fixed_native_capture result=client_already_running no_concurrent_vendor_access=1" >&2
     exit 40
   fi
   exec "/Users/bene/Applications/Fountain Video.app/Contents/MacOS/FountainVideo" --self-test-fixed-silver-hdmi'
