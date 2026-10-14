---
name: scaffold-knob-firmware
description: >
  Start a new firmware project for the JC3636K518C knob display from the tested starter
  template, get it built in Docker, flashed over USB and showing its first screen, then climb
  the exercise ladder (hello screen, knob counter, touch, colour = state, haptics, host test).
  Use when someone wants to begin a knob project, says "scaffold", "new project", "hello
  world", "first screen", "getting started", "how do I build this", or when a project needs the
  known-good build, flash and test scripts. Copy the template; do not generate the bring-up
  code from scratch.
---

# Scaffold a knob firmware from the starter template

The template at `templates/knob-starter/` is a complete ESP-IDF 5.3 project that has been built,
flashed and run on the board: display, touch, knob, backlight, theme, haptics, Wi-Fi, a streamed
HTTP upload, and host tests. Copy it, confirm it runs, then change one thing at a time.

## 0. Prerequisites (check, do not assume)

| Need | Check |
|---|---|
| Docker with the IDF image | `docker pull espressif/idf:release-v5.3` |
| esptool on the host | `pip install esptool` → `esptool version` |
| gcc for host tests | `gcc --version` |
| The board on the S3 side of USB | `lsusb \| grep 303a` and `ls /dev/ttyACM0` (flip the plug if you see `1a86:7523`) |
| Python 3 for the receiver | `python3 --version` |

No native ESP-IDF install is needed. Flashing always happens from the host, never from the
container.

## 1. Copy

```bash
cp -r templates/knob-starter ~/my-knob
cd ~/my-knob
cp components/net/include/wifi_secrets.example.h components/net/include/wifi_secrets.h
```

Edit `wifi_secrets.h`: `KNOB_WIFI_SSID`, `KNOB_WIFI_PASS`, `KNOB_RECEIVER_URL`
(`http://<your-laptop-ip>:8080`). It is gitignored; never commit it.

Layout:

```
components/board/   board_jc3636k518c.h — every pin, every capability flag
components/ui/      ui.c (panel + touch + LVGL port), ui_panel_init_cmds.c, knob.c,
                    knob_decode.c, backlight.c, fonts/
components/theme/   theme.c + tokens.h (colour = state)
components/haptic/  haptic.c (DRV2605L)
components/net/     net.c (Wi-Fi), net_http.c (GET + streamed PUT), upload_plan.c
components/app/     app.c — the state machine; edit this first
main/main.c         init order
design/             tokens.json, tokens.h, principles.md, fonts + licences
scripts/            build.sh, flash.sh, test.sh, check-board-pins.sh
test/host/          Unity + tests for the pure modules
```

## 2. Build

```bash
scripts/build.sh            # runs idf.py build inside espressif/idf:release-v5.3
```

First build downloads managed components (LVGL 9, esp_lcd_st77916, esp_lvgl_port,
esp_lcd_touch_cst816s) and takes several minutes. Output: `build/knob_starter.bin`. A clean
build has no warnings from `components/`.

## 3. Flash and watch it boot

```bash
scripts/flash.sh            # host esptool --no-stub, bootloader + partitions + app
scripts/flash.sh app        # app only, for later iterations
idf.py monitor -p /dev/ttyACM0   # or: python3 -m serial.tools.miniterm /dev/ttyACM0 115200
```

Expected boot log, in order: backlight on, panel init, I2C scan `0x15 0x5A`, knob up (poll
3000 µs), heap line, Wi-Fi connecting, app state IDLE. The screen shows the template's hello
screen in the idle colour.

If the panel stays dark: backlight (GPIO47) not driven, or the wrong USB side. If colours are
wrong: an extra invert. If the knob counts backwards: flip `BOARD_KNOB_DIRECTION`.

## 4. Run the host tests

```bash
scripts/test.sh
```

Compiles each `test/host/test_*.c` with gcc + Unity and runs it. All green before you change
anything, so a red test later means your change.

## 5. The API you build on

```c
// ui
void ui_init(void);                          // panel, touch, LVGL port, backlight, knob
i2c_master_bus_handle_t ui_i2c_bus(void);    // share the touch bus with the haptic driver
bool ui_lock(uint32_t timeout_ms);           // take the LVGL lock from another task
void ui_unlock(void);
void knob_init(void (*on_step)(int dir));    // dir = +1 / -1 per detent, called on the LVGL task
void backlight_init(void);
void backlight_set_percent(uint8_t pct);
// theme
uint32_t theme_state_color(theme_state_t s); // 0xRRGGBB; use lv_color_hex()
uint32_t theme_state_motion_ms(theme_state_t s);
// haptic
void haptic_init(i2c_master_bus_handle_t bus);
void haptic_click(void);
// net (see the networking skill)
void net_init(void);  bool net_is_connected(void);  int net_rssi(void);
bool net_wifi_join(const char *ssid, const char *pass);
int  net_http_get(const char *url, char *buf, size_t cap);
bool net_http_put_stream(const char *url, uint64_t total_len,
                         void (*rewind_cb)(void *ctx),            // called before EVERY attempt
                         size_t (*read_cb)(void *ctx, uint8_t *dst, size_t cap), void *ctx,
                         void (*progress_cb)(uint64_t sent, uint64_t total));
// app
void app_init(void);                         // starts the state machine
```

`main.c` calls, in this order: NVS init → `ui_init()` → `haptic_init(ui_i2c_bus())` →
`net_init()` → `app_init()`. Keep that order.

## 6. The exercise ladder (UI half)

Each rung is one small change, built, flashed and seen. Ask the assistant for one rung at a
time; commit after each.

1. **Hello screen.** In `app.c`, change the label text and put it in the 28 px Muli font.
   Keep it inside the 340 px safe circle (`lv_obj_center`). Build, flash, look.
2. **Knob counter.** Register `knob_init(on_step)`; keep an `int` and update a label. Verify
   one detent = one count, both directions; check the direction macro if reversed.
3. **Touch target.** Add an `lv_obj` with `LV_EVENT_CLICKED`; tapping toggles a `bool`. Keep the
   handler tiny: set state, call a `render()` function.
4. **Colour = state.** Introduce `enum app_state { IDLE, ACTIVE }`, map it through
   `theme_state_color()`, and tint the background or an arc. Use `theme_state_motion_ms()` to
   pulse an arc with `lv_anim` while ACTIVE. Motion stays local to the arc.
5. **Haptic cue.** `haptic_click()` on every state change, once. If it clicks twice, your
   state changes twice.
6. **Host test.** Move the "what happens on step / tap" decision into a pure `app_logic.c`
   (state + event → new state), add `test/host/test_app_logic.c`, run `scripts/test.sh`.

Then continue with rungs 7–10 in the `networking-under-memory-constraints` skill.

## 7. Working rhythm with an assistant

- Give it the board skill and this one. Ask for one rung, then build and flash yourself.
- When it proposes a pin, a component or a `CONFIG_` symbol, ask where it came from; the
  answer must be the board header, `idf_component.yml`, or the IDF docs for 5.3.
- When it proposes a new file, ask which component owns it and what its header says.
- Before accepting a change, run the done checklist in `firmware-code-organisation`.

## 8. Troubleshooting

| Symptom | Likely cause |
|---|---|
| `Packet content transfer stopped` | Missing `--no-stub` |
| No `/dev/ttyACM0` | Wrong plug orientation, charge-only cable, or app holding USB: hold BOOT while plugging in |
| Panel dark, log fine | Backlight not driven; check `backlight_init()` ran |
| Colours inverted | `esp_lcd_panel_invert_color()` called on top of the init table |
| Knob reads zero | A quadrature decoder was introduced; restore `knob_decode.c` |
| Task watchdog on `lvgl_port` | Blocking call inside an LVGL callback or while holding the lock |
| `Out of buffer` from HTTP client | Long URL: raise `buffer_size_tx` to 2048 |
| Build fails on a `lv_*` symbol | LVGL 8 API; look the LVGL 9 name up in `managed_components/lvgl__lvgl` |
