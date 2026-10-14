#!/usr/bin/env bash
# Lint: every GPIO number lives in components/board/include/board_*.h. Fails if a component
# or main/ assigns a numeric literal to a pin field or defines a GPIO constant from a number.
set -euo pipefail
FW="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PATTERN='GPIO_NUM_[0-9]+|#define[[:space:]]+(PIN_(LCD|TOUCH|KNOB|SD|MIC)|SD_(CLK|CMD|D[0-3])|MIC_)[A-Z0-9_]*[[:space:]]+\(?-?[0-9]+|#define[[:space:]]+[A-Z0-9_]*GPIO[A-Z0-9_]*[[:space:]]+\(?-?[0-9]+|\.(clk|cmd|d[0-7]|din|dout|bclk|ws|mclk|gpio_num|sda_io_num|scl_io_num|strip_gpio_num|rst_gpio_num|int_gpio_num|reset_gpio_num)[[:space:]]*=[[:space:]]*-?[0-9]+'
if hits="$(grep -rnE "$PATTERN" --include='*.c' --include='*.h' "$FW/components" "$FW/main" \
             | grep -v "$FW/components/board/")"; then
  echo "GPIO literals outside components/board (move them into a board_*.h):"
  echo "$hits"
  exit 1
fi
echo "board pins: no GPIO literals outside components/board"
