# Device cheat-sheet — Guition JC3636K518C

One page. Every pin below was cross-checked against the Waveshare schematic and verified on a unit.
The board header in the starter template is the only place pin numbers belong in code.

## Identity

| Field | Value |
|---|---|
| Model | `JC3636K518C_I_YR1` ("K5 Knob Series"), Guition / Shenzhen Jingcai |
| Twin | Waveshare **ESP32-S3-Knob-Touch-LCD-1.8** (same PCB, blue case) |
| Display | 1.8" round IPS, **360 × 360**, ST77916 over QSPI, RGB565, colours inverted |
| Main MCU | **ESP32-S3R8**: dual-core 240 MHz, **8 MB octal PSRAM**, **16 MB** QSPI flash, Wi-Fi + BLE 5 |
| Second MCU | ESP32-U4WDH sidecar (Classic BT audio). Runs stock firmware. Treat as opaque. |
| Inputs | Capacitive touch (CST816D) + rotary knob. **No push button.** |
| Feedback | Screen + haptic (DRV2605L + LRA). **No LED ring, no speaker.** |
| Storage | microSD slot (SDMMC 4-bit), shipped card is a 480 MB SDSC |
| Audio | PCM5100A DAC, **line-out 3.5 mm jack only**; PDM MEMS mic |

## ESP32-S3 pin map

| Display (ST77916, QSPI on SPI2) | GPIO |   | Touch (CST816D, I2C) | GPIO |
|---|---|---|---|---|
| SCK | 13 |   | SDA | 11 |
| CS | 14 |   | SCL | 12 |
| D0 / D1 / D2 / D3 | 15 / 16 / 17 / 18 |   | INT | 9 |
| RST | 21 |   | RST | 10 |
| TE | **not wired** |   | DRV2605L haptic | same bus, addr **0x5A** |
| Backlight | 47 (LEDC PWM, off at boot) |   | | |

| Knob (detector switches) | GPIO |   | microSD (SDMMC 4-bit) | GPIO |
|---|---|---|---|---|
| EC1_A | 8 |   | CLK | 4 |
| EC1_B | 7 |   | CMD | 3 |
| | |   | D0 / D1 / D2 / D3 | 5 / 6 / 42 / 2 |

| Other | GPIO | Notes |
|---|---|---|
| PDM mic clock / data | 45 / 46 | Strapping pins; inputs only, fine |
| Battery / rail sense | 1 | ADC1_CH0, ×2 divider on the **system rail**, not the cell |
| USB D− / D+ | 19 / 20 | Native USB-Serial/JTAG |

## Reserved pins — never drive these

| GPIO | Why |
|---|---|
| **0** | BOOT strap **and** I2S mux select **and** exposed on the expansion header. On-board pull-up. Leave it alone. |
| 38 / 48 | UART to the ESP32 sidecar (TX / RX) |
| 39 / 40 / 41 | I2S to the DAC (BCK / WS / DOUT); DAC mute is owned by the sidecar |

## What this board does NOT have

No LED ring · no speaker or amplifier · no push button on the knob · no TE (vsync) line ·
no quadrature encoder (see gotchas) · no OTA partitions in the template (single 9 MB app).

## USB: the cable orientation picks the chip

| You see | Chip | Port | Use |
|---|---|---|---|
| `303a:1001` Espressif USB JTAG/serial | **ESP32-S3** | `/dev/ttyACM0` (Linux), `/dev/cu.usbmodem*` (macOS), `COMx` | **This is the one you flash** |
| `1a86:7523` CH340 | ESP32 sidecar | `/dev/ttyUSB0`, `/dev/cu.wchusbserial*` | Not ours. Flip the plug. |

`lsusb` (Linux) or `system_profiler SPUSBDataType` (macOS) tells you which one you got.

## Flash recipe (host, never the container)

```bash
# After scripts/build.sh produced build/ — run on the HOST, from the template root
esptool --chip esp32s3 --port /dev/ttyACM0 --no-stub \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0     build/bootloader/bootloader.bin \
  0x8000  build/partition_table/partition-table.bin \
  0x10000 build/knob_starter.bin
```

- **`--no-stub` is mandatory** on this board's native USB serial; with the stub it stalls.
- **First flash must be the full set** (bootloader + partition table + app) because the 16 MB
  partition table differs from stock. After that `0x10000 app` alone is enough.
- If the port vanishes: hold **BOOT** while tapping **RESET** to force download mode.
- Monitor: `esptool` does not monitor; use `python3 -m serial.tools.miniterm /dev/ttyACM0 115200`
  or `idf.py monitor` only if you have a native IDF.

## Top 8 gotchas

1. **The knob is not a quadrature encoder.** Two detector switches to GND, idle high; one detent
   is one clean low pulse on exactly one channel and *which* channel encodes direction. Poll at
   3 ms and debounce in pure code. PCNT/rotary-encoder drivers read garbage.
2. **GPIO0 is triple-booked.** Never configure it as an output. Driving it low around reset
   puts the chip in download mode and silences the audio path.
3. **Invert colours exactly once.** The panel needs inversion (`0x21` in the init table). Doing it
   both in the init table and via `esp_lcd_panel_invert_color()` gives a negative image.
4. **Internal RAM is the scarce resource.** 8 MB PSRAM is plenty; the ~512 KB internal SRAM is
   shared by Wi-Fi, lwIP, mbedTLS and task stacks. Keep LVGL's pool in PSRAM, keep the draw
   buffer small (~20 rows) and internal, stream uploads through one 16 KB chunk.
5. **No TE line** means tearing on full-screen animations. Partial refresh, calm motion.
6. **Strapping warnings on 45/46 are normal.** They are mic inputs. Do not "fix" them.
7. **Touch and haptic share one I2C bus.** Init the bus once; the DRV2605L is at 0x5A.
8. **The backlight is dark until driven.** GPIO47 has a pull-down; if the screen is black after
   flashing, check the LEDC init before suspecting the panel.
