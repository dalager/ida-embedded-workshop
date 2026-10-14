# Workshop agenda — AI-built embedded on the round knob

**Format:** 3.5 hours, hands-on, one Guition JC3636K518C knob per participant (or per pair).
**Goal:** leave with a firmware you built with your own AI tool that shows a screen in the design
language, reacts to the knob and touch, buzzes, joins Wi-Fi and streams megabytes to an HTTP
endpoint without running out of RAM.

## Bring / preinstall (before you arrive)

| Item | Why |
|---|---|
| Laptop with **Docker** running (`docker pull espressif/idf:release-v5.3`, ~4.5 GB) | Builds run in the ESP-IDF container |
| **git** | Clone the plugin repo and the starter template |
| **Python 3** with `pip install esptool` | Flashing runs on the host, never in the container |
| **An AI coding tool** you already use (Claude Code, Codex, Copilot CLI, Cursor, Droid, Pi, OpenCode, Kiro …) | "Bring your own AI" |
| **USB-C data cable** (not charge-only) | The knob enumerates as USB serial |
| Linux: your user in the `dialout` group (or a udev rule) | Serial port access |
| Windows: Espressif USB JTAG/serial driver (or Zadig) | See the install card |

Check before the day: `docker run --rm espressif/idf:release-v5.3 idf.py --version` prints a
5.3.x version, and `esptool version` works.

## Timetable

| Time | Block | What happens |
|---|---|---|
| 00:00 – 00:15 | **Intro + why the knob** | What the device is, what you will build, how the day runs. Why a round 360×360 display with a knob is a good teaching target: rich enough to be fun, constrained enough that the AI cannot bluff its way through. |
| 00:15 – 00:45 | **The reference firmware walkthrough** (worked example) | A production firmware for the same board, shown not shipped. Four ideas to steal: **one contract, two targets** (one schema emits TypeScript and a C header); **pins live in one header** and a lint fails the build on any GPIO literal elsewhere; **pure logic is host-tested** with gcc + Unity, no hardware; and **the lessons that cost real time** (internal RAM starvation during upload, the knob that is not a quadrature encoder, GPIO0 triple-booked, a 30-day token with no refresh). |
| 00:45 – 01:05 | **Tool setup + plugin install** | Install the workshop plugin into your AI tool (see `INSTALL_CARD.md`). Verify the skills load. Plug in the knob, find the port, confirm the toolchain. |
| 01:05 – 01:35 | **Scaffold + first flash** | Ask your AI to scaffold from the starter template. Build in Docker, flash from the host with `esptool --no-stub`, see the first screen. Run the host tests. |
| 01:35 – 01:45 | **Break** | |
| 01:45 – 02:45 | **Exercise ladder, rungs 1–6** (UI half) | Hello screen → knob counter → touch target → colour = state → haptic cue → host test for your pure logic. See `EXERCISE_LADDER.md`. |
| 02:45 – 03:30 | **Exercise ladder, rungs 7–10** (network half) | Join Wi-Fi and show RSSI → GET JSON and render it → stream ≥ 2 MB to the reference receiver with a progress arc while watching the heap log → survive a Wi-Fi drop mid-transfer. |
| 03:30 – 03:50 | **Show-and-tell + wrap** | Two minutes per table: what you built, what the AI got wrong, what the guardrails caught. What to take home. |

## Ground rules for the day

- **The AI writes the code; you own the decisions.** Read what it produces before you build it.
- **The board header is the only source of pin numbers.** If your AI invents one, that is a
  teaching moment, not a bug to live with.
- **Build before you believe.** A green Docker build and green host tests are the only proof.
- **Flash from the host.** The container has no serial access. Ever.
- **Small files, one concern each.** If a file passes 300 lines, split it before adding more.

## Facilitator notes

- Have two spare knobs and two spare USB-C cables.
- Pre-pull the Docker image on a USB stick (`docker save`) for slow Wi-Fi venues.
- Run the reference receiver on the facilitator laptop as a fallback target for rungs 9–10 and
  put its LAN IP on the board.
- A dedicated workshop Wi-Fi (2.4 GHz, WPA2, no captive portal) saves twenty minutes.
