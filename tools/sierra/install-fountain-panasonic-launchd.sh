#!/bin/sh
# RETIRED 2026-10-25.
#
# During a real Panasonic show on the original Sierra Mac, the launch agent
# coach.fountain.video.panasonic was unloaded and renamed to .plist.disabled by the authorized
# agent following the owner's explicit instruction to stop the cycling. The running service then received SIGTERM.
# The source-owned application must NOT override that local disablement.
#
# Historical installer and launchd program are preserved in Git history.
# Normal supported owner: a regular LaunchServices AppKit application,
# installed by install-fountain-panasonic-app.sh. No persistence agent.
set -eu
echo "stage=panasonic_launchd result=RETIRED_LOCAL_AGENT_DISABLED"
echo "stage=panasonic_launchd supported_owner=Fountain_Panasonic_Show_AppKit"
echo "stage=panasonic_launchd writes=0 service_registration=0"
exit 65
