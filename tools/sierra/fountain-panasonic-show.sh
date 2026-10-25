#!/bin/sh
# Fountain Coach: show controls for an explicitly requested Panasonic session.
# The bounded playback process belongs to Sierra's GUI launchd domain, not
# an SSH/remote-command shell process group. No autostart or KeepAlive.
set -eu
LABEL=coach.fountain.video.panasonic
SERVICE="gui/$(id -u)/$LABEL"
BASE="$HOME/Library/Application Support/Fountain Coach/Video Instrument"
LOG="$BASE/run/panasonic-launchd.log"
ERRORLOG="$BASE/run/panasonic-launchd-error.log"

usage(){
 echo "usage: fountain-panasonic-show.sh start|status|stop" >&2
 exit 64
}
registered(){
 launchctl print "$SERVICE" >/dev/null 2>&1
}
running(){
 registered || return 1
 details=$(launchctl print "$SERVICE" 2>/dev/null)
 printf "%s\n" "$details" | grep -q 'state = running'
}
tail_logs(){
 if [ -f "$LOG" ];then tail -n 6 "$LOG";fi
 if [ -s "$ERRORLOG" ];then
   echo "stage=show stderr_present=1"
   tail -n 3 "$ERRORLOG"
 fi
}
[ $# -eq 1 ] || usage
case "$1" in
 start)
  if ! registered;then
    echo "stage=show result=NOT_REGISTERED"
    exit 20
  fi
  if running;then
    echo "stage=show result=ALREADY_RUNNING max_minutes=20"
    exit 0
  fi
  launchctl kickstart -p "$SERVICE"
  sleep 2
  if ! running;then
    echo "stage=show result=FAILED_TO_START"
    tail_logs
    exit 21
  fi
  echo "stage=show result=RUNNING lease=launchd max_minutes=20"
  tail_logs
  ;;
 status)
  if running;then
    echo "stage=show result=RUNNING lease=launchd max_minutes=20"
    launchctl print "$SERVICE" | grep -E 'state = running|pid = |program = ' | head -n 8
  elif registered;then
    echo "stage=show result=NOT_RUNNING registered=1"
  else
    echo "stage=show result=NOT_REGISTERED"
  fi
  tail_logs
  ;;
 stop)
  if ! registered;then
    echo "stage=show result=NOT_REGISTERED"
    exit 20
  fi
  if running;then
    launchctl kill SIGTERM "$SERVICE"
    echo "stage=show result=STOP_REQUESTED"
    sleep 2
    if running;then
      echo "stage=show result=STILL_STOPPING"
      exit 22
    fi
    echo "stage=show result=STOPPED"
    tail_logs
  else
    echo "stage=show result=ALREADY_STOPPED"
  fi
  ;;
 *)usage;;
esac
