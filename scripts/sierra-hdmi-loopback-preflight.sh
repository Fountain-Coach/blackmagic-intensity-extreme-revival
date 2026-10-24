#!/bin/sh
# Fountain Coach — repeatable NONINVASIVE HDMI physical-loopback preflight.
#
# Proves: BMD EDID/sink negotiation, actual OS framebuffer reference,
# source-owned callback ABI conformance (including real Sierra toolchain).
# It intentionally NEVER starts vendor capture while Desktop Video 10.4's
# video-frame destructor crash is unresolved.
#
# Usage: sh scripts/sierra-hdmi-loopback-preflight.sh
# No screen content is persisted/transmitted; binaries are built in /tmp only.
set -eu

if [ "$(uname -s)" != "Darwin" ]; then
    echo "stage=hdmi_preflight result=unsupported_os expected=Sierra"
    exit 20
fi

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
temporary=$(mktemp -d /tmp/fc-hdmi-preflight.XXXXXX)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

echo "stage=hdmi_preflight step=edid_and_output_mode"
sh "$repo/scripts/sierra-hdmi-loopback-readonly.sh"
echo "stage=hdmi_preflight step=edid_capabilities"
perl "$repo/tools/sierra/hdmi-edid-capability-witness.pl"

echo "stage=hdmi_preflight step=source_pixels"
clang -std=c11 -Wall -Wextra -Werror \
  "$repo/tools/sierra/hdmi-output-framebuffer-witness.c" \
  -framework CoreGraphics -framework CoreFoundation \
  -o "$temporary/output-reference"
"$temporary/output-reference"

echo "stage=hdmi_preflight step=mock_callback_abi"
clang++ -std=c++11 -Wall -Wextra -Werror \
  "$repo/tests/mock-decklink-callback-abi.cpp" \
  -framework CoreFoundation \
  -o "$temporary/observer-mock"
"$temporary/observer-mock"

echo "stage=hdmi_preflight step=installed_vendor_state"
if ! kextstat | grep -F 'com.blackmagic-design.desktopvideo.iokit.driver (10.4)' >/dev/null; then
  echo "stage=installed_blackmagic result=unexpected_driver"
  exit 40
fi
echo "stage=installed_blackmagic result=detected version=10.4"

found=0
for report in "$HOME"/Library/Logs/DiagnosticReports/fc-decklink-input-observer-with-evidence*.crash; do
    if [ -f "$report" ] &&
       grep -F 'CDeckLinkVideoInputFrame::~CDeckLinkVideoInputFrame' "$report" >/dev/null &&
       grep -F 'EXC_BAD_ACCESS' "$report" >/dev/null; then
        found=1
        break
    fi
done
if [ "$found" -ne 1 ]; then
    echo "stage=capture_crash_evidence result=not_found no_live_retry=1"
    exit 41
fi

echo "stage=capture_crash_evidence result=present vendor_frame_release=invalid"
echo "stage=physical_capture result=NOT_ACCEPTED signal_lock=unverified real_frame=not_witnessed"
echo "stage=hdmi_preflight result=pass edid=1 edid_caps=1 framebuffer=1 callback_mock=1 crash_evidence=1 vendor_capture_calls=0 driver_modifications=0"
