#!/usr/bin/env bash
# Walk the app's states through the serial test console (components/testctl) and record what the
# device shows: the LVGL framebuffer (snap) and, if a camera script is given, the webcam. One
# directory of evidence per run; exits 1 on a model mismatch. Extend the list when you add a state.
#   scripts/verify-screens.sh [out_dir]
#   KNOB_CAM=/path/to/verify-on-device/scripts/capture.sh   # optional webcam capture per step
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$ROOT/build/verify}"
TC="$ROOT/scripts/testctl.py"
CAM="${KNOB_CAM:-}"
mkdir -p "$OUT"
fail=0

step() {   # step <name> <command...> ; EXPECT holds the expected reply substring
  local name="$1"; shift
  local reply; reply="$(python3 "$TC" "$@")" || { echo "FAIL $name: $reply"; fail=1; return; }
  if [[ "$reply" != *"$EXPECT"* ]]; then echo "FAIL $name: got '$reply', want '$EXPECT'"; fail=1; else echo "ok   $name: $reply"; fi
  python3 "$TC" snap "$OUT/$name.png" >/dev/null
  if [ -n "$CAM" ] && [ -x "$CAM" ]; then "$CAM" "$OUT/$name.cam.jpg" >/dev/null; fi
}

EXPECT="pong"                       python3 "$TC" ping >/dev/null || { echo "device not answering on the test console"; exit 1; }
EXPECT="state=idle counter=0"       step 00-reset        reset
EXPECT="state=idle counter=5"       step 01-knob-up      knob +5
EXPECT="state=idle counter=-2"      step 02-knob-down    knob -7
EXPECT="state=active counter=-2"    step 03-tap-active   tap
EXPECT="state=idle counter=-2"      step 04-tap-idle     tap

echo; echo "evidence in $OUT"; ls "$OUT"
[ "$fail" -eq 0 ] && echo "ALL SCREEN CHECKS PASSED" || { echo "SCREEN CHECKS FAILED"; exit 1; }
