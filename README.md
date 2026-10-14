# IDA Embedded Workshop

Build small ESP-IDF + LVGL firmware for the **Guition JC3636K518C** 1.8" round knob display
(ESP32-S3, 360×360 ST77916 panel, CST816 touch, rotary knob, haptic, Wi-Fi) with **any AI coding
tool**. The plugin is a set of plain-Markdown skills plus a tested starter template, distilled
from a production firmware on the same board. Nothing in it depends on one AI vendor.

| Skill | Load it when |
|---|---|
| `jc3636k518-device` | You need pins, what the board has and lacks, flashing, the gotchas |
| `embedded-constraints` | Memory, performance, tasks, and the mistakes AI tools make on ESP32 |
| `firmware-code-organisation` | Keeping files small, components cohesive, pure logic host-tested |
| `scaffold-knob-firmware` | Starting a new project from the template, first flash, exercise rungs 1–6 |
| `networking-under-memory-constraints` | Wi-Fi, HTTP, streaming uploads without exhausting internal RAM, rungs 7–10 |
| `backend-integration-patterns` | You need a backend: pairing, tokens, resumable upload, contract-first API |
| `round-display-ux-navigation` | Adding screens, lists, details, back, idle timeout; swipe + knob navigation on a round panel |

`templates/knob-starter/` is the firmware the scaffold skill copies: board header, display and
touch bring-up, knob decoder, theme tokens, haptic, a trimmed networking component with a
streamed PUT, a tiny app, host tests with Unity, Docker build and host flash scripts.
`templates/receiver/receiver.py` is a one-file HTTP receiver so the upload exercise needs no
backend.

## Install

**Claude Code**

```
/plugin marketplace add dalager/ida-embedded-workshop
/plugin install ida-embedded-workshop@ida-embedded-workshop
```

**Codex**

```bash
codex plugin marketplace add dalager/ida-embedded-workshop
codex plugin add ida-embedded-workshop@ida-embedded-workshop
```

**GitHub Copilot CLI**

```bash
copilot plugin marketplace add dalager/ida-embedded-workshop
copilot plugin install ida-embedded-workshop@ida-embedded-workshop
```

**Factory Droid**

```bash
droid plugin marketplace add dalager/ida-embedded-workshop
droid plugin install ida-embedded-workshop@ida-embedded-workshop --scope user
```

**Pi**

```bash
pi install https://github.com/dalager/ida-embedded-workshop
```

**Cursor, Kiro, OpenCode, Cline, or anything that reads Agent Skills**

```bash
git clone https://github.com/dalager/ida-embedded-workshop ~/ida-embedded-workshop
for d in ~/.agents/skills ~/.cursor/skills ~/.kiro/skills ~/.config/opencode/skills ~/.cline/skills ~/.copilot/skills ~/.claude/skills; do
  mkdir -p "$d" && for s in ~/ida-embedded-workshop/skills/*/; do ln -sfn "$s" "$d/$(basename "$s")"; done
done
```

**No plugin support at all**: paste the relevant `skills/*/SKILL.md` into the chat. They are
written to work that way.

## Toolchain

- Docker with `espressif/idf:release-v5.3` pulled (`docker pull espressif/idf:release-v5.3`).
- `pip install esptool` on the host. Flashing happens on the host, never inside the container.
- A USB-C **data** cable. The board enumerates as `303a:1001` (ESP32-S3 USB JTAG/serial) in one
  plug orientation and as `1a86:7523` (the sidecar's CH340) in the other. Flip the plug for the S3.

## Quick start

```bash
cp -r ~/ida-embedded-workshop/templates/knob-starter my-knob && cd my-knob
cp components/net/include/wifi_secrets.example.h components/net/include/wifi_secrets.h   # edit it
./scripts/build.sh          # idf.py build inside Docker
./scripts/flash.sh          # host esptool --no-stub over /dev/ttyACM0
./scripts/test.sh           # gcc + Unity host tests, no IDF needed
python3 ~/ida-embedded-workshop/templates/receiver/receiver.py --port 8080   # for the upload exercise
```

Then ask your AI tool to take you up the exercise ladder in `skills/scaffold-knob-firmware`.

## Workshop material

The workshop site with agenda, device cheat-sheet, install card and exercise ladder is at
https://dalager.github.io/ida-embedded-workshop/.

## Licence

MIT for code and text. Fonts are SIL OFL 1.1. The panel init table derives from the Waveshare
demo for the same PCB. See `LICENSE`.
