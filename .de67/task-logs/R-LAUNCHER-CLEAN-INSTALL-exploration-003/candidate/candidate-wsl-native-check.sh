#!/usr/bin/env bash
set +e
base=/var/tmp/r-launcher-exploration-003-candidate
exe="$base/extract/linux/Catapult-Dabubu.x86_64"
log="$base/pulseaudio-observed.log"
export HOME="$base/home"
export XDG_CONFIG_HOME="$HOME/.config"
export XDG_CACHE_HOME="$HOME/.cache"
export XDG_DATA_HOME="$HOME/.local/share"
export CATAPULT_DABUBU_DATA_DIR="$base/data"
export LACAPULT_OLLAMA_FIXTURE=command_missing
export PULSE_SERVER=unix:/mnt/wslg/PulseServer
sha256sum "$exe"
printf 'DISPLAY=%s WAYLAND_DISPLAY=%s PULSE_SERVER=%s\n' "$DISPLAY" "$WAYLAND_DISPLAY" "$PULSE_SERVER"
"$exe" --audio-driver PulseAudio >"$log" 2>&1 < /dev/null &
pid=$!
sleep 3
if test -r "/proc/$pid/stat"; then
  printf 'PROCESS=live PID=%s START_TICKS=' "$pid"
  awk '{print $22}' "/proc/$pid/stat"
  printf 'EXE='; readlink "/proc/$pid/exe"
  printf 'CMD='; tr '\0' ' ' < "/proc/$pid/cmdline"; printf '\n'
else
  printf 'PROCESS=exited-early PID=%s\n' "$pid"
fi
xwininfo -root -tree
winid=$(xwininfo -root -tree 2>/dev/null | awk '/Catapult-Dabubu/ && $1 ~ /^0x/ {print $1; exit}')
printf 'GAME_WINDOW=%s\n' "$winid"
if test -n "$winid"; then
  xwininfo -id "$winid"
  xprop -id "$winid" WM_NAME WM_CLASS
  xwd -id "$winid" -silent -out /mnt/c/Users/josef/AppData/Local/Temp/r-launcher-exploration-003-candidate/catapult-window.xwd
fi
sleep 5
if test -r "/proc/$pid/stat"; then
  printf 'PREQUIT=live PID=%s START_TICKS=' "$pid"
  awk '{print $22}' "/proc/$pid/stat"
  kill -TERM "$pid"
  wait "$pid"
  rc=$?
  printf 'EXIT=wait_status:%s\n' "$rc"
else
  printf 'PREQUIT=already-exited PID=%s\n' "$pid"
fi
if test -e "/proc/$pid"; then printf 'POSTQUIT=still-present\n'; else printf 'POSTQUIT=absent\n'; fi
printf 'OLLAMA='; command -v ollama || echo absent
printf 'LOG_BEGIN\n'; cat "$log"; printf 'LOG_END\n'
