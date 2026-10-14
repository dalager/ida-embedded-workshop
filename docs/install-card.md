# Install card — plugin + toolchain

Repo: `https://github.com/dalager/ida-embedded-workshop`. The plugin is six plain-Markdown skills plus a
starter template; it needs nothing Claude-specific and works in any Agent-Skills-aware tool.

## 1. Install the plugin in your AI tool

**Claude Code**

```
/plugin marketplace add dalager/ida-embedded-workshop
/plugin install ida-embedded-workshop@ida-embedded-workshop
```

Then `/reload-plugins` (or start a new session). Check with `/plugin` → Installed.

**Codex**

```bash
codex plugin marketplace add dalager/ida-embedded-workshop
codex plugin add ida-embedded-workshop@ida-embedded-workshop
```

Start a new session. `codex plugin marketplace upgrade ida-embedded-workshop` fetches updates.

**GitHub Copilot CLI**

```bash
copilot plugin marketplace add dalager/ida-embedded-workshop
copilot plugin install ida-embedded-workshop@ida-embedded-workshop
copilot skill list        # should list the six ida-embedded-workshop skills
```

**Cursor**

Cursor → Settings → Plugins → *Install from repository* → `https://github.com/dalager/ida-embedded-workshop`.
Or use the symlink fallback below into `~/.cursor/skills`.

**Factory Droid**

```bash
droid plugin marketplace add dalager/ida-embedded-workshop
droid plugin install ida-embedded-workshop@ida-embedded-workshop --scope user
```

**Pi**

```bash
pi install https://github.com/dalager/ida-embedded-workshop
```

Then `/reload` in an open session; `/skill:scaffold-knob-firmware` invokes a skill explicitly.

**Universal fallback (any tool, including OpenCode, Kiro, Cline)**

```bash
git clone https://github.com/dalager/ida-embedded-workshop ~/ida-embedded-workshop
# pick the directory your tool scans; one or more of:
for d in ~/.agents/skills ~/.copilot/skills ~/.cursor/skills ~/.config/opencode/skills \
         ~/.kiro/skills ~/.claude/skills; do
  mkdir -p "$d" && ln -sfn ~/ida-embedded-workshop/skills/* "$d"/
done
```

Last resort: open `skills/<name>/SKILL.md` and paste it into the chat. The skills are written to
work that way.

**Verify:** ask your AI *"Which pins does the knob use on the JC3636K518C?"* The answer must be
GPIO 8 and 7 and mention that it is not a quadrature encoder. If it guesses, the skill did not load.

## 2. Toolchain

**Docker (build)**

```bash
docker pull espressif/idf:release-v5.3     # ~4.5 GB, do this before the workshop
docker run --rm espressif/idf:release-v5.3 idf.py --version
```

The template's `scripts/build.sh` runs `idf.py build` in this image. You never need a native
ESP-IDF install. The container cannot see USB; it only builds.

**esptool (flash, on the host)**

```bash
pip install --upgrade esptool      # or pipx install esptool
esptool version
```

**Linux serial permissions**

```bash
sudo usermod -aG dialout "$USER"   # log out and in again
# or a udev rule:
echo 'SUBSYSTEM=="tty", ATTRS{idVendor}=="303a", MODE="0666"' | sudo tee /etc/udev/rules.d/99-esp.rules
sudo udevadm control --reload-rules
```

Port is `/dev/ttyACM0` when the S3 side of the USB plug is up (`lsusb` shows `303a:1001`).
If you see `1a86:7523` / `/dev/ttyUSB0`, flip the USB-C plug.

**macOS**

No driver needed for the S3 (`/dev/cu.usbmodem*`). Docker Desktop builds fine; flash from the
host terminal. If the port is missing, `system_profiler SPUSBDataType | grep -A3 303a`.

**Windows**

- Use Docker Desktop (WSL 2 backend) for the build; flash from a PowerShell on the host.
- Windows 10 usually needs the Espressif USB JTAG/serial driver: install it with Zadig
  (select the `USB JTAG/serial debug unit` interface → WinUSB) or the Espressif driver package.
- Port is `COMx`; pass `--port COM5` to esptool.
- Never try to pass the USB device into the container. Build in Docker, flash on the host.

## 3. Smoke test (five minutes, before the first exercise)

```bash
git clone https://github.com/dalager/ida-embedded-workshop
cp -r ida-embedded-workshop/templates/knob-starter ~/my-knob && cd ~/my-knob
cp components/net/include/wifi_secrets.example.h components/net/include/wifi_secrets.h
scripts/build.sh             # Docker; first run downloads managed components
scripts/test.sh              # host unit tests, gcc + Unity, no hardware
scripts/flash.sh /dev/ttyACM0
```

A teal screen with a label means you are ready for rung 1.
