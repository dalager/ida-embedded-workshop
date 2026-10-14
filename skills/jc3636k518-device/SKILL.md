---
name: jc3636k518-device
description: >
  Hardware reference for the Guition JC3636K518C 1.8" round knob display (also sold as the
  Waveshare ESP32-S3-Knob-Touch-LCD-1.8, same PCB). Use whenever firmware for this board is
  written, reviewed or debugged: pin numbers, which pins must never be driven, what the board
  does not have, which USB orientation reaches which chip, and how to flash it. Also use when
  someone says "the knob", "K518", "ST77916", "CST816", "which GPIO", "it won't flash", or
  "the display stays dark". Every pin here was verified on a real unit; do not substitute pins
  from memory, from the older K718C board, or from an ESPHome forum post.
---

# JC3636K518C — the board on the desk

A 1.8" round 360×360 IPS display with a rotary knob around it, driven by an **ESP32-S3R8**
(dual-core 240 MHz, 8 MB octal PSRAM, 16 MB QSPI flash). A second MCU, an **ESP32-U4WDH**
"sidecar", runs the vendor's Bluetooth audio firmware. Our firmware targets the S3 only.

Source of truth for every number below: the Waveshare schematic and ESP-IDF demos, cross-checked
on a real unit (I2C scan, SD mount, knob events, touch, mic, backlight, haptics). The full pin
list is in `references/pinmap.md`; the same values live as macros in
`templates/knob-starter/components/board/include/board_jc3636k518c.h`, which is the **only**
place pins may appear in code.

## What it has

| Subsystem | Part | Bus | Notes |
|---|---|---|---|
| Display | ST77916, 360×360 RGB565 | QSPI on SPI2, 80 MHz | Colours inverted in the init table; TE not wired |
| Touch | CST816D (chip ID 0xB6) | I2C0 @ 0x15 | Shares the bus with the haptic driver |
| Haptics | DRV2605L + LRA | I2C0 @ 0x5A | I2C-only control, ~120 library effects |
| Knob | 2 detector switches (SSCM110100) | 2 GPIO | One switch pair on the S3, the other on the sidecar |
| microSD | TF slot, SDMMC 4-bit | 6 GPIO | Shipped 480 MB card; units vary in quality |
| Microphone | MSM261D4030H1CPM PDM | 2 GPIO | Mono, 16 kHz works |
| Audio out | PCM5100A I2S DAC, line-level | 3 GPIO + mux | 3.5 mm jack only; **no amplifier, no speaker** |
| Backlight | AO3400A low-side switch | GPIO47 | 10 kΩ pull-down: dark until driven; LEDC PWM at 20 kHz works |
| Battery sense | 10k/10k divider from the 5 V rail | GPIO1 (ADC1_CH0) | Reads the system rail ×2, not the cell |
| USB | One USB-C, D+/D− to **both** chips | — | Orientation picks the chip (below) |

## What it does not have

- **No LED ring** (the older K718C has 13 WS2812). Feedback is screen + haptics.
- **No push button on the knob.** Rotate and touch only; use a tap as "select".
- **No speaker, no amp.** Line-out on the jack only.
- **No tearing-effect line.** Expect tearing on full-screen animation; keep motion local.
- **No Classic Bluetooth on the S3** (BLE 5 only). Classic BT lives on the sidecar.

## Pins you must leave alone

| GPIO | Why |
|---|---|
| **0** | Triple-booked: BOOT strap, select line of the CH445P I2S mux (high = S3 owns the DAC), and exposed on the 10-pin header. On-board pull-up. **Never configure or drive it.** |
| 38, 48 | UART to the sidecar (S3 TX 38, RX 48). Reserved. |
| 39, 40, 41 | I2S to the DAC (BCK, WS, DOUT). Unused by the starter; reserved. |
| 45, 46 | Mic clock and data. Both are S3 strapping pins; as inputs they are fine. Never pull 46 up at boot. |
| 19, 20 | Native USB D−/D+. |

The sidecar owns the DAC soft-mute line, so line-out audio from the S3 only works if the stock
sidecar firmware leaves the DAC unmuted. Treat the sidecar as opaque.

## USB: cable orientation picks the chip

One USB-C socket, two USB data paths. Flip the plug to switch.

| Enumerates as | Chip | Port | Use |
|---|---|---|---|
| `303a:1001` Espressif USB JTAG/serial | ESP32-S3 | `/dev/ttyACM0` | **Flash and monitor our firmware here** |
| `1a86:7523` CH340 USB-UART | ESP32 sidecar | `/dev/ttyUSB0` | Never flash the S3 image here |

If `lsusb` shows `1a86:7523`, flip the plug.

## Flashing recipe (host esptool, `--no-stub`)

The S3 enumerates as native USB-Serial/JTAG, and the esptool flasher **stub stalls on it**
("Packet content transfer stopped"). Baud is ignored over USB CDC. Always pass `--no-stub`.
Flash from the **host**, never from inside the Docker build container (it has no serial access).

```bash
lsusb | grep 303a && ls /dev/ttyACM0            # confirm the S3 side is up
esptool --chip esp32s3 --port /dev/ttyACM0 --no-stub \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0     build/bootloader/bootloader.bin \
  0x8000  build/partition_table/partition-table.bin \
  0x10000 build/knob_starter.bin
```

Success ends with `Hash of data verified.` and `Hard resetting via RTS pin...`. After the first
full flash, an app-only flash (just the `0x10000` image) is enough for code changes.
`templates/knob-starter/scripts/flash.sh` wraps this.

- **First flash must be over USB**: the 16 MB custom partition table (factory 9 MB + spiffs 4 MB,
  no OTA slots) cannot be applied any other way.
- If the port is missing: plug a USB-C **data** cable, flip it, or hold BOOT while plugging in.
- The vendor firmware is the only copy of itself. Back it up before the first flash:
  `esptool --chip esp32s3 --port /dev/ttyACM0 --no-stub read_flash 0 0x1000000 stock.bin`.
- Monitor with `idf.py monitor -p /dev/ttyACM0` from the host or any serial terminal at 115200.

## Gotchas that cost real time

1. **The knob is not a quadrature encoder.** Two detector switches to GND, idle high; one clean
   LOW pulse per detent on exactly one channel, and *which* channel pulsed is the direction.
   PCNT rotary-encoder drivers and `espressif/knob` read zero. Poll both pins every 3 ms and
   decode with a small debounced state machine (`knob_decode.c` in the template). On this board
   only one switch pair reaches the S3 (GPIO8/7); that is enough for direction.
2. **Invert colours exactly once.** The panel needs inversion. The init table already contains
   `0x21`; do **not** also call `esp_lcd_panel_invert_color()`. Byte swapping (`swap_bytes`) is a
   separate axis and is still required for RGB565 over SPI.
3. **Use the vendor init table, don't guess one.** `ui_panel_init_cmds.c` in the template is the
   proven ST77916 sequence (the ESPHome port also needed an explicit `0x3A 0x55` for RGB565).
4. **The backlight is off at boot** (pull-down). Nothing shows until GPIO47 is driven. A dark
   panel after flashing is usually this, not a broken display.
5. **Draw buffers live in internal RAM**, not PSRAM: 40 rows × 360 × 2 B ≈ 28.8 KB each,
   double-buffered, DMA-capable. QSPI bandwidth is the limit, not RAM; see the
   `embedded-constraints` skill.
6. **Haptic at 0x5A on the touch bus.** Probe before use; share the I2C bus handle from the
   touch init rather than creating a second bus.
7. **Strapping-pin warnings at boot are expected** for 45/46 (mic). Ignore them; never "fix"
   them by adding pull-ups.
8. **The shipped SD card may be short.** One unit's card reported 503 MB but timed out on every
   read past ~303 MB, and a failed read wedged the card until power-off. Fit a known-good card
   before relying on storage.
9. **The battery ADC reads the 5 V rail**, so on USB it reports ~4.6 V regardless of the cell.
   Any percentage is a heuristic; calibrate per unit if you need it.
10. **Two radios on one band.** The sidecar's Bluetooth stack transmits while it runs stock
    firmware and will degrade S3 Wi-Fi throughput. Harmless on USB, a real tax on battery.

## Boot checklist for a new firmware

1. Backlight driven on GPIO47 (LEDC) → panel lights.
2. Panel init table applied, `swap_bytes` on, no extra invert → correct colours.
3. I2C scan on SDA11/SCL12 shows exactly 0x15 and 0x5A.
4. Knob poll on GPIO8/7 yields ±1 per detent, direction as expected (flip the sign in the
   board header if not; the direction macro is the one sign-flip point).
5. GPIO0 untouched; no code mentions it except the reserved-pin macro.

## References

- Waveshare wiki (schematic, demos, datasheets): https://www.waveshare.com/wiki/ESP32-S3-Knob-Touch-LCD-1.8
- Guition product page: https://www.guition.com/esp32-display-module/1-8-inch-knob-display
- ESPHome device page (display init, mic pins): https://devices.esphome.io/devices/esp32s3-18-inch-jc3636k518c/
- Datasheets: ST77916, CST816D, DRV2605L, PCM5100A (links on the Waveshare wiki)
