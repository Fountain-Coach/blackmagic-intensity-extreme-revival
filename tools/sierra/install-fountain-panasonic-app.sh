#!/bin/sh
# RETIRED 2026-10-25. A local process renamed the newly created app to
# "Fountain Panasonic Show.app.disabled", and the bounded video output
# program was subsequently made non-executable. Owner/provenance unknown.
# Do not reinstall this app or bypass the disabled executable.
#
# Full source-owned installer is retained in Git history for review.
# Creative image source and pure tests remain available without hardware.
set -eu
echo "stage=panasonic_app_install result=BLOCKED_LOCAL_RUNTIME_DISABLED"
echo "stage=panasonic_app_install image_source_preserved=1 writes=0"
exit 65
