#!/bin/sh
# Fountain Coach — Panasonic Die Maschine video playout control.
# No GUI scripting, external services, firmware writes, kext changes,
# input-stream takeover, or driver update.
# A bounded video show is started only on an authorized user request.
# Copyright (c) Fountain Coach contributors, 2026. MIT.
set -eu
BASE="$HOME/Library/Application Support/Fountain Coach/Video Instrument"
BIN="$BASE/bin/fountain-video-output"
RUN="$BASE/run"
PIDFILE="$RUN/panasonic-die-maschine-show.pid"
LOG="$RUN/panasonic-die-maschine-show.log"

usage(){
 printf "usage: fountain-panasonic-show.sh start [minutes_1_to_20] | status | stop\n" >&2
 exit 64
}
live(){
 if [ ! -r "$PIDFILE" ];then return 1;fi
 pid=$(cat "$PIDFILE")
 case "$pid" in ''|*[!0-9]*)return 1;;esac
 if ! kill -0 "$pid" 2>/dev/null;then return 1;fi
 # Do not operate an unrelated process, even when a stale PID was reused.
 current=$(ps -p "$pid" -o command= 2>/dev/null || true)
 case "$current" in
  *"/bin/fountain-video-output --mode 720p60 --minutes "*)return 0;;
  *)return 1;;
 esac
}
if [ $# -lt 1 ];then usage;fi
case "$1" in
 start)
  if [ $# -gt 2 ];then usage;fi
  if [ $# -eq 2 ];then minutes=$2;else minutes=20;fi
  case "$minutes" in ''|*[!0-9]*)usage;;esac
  if [ "$minutes" -lt 1 ] || [ "$minutes" -gt 20 ];then usage;fi
  [ -x "$BIN" ] || { echo "stage=show result=NO_INSTALLED_HARDWARE_ADAPTER"; exit 20; }
  mkdir -p "$RUN"
  if live;then
    echo "stage=show result=ALREADY_RUNNING pid=$pid"
    exit 0
  fi
  rm -f "$PIDFILE"
  nohup "$BIN" --mode 720p60 --minutes "$minutes" > "$LOG" 2>&1 < /dev/null &
  pid=$!
  printf "%s\n" "$pid" > "$PIDFILE"
  sleep 2
  if ! live;then
    echo "stage=show result=FAILED_START"
    tail -n 15 "$LOG" || true
    exit 21
  fi
  echo "stage=show result=RUNNING pid=$pid max_minutes=$minutes mode=720p60"
  tail -n 6 "$LOG"
  ;;
 status)
  if live;then
    echo "stage=show result=RUNNING pid=$pid"
    ps -p "$pid" -o pid=,etime=,command=
    tail -n 4 "$LOG"
  else
    echo "stage=show result=NOT_RUNNING"
    if [ -r "$LOG" ];then tail -n 8 "$LOG";fi
  fi
  ;;
 stop)
  if live;then
    kill -TERM "$pid"
    echo "stage=show result=STOP_REQUESTED pid=$pid"
    sleep 2
    if live;then
      echo "stage=show result=STILL_STOPPING pid=$pid"
      exit 22
    fi
    echo "stage=show result=STOPPED"
    tail -n 5 "$LOG"
  else
    echo "stage=show result=NO_ACTIVE_OWNED_SHOW"
  fi
  ;;
 *)usage;;
esac
