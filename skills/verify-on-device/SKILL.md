---
name: verify-on-device
description: >
  Close the loop on the real JC3636K518C knob: drive the firmware from the laptop through a
  serial test console (inject knob detents, taps, holds; read the model state), pull a
  pixel-exact LVGL screenshot over USB, and look at the physical panel through a webcam. Use
  after every flash, in any build → flash → look iteration, when asked "can you see the
  screen", "verify on the device", "cam verify", "take a screenshot of the device", "walk
  through the screens", or when a change must be proven working and not just compiled. The
  starter template ships the console (components/testctl) and the host scripts.
---

# Verify on the device, not in your head

Three sources of truth, each answering a different question. Use all three; they disagree in
useful ways.

| Source | Answers | Misses |
|---|---|---|
| `scripts/testctl.py state` | What the app model thinks (screen, counter, state) | Anything about pixels |
| `scripts/testctl.py snap` | What LVGL rendered, pixel-exact, no blur | A panel that never got the frame |
| webcam `capture.sh` | What a human sees: backlight, colour, inversion, a black panel | Small text (it is soft) |

A bug you have already met: a 120 px font blanked the panel with a clean boot log and a
"ready" line. Only the camera caught it. A bug you will meet: a layout is off by 10 px. Only
the snapshot shows it. The model line tells you which half (logic or rendering) is wrong.

## 1. The serial test console (in the template)

`components/testctl/` is a dev-only console on the USB Serial/JTAG port, the same port as the
log, gated by `CONFIG_KNOB_TESTCTL` (default on; turn it off for a production image). It
injects events through the app's **real** handlers under the LVGL lock, so there is no second
code path to drift. One command per line, every reply starts with `> `:

```
knob +3 | knob -2     inject detents (sign = direction)        > state=idle counter=3
tap                   short tap                                > state=active counter=3
hold                  long press                               > state=uploading counter=3
reset                 boot state                               > state=idle counter=0
state                 report only
snap                  framebuffer: SNAP BEGIN w h RGB565 / S:<base64 row>… / SNAP END
ping                  > pong
```

The host wrapper hides the protocol:

```bash
scripts/testctl.py state
scripts/testctl.py knob +3
scripts/testctl.py tap
scripts/testctl.py snap out.png        # PNG via ffmpeg
scripts/verify-screens.sh [out_dir]    # scripted walk: assert model, snap, camera; exit 1 on mismatch
```

Rules the console follows, keep them when you extend it:

- **Inject on the LVGL task or under `ui_lock()`**: the handlers touch widgets.
- **Expose hooks, not internals**: `app_inject_knob/tap/hold/reset` and `app_state_line` in
  `app.h`. Add a hook per new input; never let `testctl` reach into the app's statics.
- **Replies are one line, machine-readable, prefixed `> `**, so a test can grep them out of
  the interleaved log.
- **The snapshot is one `fwrite` per row with logging muted** (`esp_log_level_set("*", NONE)`
  around the dump). The first version lost rows to a Wi-Fi "reconnecting" line landing inside
  a base64 row.
- **Wait for animations before snapping** (`KNOB_SETTLE`, 300 ms). Forty injected detents
  under one lock start one easing animation; snap too early and the arc is half drawn.
- **Make the walk idempotent**: start with `reset`. The second run of a walk that assumed a
  fresh boot failed on carried-over state.

Only one process may hold `/dev/ttyACM0`. `testctl.py` opens it per call; a `miniterm` left
open swallows the replies.

## 2. The camera

`scripts/capture.sh` (in this skill's directory) takes one exposure-corrected, cropped frame:

```bash
<skill-dir>/scripts/capture.sh out.jpg                     # then look at out.jpg
EXPOSURE=70 <skill-dir>/scripts/capture.sh out.jpg         # bright, full-colour screens
CROP=1920:1080:0:0 <skill-dir>/scripts/capture.sh full.jpg # find the panel after moving the camera
```

- **Manual exposure is mandatory.** A bright round panel in a dim room makes auto-exposure
  render a white disc. `set_exposure.py` forces manual mode through a raw V4L2 ioctl (no
  `v4l2-ctl` needed); it sticks until the camera is unplugged. 250 suits the dark token
  theme, 50–70 a full-colour background. White disc → lower; black frame → raise, then check
  the backlight line in the boot log.
- **Settle 1 s** after changing exposure or the previous setting leaks into the frame.
- **Crop** is the panel's position in the frame; re-measure with the full-frame capture when
  anything moves. `ROTATE=180` if the camera hangs upside down.
- **Describe what is actually there**: background colour, every text, its position. Never
  claim a number you cannot read; say it is unreadable and change the exposure.

## 3. The loop

1. Edit → `scripts/build.sh` → `scripts/flash.sh /dev/ttyACM0 app`.
2. Wait for `app: ready` (or ~4 s), then `scripts/testctl.py ping`.
3. `scripts/verify-screens.sh out/` for the scripted states; or hand-drive with `knob`/`tap`
   and `snap` + `capture.sh` per state.
4. Compare each snapshot with what the code should draw; compare the camera with the
   snapshot. State plainly which source disagrees and what differs: wrong colour, missing
   label, clipped text, black panel.
5. Extend `verify-screens.sh` whenever you add a state. A state that is not in the walk is
   not verified.

Six iterations is a sensible budget for a design loop; if a screen still is not right, the
plan is wrong, not the pixels.

## 4. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `no reply from /dev/ttyACM0` | Another reader holds the port; or console is not on USB (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`) |
| `corrupt row N (log noise…)` | A write slipped into the stream; snapshot code must mute logs and write rows atomically |
| Snapshot shows a half-drawn arc | Snapped mid-animation; raise `KNOB_SETTLE` |
| Walk passes once, fails on rerun | It assumes boot state; start with `reset` |
| Camera: white disc / black frame | Exposure too high / too low (or backlight off) |
| Camera: panel off-centre | Re-measure `CROP` from a full-frame capture |
| Snapshot fine, camera black | Rendering never reached the panel: font, flush, backlight. Trust the camera |
