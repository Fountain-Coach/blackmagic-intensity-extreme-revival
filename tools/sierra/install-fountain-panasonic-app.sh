#!/bin/sh
# RETIRED 2026-10-25. The owner's explicit stop instruction led to the authorized disabling of
# "Fountain Panasonic Show.app" and the bounded HDMI playback executable.
# Do not restore these without a new explicit owner instruction.
# Do not reinstall this app or bypass the disabled executable.
#
# Full source-owned installer is retained in Git history for review.
# Creative image source and pure tests remain available without hardware.
set -eu
echo "stage=panasonic_app_install result=BLOCKED_LOCAL_RUNTIME_DISABLED"
echo "stage=panasonic_app_install image_source_preserved=1 writes=0"
exit 65
