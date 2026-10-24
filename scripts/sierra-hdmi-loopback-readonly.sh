#!/bin/sh
# Source-owned, READ-ONLY inventory of the silver Sierra Mac mini's HDMI
# connection to a display identifying as BMD HDMI.
#
# This script does not open Blackmagic's vendor DeckLink runtime, start any
# capture session, change any display mode, access PCI BARs or alter drivers.
# A recognized EDID is evidence of monitor/sink negotiation, NOT HDMI capture.
set -eu

if [ "$(uname -s)" != "Darwin" ]; then
    echo "stage=sierra_hdmi_inventory result=unsupported_os"
    exit 20
fi

profile=$(system_profiler SPDisplaysDataType 2>/dev/null)
if ! printf '%s\n' "$profile" | grep -F 'BMD HDMI:' >/dev/null 2>&1; then
    echo "stage=hdmi_sink result=not_detected expected=BMD_HDMI"
    echo "stage=hdmi_capture result=not_tested"
    exit 21
fi

mode=$(printf '%s\n' "$profile" |
    awk '/BMD HDMI:/{seen=1;next} seen && /Resolution:/{sub(/^[[:space:]]*Resolution:[[:space:]]*/,""); print; exit}')
online=$(printf '%s\n' "$profile" |
    awk '/BMD HDMI:/{seen=1;next} seen && /Online:/{sub(/^[[:space:]]*Online:[[:space:]]*/,""); print; exit}')
main=$(printf '%s\n' "$profile" |
    awk '/BMD HDMI:/{seen=1;next} seen && /Main Display:/{sub(/^[[:space:]]*Main Display:[[:space:]]*/,""); print; exit}')
vendor=$(ioreg -r -c AppleDisplay -l 2>/dev/null |
    awk '/"DisplayVendorID" = 8961/{found=1} END {if(found)print "BMD"; else print "unverified"}')

printf 'stage=hdmi_sink result=detected monitor_name=BMD_HDMI mode="%s" online=%s main_display=%s\n' "$mode" "$online" "$main"
printf 'stage=hdmi_edid result=%s vendor=%s\n' "$(if [ "$vendor" = BMD ]; then echo matched; else echo not_matched; fi)" "$vendor"
printf 'stage=hdmi_capture result=not_tested source_lock=not_measured frame_count=not_measured\n'
printf 'stage=safety kernel_changes=0 pci_access=0 decklink_runtime_calls=0 display_mode_changes=0\n'
