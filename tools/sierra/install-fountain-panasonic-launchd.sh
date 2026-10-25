#!/bin/sh
# Fountain Coach: stable, bounded Panasonic playout via the Sierra user's
# own launchd Aqua domain. Does not run at login, alter drivers or settings,
# touch display modes, or start a second stream.
# Invoked only after an explicit display request.
set -eu
LABEL=coach.fountain.video.panasonic
UID_NUM=$(id -u)
SERVICE="gui/$UID_NUM/$LABEL"
BASE="$HOME/Library/Application Support/Fountain Coach/Video Instrument"
BIN="$BASE/bin/fountain-video-output"
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
LOG="$BASE/run/panasonic-launchd.log"
ERRORLOG="$BASE/run/panasonic-launchd-error.log"
case "$(sw_vers -productVersion)" in 10.12.*) ;; *)echo "stage=launchd result=WRONG_OS";exit 20;;esac
[ "$(sysctl -n hw.model)" = "Macmini5,3" ] || {
  echo "stage=launchd result=WRONG_MAC";exit 21;
}
[ -x "$BIN" ] || {echo "stage=launchd result=NO_INSTALLED_ADAPTER";exit 22;}
mkdir -p "$HOME/Library/LaunchAgents" "$BASE/run"
if [ -e "$PLIST" ];then
  echo "stage=launchd existing_plist=1 path=$PLIST"
else
  TEMP="$PLIST.new.$$"
  trap 'rm -f "$TEMP"' EXIT HUP INT TERM
  cat > "$TEMP" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>Label</key><string>$LABEL</string>
  <key>ProgramArguments</key><array>
    <string>$BIN</string>
    <string>--mode</string>
    <string>720p60</string>
    <string>--minutes</string>
    <string>20</string>
  </array>
  <key>StandardOutPath</key><string>$LOG</string>
  <key>StandardErrorPath</key><string>$ERRORLOG</string>
  <key>WorkingDirectory</key><string>$BASE</string>
  <key>ProcessType</key><string>Interactive</string>
  <key>RunAtLoad</key><false/>
  <key>KeepAlive</key><false/>
  <key>ThrottleInterval</key><integer>10</integer>
</dict></plist>
PLIST
  plutil -lint "$TEMP"
  mv "$TEMP" "$PLIST"
fi
plutil -lint "$PLIST"
if launchctl print "$SERVICE" >/dev/null 2>&1;then
  echo "stage=launchd result=ALREADY_REGISTERED service=$SERVICE"
else
  launchctl bootstrap "gui/$UID_NUM" "$PLIST"
  echo "stage=launchd result=REGISTERED service=$SERVICE"
fi
echo "stage=launchd run_at_login=0 keep_alive=0 max_minutes=20"
echo "stage=launchd start_command=launchctl_kickstart"
