#!/usr/bin/env bash
# Regenerate the LVGL bitmap font(s) in components/ui/fonts/ from design/fonts/. Needs node.
# Latin-1 supplement (0x00C0-0x00FF) covers æøåÆØÅ and accents. 4 bpp = smooth edges at 28 px.
# Add a size or face here, declare it in fonts/fonts.h and list the .c in components/ui/CMakeLists.txt.
set -euo pipefail
cd "$(dirname "$0")/.."
npx --yes lv_font_conv --font design/fonts/Muli-SemiBold.ttf \
  --size 28 --bpp 4 --format lvgl \
  --range 0x20-0x7E,0x00C0-0x00FF \
  --force-fast-kern-format --no-compress \
  -o components/ui/fonts/font_muli_28.c
