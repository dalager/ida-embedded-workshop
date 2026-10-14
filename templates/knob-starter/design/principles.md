# Design principles for the round knob display

The tokens in `tokens.json` / `tokens.h` are the values. These are the rules they serve.

- **Colour = state.** One status colour per screen, on the accent ring and on-screen accents:
  red = active/recording/error, teal = idle, amber = paused/caution, blue = in progress,
  green = done/success. Never use a status colour for decoration.
- **The ring is a trust device.** When the device is doing something the user should know about
  (recording, sending), the ring says so and nothing else may hide it.
- **Two typefaces, strict roles.** A humanist sans (Muli) for human labels; a mono (JetBrains Mono,
  Fira Code) for machine data: timestamps, filenames, codes, status tags.
- **Dark, low-saturation, physical.** Near-black warm-neutral surfaces so the one accent colour
  dominates. RGB565 will band on gradients — keep surfaces flat or dither.
- **Round-native layout.** Everything inside the **340 px safe circle** (`HUD_SAFE_DIA`); nothing
  hugs the bezel. The knob is the primary input, touch confirms.
- **Calm, purposeful motion.** Breathing (idle) → fast pulse (arming) → solid (active) → spin
  (in progress) → blink (error). Speed signals urgency; periods are in `tokens.h` (`HUD_MS_*`).
- **Restraint over decoration.** Low information density so the single active state reads at arm's
  length.
