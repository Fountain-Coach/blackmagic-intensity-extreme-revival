#!/bin/sh
# Fountain Coach Panasonic show control — normal Sierra AppKit application.
# Explicitly no LaunchAgent, no launchctl, no auto-start and no second
# output process. The app supervises a bounded 20-minute playback.
set -eu
APP="$HOME/Applications/Fountain Panasonic Show.app"
EXEC="$APP/Contents/MacOS/FountainPanasonicShow"
BASE="$HOME/Library/Application Support/Fountain Coach/Video Instrument"
LOG="$BASE/run/panasonic-gui-performance.log"

usage(){
 echo "usage: fountain-panasonic-show.sh start|status|stop" >&2
 exit 64
}
owned_pid(){
 [ -x "$EXEC" ] || return 1
 ps -axo pid=,command= | awk -v path="$EXEC" '
   $2 == path && count==0 { print $1; count=1 }
 ' | grep -E '^[0-9]+$' | head -n 1
}
read_log(){
 if [ -r "$LOG" ];then tail -n 7 "$LOG";fi
}
[ $# -eq 1 ] || usage
case "$1" in
 start)
  [ -x "$EXEC" ] || { echo "stage=show result=NO_OWNED_APP";exit 20; }
  if pid=$(owned_pid);then
    echo "stage=show result=ALREADY_RUNNING owner=AppKit pid=$pid"
    exit 0
  fi
  open -g -a "$APP"
  sleep 2
  if pid=$(owned_pid);then
    echo "stage=show result=RUNNING owner=AppKit pid=$pid max_minutes=20"
    read_log
  else
    echo "stage=show result=LAUNCH_FAILED"
    read_log
    exit 21
  fi
  ;;
 status)
  if pid=$(owned_pid);then
    echo "stage=show result=RUNNING owner=AppKit pid=$pid max_minutes=20"
    ps -p "$pid" -o pid=,etime=,command=
  else
    echo "stage=show result=NOT_RUNNING owner=AppKit"
  fi
  read_log
  ;;
 stop)
  if pid=$(owned_pid);then
    # Recheck exact executable before signalling any PID.
    actual=$(ps -p "$pid" -o command= 2>/dev/null || true)
    [ "$actual" = "$EXEC" ] || {
      echo "stage=show result=PID_IDENTITY_CHANGED"
      exit 23
    }
    kill -TERM "$pid"
    echo "stage=show result=STOP_REQUESTED pid=$pid"
    sleep 2
    if owned_pid >/dev/null;then
      echo "stage=show result=STILL_STOPPING"
      exit 22
    fi
    echo "stage=show result=STOPPED"
    read_log
  else
    echo "stage=show result=NOT_RUNNING"
  fi
  ;;
 *)usage;;
esac
