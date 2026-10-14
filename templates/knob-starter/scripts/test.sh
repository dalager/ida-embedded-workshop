#!/usr/bin/env bash
# Host unit tests for the portable C modules — no ESP-IDF, no hardware, no cmake. Each suite is
# compiled with gcc + the vendored Unity (test/host/unity) and run. Then the board-pin lint.
# The rule that makes this possible: pure logic lives in its own .c with no ESP-IDF includes.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HOST="$ROOT/test/host"
UNITY="$HOST/unity"
BUILD="$HOST/build"
mkdir -p "$BUILD"

CC="${CC:-gcc}"
CFLAGS="-std=c11 -Wall -Wextra -O0 -g -I$UNITY"

fail=0
run_suite() {
  local name="$1"; shift
  echo "── $name ──────────────────────────────────────────────"
  if "$CC" $CFLAGS "$@" "$UNITY/unity.c" -o "$BUILD/$name" -lm; then
    "$BUILD/$name" || fail=1
  else
    echo "BUILD FAILED: $name"; fail=1
  fi
  echo
}

# Board definition: unique, usable GPIOs + capability consistency (stubs/sdkconfig.h picks the board).
run_suite test_board \
  "$HOST/test_board.c" \
  -I"$HOST/stubs" -I"$ROOT/components/board/include"

# Rotary knob detent decoder: single-pulse direction + return-to-idle debounce.
run_suite test_knob_decode \
  "$HOST/test_knob_decode.c" \
  "$ROOT/components/ui/knob_decode.c" \
  -I"$ROOT/components/ui/include"

# Theme state->colour/motion wiring (stub tokens header with sentinels).
run_suite test_theme \
  "$HOST/test_theme.c" \
  "$ROOT/components/theme/theme.c" \
  -I"$HOST/stubs" -I"$ROOT/components/theme/include"

# Chunked-upload part-plan math: equal non-last parts + remainder last.
run_suite test_upload_plan \
  "$HOST/test_upload_plan.c" \
  "$ROOT/components/net/upload_plan.c" \
  -I"$ROOT/components/net/include"

echo "── board pin lint ──────────────────────────────────────────"
bash "$ROOT/scripts/check-board-pins.sh" || fail=1
echo

if [ "$fail" -ne 0 ]; then echo "HOST TESTS FAILED"; exit 1; fi
echo "ALL HOST TESTS PASSED"
