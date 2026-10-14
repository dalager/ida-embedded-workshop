#!/usr/bin/env bash
# Grab one frame of the knob's screen from a webcam, cropped to the round panel.
#   capture.sh [out.jpg]
# Env (defaults suit a camera ~25 cm from the panel in a dim room, 1920x1080):
#   CAM=/dev/video0        EXPOSURE=250 (manual; lower = darker; 50-70 for bright full-colour screens)
#   ROTATE=0|180           CROP=w:h:x:y in the full frame (find it once with CROP=1920:1080:0:0)
# Prints the output path on success. Needs ffmpeg + python3; no v4l2-ctl required.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${1:-${TMPDIR:-/tmp}/knob-cam.jpg}"
CAM="${CAM:-/dev/video0}"
EXPOSURE="${EXPOSURE:-250}"
ROTATE="${ROTATE:-0}"
CROP="${CROP:-600:600:740:50}"

[ -e "$CAM" ] || { echo "no camera at $CAM (ls /dev/video*)" >&2; exit 2; }
python3 "$HERE/set_exposure.py" "$CAM" "$EXPOSURE" >/dev/null

VF="crop=$CROP"
[ "$ROTATE" = "180" ] && VF="$VF,transpose=1,transpose=1"
RAW="$(mktemp --suffix=.jpg)"
# -ss 1: let the sensor settle on the new exposure before taking the frame.
ffmpeg -y -loglevel error -f v4l2 -i "$CAM" -ss 1 -frames:v 1 -update 1 "$RAW"
ffmpeg -y -loglevel error -i "$RAW" -vf "$VF" "$OUT"
rm -f "$RAW"
echo "$OUT"
