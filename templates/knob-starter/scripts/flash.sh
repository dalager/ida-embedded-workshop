#!/usr/bin/env bash
# Flash from the HOST with esptool over the ESP32-S3's native USB (the Docker image has no serial
# access, so `idf.py flash` from the container does not work).
#   scripts/flash.sh [port] [app]
#     port  serial device, default /dev/ttyACM0 (the S3 enumerates as 303a:1001)
#     app   write only the app image at 0x10000 (after the first full flash)
# --no-stub is mandatory on this board's USB-Serial/JTAG; the flasher stub hangs. USB CDC ignores
# baud, so there is nothing to tune. The first flash must be the full set so the 16 MB partition
# table gets written.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${1:-/dev/ttyACM0}"
MODE="${2:-full}"
B="$ROOT/build"

if ! command -v esptool >/dev/null 2>&1; then
  echo "esptool not found: pip install esptool" >&2; exit 2
fi
if [ ! -e "$PORT" ]; then
  echo "$PORT not present. Plug a USB-C DATA cable into the knob. On the K518 the plug orientation" >&2
  echo "picks the chip: 303a:1001 (/dev/ttyACM0) is the S3 you want; 1a86:7523 (/dev/ttyUSB0) is" >&2
  echo "the Bluetooth sidecar — flip the plug. Still missing? hold BOOT while tapping RESET." >&2
  exit 2
fi
[ -f "$B/knob_starter.bin" ] || { echo "no build/knob_starter.bin — run scripts/build.sh first" >&2; exit 2; }

ARGS=(--chip esp32s3 --port "$PORT" --no-stub --before default_reset --after hard_reset
      write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m)
if [ "$MODE" = "app" ]; then
  esptool "${ARGS[@]}" 0x10000 "$B/knob_starter.bin"
else
  esptool "${ARGS[@]}" \
    0x0     "$B/bootloader/bootloader.bin" \
    0x8000  "$B/partition_table/partition-table.bin" \
    0x10000 "$B/knob_starter.bin"
fi
echo "done — 'Hash of data verified' + 'Hard resetting' above means the knob rebooted into the new image"
