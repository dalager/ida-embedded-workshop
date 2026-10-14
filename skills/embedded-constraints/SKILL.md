---
name: embedded-constraints
description: >
  Working within the limits of the ESP32-S3 knob board: where memory actually runs out
  (internal SRAM, not PSRAM), why the round display is bandwidth-bound, how to lay out tasks so
  LVGL never stalls, and how to measure before optimising. Use whenever firmware is designed,
  when something crashes with out-of-memory, StoreProhibited, a task watchdog or a stuck UI,
  when a feature is "too slow", or when an assistant is about to write code for this board.
  Includes the AI-specific pitfalls seen on real projects (invented pins and component names,
  LVGL 8 API in an LVGL 9 project, guessed panel init sequences, non-existent CONFIG symbols)
  and a verification routine to run before trusting generated code.
---

# Embedded constraints on the ESP32-S3 knob board

The reference firmware (a production recorder on this exact board) hit every limit below at
least once. The numbers are from its logs, not from datasheets.

## 1. Memory: internal SRAM is the scarce resource

| Pool | Size | Who needs it |
|---|---|---|
| Internal SRAM | ~512 KB total, ~300 KB usable heap after Wi-Fi + Bluetooth stacks and task stacks | DMA buffers, lwIP, mbedTLS, FreeRTOS stacks, anything latency-critical |
| Octal PSRAM | 8 MB at 80 MHz | Everything that tolerates ~2× latency: LVGL object pool, big BSS, file chunks, audio rings |
| Flash | 16 MB | App 9 MB + spiffs 4 MB in the starter partition table |

Rules:

- **Put big, cold things in PSRAM.** `heap_caps_malloc(n, MALLOC_CAP_SPIRAM)` for buffers over a
  few KB. Let large static arrays go there too: `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y`
  plus `EXT_RAM_BSS_ATTR` (or LVGL's `LV_ATTRIBUTE_LARGE_RAM_ARRAY`).
- **Keep DMA and hot things internal.** The LVGL draw buffers (`buff_spiram = false`,
  `buff_dma = true`), SD/I2S DMA descriptors, and anything touched every millisecond.
- **The lesson that cost a day:** LVGL's 128 KB object pool was in internal SRAM. During an
  HTTPS upload the log showed **8 KB free, largest block 1.5 KB** and lwIP's TCP send buffer
  could not allocate. Moving the pool to PSRAM freed it. Networking and the UI compete for the
  same internal heap; budget them together.
- **No malloc in hot paths.** Allocate at init, reuse. One shared chunk buffer for streaming, not
  one per request.
- **Stacks are internal RAM.** Size tasks deliberately (4–6 KB for a worker, 8 KB if it does TLS
  through `esp_http_client`, more if it `printf`s floats). Check with
  `uxTaskGetStackHighWaterMark()`.

## 2. The display is bandwidth-bound, not RAM-bound

360×360×2 B = 253 KB per frame over a QSPI bus at 80 MHz. A full-frame redraw lands around
25–40 fps on a good day; with other SPI traffic it drops.

- **Draw buffers:** 40 rows × 360 × 2 B ≈ 28.8 KB, two of them, internal DMA RAM, so LVGL renders
  the next dirty region while the previous one flushes. Bigger buffers do not help; PSRAM
  buffers make it worse.
- **Partial rendering.** Design for localized change: an arc that moves, a number that updates,
  a ring that pulses. Never animate the whole background.
- **No TE line on this board** → no vsync → tearing on full-screen motion. Live with it or keep
  motion local.
- **Compile at `-O2`** (`CONFIG_COMPILER_OPTIMIZATION_PERF=y`) and run at **240 MHz**. Both are
  measurable on software-rendered LVGL.
- **Round canvas.** The corners are clipped; the usable circle is 340 px. Radial layouts beat
  grids.

## 3. Tasks: never block the LVGL task

`esp_lvgl_port` owns the LVGL tick, timer handler and flush, and a mutex. Everything that
touches LVGL objects runs either inside that task (timers, event callbacks) or under
`ui_lock(timeout)` / `ui_unlock()` from another task.

- **No I/O, no TLS, no SD, no `vTaskDelay` while holding the LVGL lock.** The reference firmware
  hit the task watchdog when a task spun in `lv_malloc` while the port task held the lock
  during an upload.
- **One network task**, its own stack, results posted back to the UI through a callback or a
  queue. Strings passed to a callback are valid only during the call; copy them.
- **The knob is polled on a 3 ms `esp_timer`** and decoded by pure code; steps accumulate in an
  atomic and the LVGL task drains them. No per-detent interrupts.
- **FreeRTOS tick at 1000 Hz** so `vTaskDelay(pdMS_TO_TICKS(3))` means 3 ms.
- **Watchdogs are your friends.** Leave the task WDT on; a trip tells you which task hogged
  the CPU.

## 4. Power (only matters on battery)

- The backlight is the dominant draw; dim on idle.
- The sidecar ESP32 runs its Bluetooth stack unless reflashed: tens of mA, irrelevant on USB.
- Wi-Fi modem sleep is on by default in station mode; leave it unless latency matters.

## 5. Measure before optimising

```c
ESP_LOGI(TAG, "heap[%s]: free-int=%u min-int=%u free-psram=%u", phase,
         heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
         heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
         heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
```

Log that at named phases: after init, before the first TLS session, after each transfer. The
**minimum-free-internal** number is the one that decides whether a feature fits.

- `idf.py size-components` for flash and static RAM per component.
- `heap_caps_print_heap_info(MALLOC_CAP_INTERNAL)` when fragmentation is suspected (small
  "free" but tiny "largest block" is fragmentation).
- `esp_timer_get_time()` around a suspect section; log milliseconds.
- `lv_obj_get_child_count` and the LVGL memory monitor when the object pool grows.

## 6. AI pitfalls seen on this board

These are the mistakes generated code made on real projects here. Check for them explicitly.

| Pitfall | What it looked like | Guard |
|---|---|---|
| **Invented pin numbers** | Pins from the K718C, from a Waveshare *W*-series board, from an ESPHome post for a different PCB | Pins exist only as `BOARD_*` macros in one header. Run `scripts/check-board-pins.sh`; it fails the build on any GPIO literal elsewhere. |
| **Invented components** | `espressif/knob` for the rotary (reads zero: not quadrature), `esp_lcd_gc9a01` for an ST77916 panel, `esp_lcd_touch_cst816d` (the driver is `cst816s`, it covers the D) | Only add managed components that appear in `idf_component.yml`; look the name up on components.espressif.com before adding one. |
| **LVGL 9.2 idioms on LVGL 9.6** | `lv_obj_add_flag/remove_flag` (deprecated), `CONFIG_LV_MEM_SIZE_KILOBYTES` (ignored; the pool silently falls back to 64 KB) | The lock pins lvgl 9.6.0. Use `lv_obj_set_hidden/clickable/scrollable` and `CONFIG_LV_MEM_SIZE=131072`. Treat every deprecation warning as a bug. |
| **LVGL 8 API in an LVGL 9 project** | `lv_disp_drv_t`, `lv_obj_set_style_local_*`, `lv_btn_*`, `LV_STATE_PRESSED` as a style selector arg | The template pins LVGL ^9.2. Check the symbol exists in `managed_components/lvgl__lvgl/src/` before using it. |
| **Guessed panel init sequences** | A plausible ST7789 table, or an extra `esp_lcd_panel_invert_color(true)` on top of the table's `0x21` | Use `ui_panel_init_cmds.c` unchanged. Invert once. |
| **Non-existent `CONFIG_` symbols** | `CONFIG_LVGL_...` (the real prefix is `CONFIG_LV_`), `CONFIG_SPIRAM_USE_MALLOC` variations from other IDF versions | Grep `sdkconfig` after a build; an unknown symbol in `sdkconfig.defaults` is silently ignored. |
| **Quadrature assumptions** | PCNT setup, `rotary_encoder` component | The knob is two switches. Use `knob_decode.c`. |
| **Driving GPIO0** | "Set the I2S mux to the S3 side" | Never. The on-board pull-up already selects the S3. |
| **Blocking inside LVGL callbacks** | HTTP GET inside a button event | Post to the network task; update the UI from a callback under the lock. |
| **Whole-file buffers** | `malloc(file_size)` before upload | Stream through one 16 KB chunk; see `networking-under-memory-constraints`. |
| **Wrong IDF version idioms** | Legacy `driver/i2c.h` instead of `driver/i2c_master.h`; `esp_netif_sntp` missing on old IDF | The template is IDF 5.3; use the new I2C master driver and LEDC APIs as written there. |

### Verification routine (run before trusting generated code)

1. `scripts/check-board-pins.sh` — no GPIO literals outside `components/board`.
2. `grep -rn "GPIO_NUM_0\b\|gpio_num = 0\b" components main` — nothing drives GPIO0.
3. `idf.py build` in Docker — zero warnings in your own components.
4. `scripts/test.sh` — host tests pass for every pure module you touched.
5. Boot log: no `E (` lines, I2C scan shows 0x15 and 0x5A, heap log shows internal
   minimum-free above your floor.
6. Every managed component you added appears in `dependencies.lock` with a real version.

## 7. Quick reference numbers

| Thing | Value |
|---|---|
| CPU | 2× Xtensa LX7, 240 MHz |
| Internal heap after Wi-Fi init | roughly 250–300 KB; log it |
| PSRAM | 8 MB octal, 80 MHz |
| LVGL draw buffer | 2 × 28.8 KB internal (40 rows) |
| LVGL object pool | 128 KB, in PSRAM |
| Panel clock | 80 MHz QSPI |
| Knob poll | 3 ms, debounce 2 samples (~6 ms) |
| TCP send/receive window | 16 KB each (`CONFIG_LWIP_TCP_SND_BUF_DEFAULT`, `_WND_DEFAULT`) |
| Streaming chunk | 16 KB in PSRAM |
| FreeRTOS tick | 1000 Hz |
