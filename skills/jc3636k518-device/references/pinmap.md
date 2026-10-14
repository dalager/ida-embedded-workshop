# JC3636K518C — verified ESP32-S3 pin map

All values cross-checked between the Waveshare schematic net labels and the demo
`user_config.h`, then confirmed on a real unit. Code must use the `BOARD_*` macros in
`components/board/include/board_jc3636k518c.h`, never these numbers directly.

## Display — ST77916, QSPI, SPI2 host

| Signal | GPIO | Macro |
|---|---|---|
| SCL/CLK | 13 | `BOARD_LCD_SCK` |
| CS | 14 | `BOARD_LCD_CS` |
| D0 | 15 | `BOARD_LCD_D0` |
| D1 | 16 | `BOARD_LCD_D1` |
| D2 | 17 | `BOARD_LCD_D2` |
| D3 | 18 | `BOARD_LCD_D3` |
| RST | 21 | `BOARD_LCD_RST` |
| TE | not connected | `BOARD_LCD_TE` = -1 |
| Backlight | 47 | `BOARD_LCD_BL` |

## Touch — CST816D (I2C0, also the haptic bus)

| Signal | GPIO | Macro |
|---|---|---|
| SDA | 11 | `BOARD_TOUCH_SDA` |
| SCL | 12 | `BOARD_TOUCH_SCL` |
| INT | 9 | `BOARD_TOUCH_INT` |
| RST | 10 | `BOARD_TOUCH_RST` |

I2C addresses: touch 0x15, DRV2605L haptic 0x5A. 5.1 kΩ pull-ups on board.

## Knob — detector-switch pair on the S3

| Signal | GPIO | Macro |
|---|---|---|
| EC1_A | 8 | `BOARD_KNOB_A` |
| EC1_B | 7 | `BOARD_KNOB_B` |

10 kΩ pull-ups on board. `BOARD_KNOB_DIRECTION` (±1) is the only sign-flip point.
The second pair (EC2) is wired to the sidecar ESP32 (its IO19/IO22) and is not needed.

## microSD — SDMMC 4-bit

| Signal | GPIO | Macro |
|---|---|---|
| CLK | 4 | `BOARD_SD_CLK` |
| CMD | 3 | `BOARD_SD_CMD` |
| D0 | 5 | `BOARD_SD_D0` |
| D1 | 6 | `BOARD_SD_D1` |
| D2 | 42 | `BOARD_SD_D2` |
| D3 | 2 | `BOARD_SD_D3` |

## PDM microphone

| Signal | GPIO | Macro |
|---|---|---|
| Clock | 45 | `BOARD_MIC_CLK` |
| Data | 46 | `BOARD_MIC_DIN` |

Both are strapping pins; fine as inputs.

## Audio out — I2S to the PCM5100A via a CH445P mux

| Signal | GPIO | Macro |
|---|---|---|
| BCK | 39 | `BOARD_DAC_BCK` |
| WS | 40 | `BOARD_DAC_WS` |
| DOUT | 41 | `BOARD_DAC_DOUT` |
| Mux select | 0 | `BOARD_I2S_MUX_GPIO` — **never driven** |

Soft-mute (XSMT) is on the sidecar's IO32, unreachable from the S3.

## Other

| Function | GPIO | Macro / note |
|---|---|---|
| Battery / rail sense | 1 | `BOARD_BAT_ADC`, reads 5 V rail ÷ 2 |
| Sidecar UART RX / TX | 48 / 38 | `BOARD_SIDECAR_UART_RX` / `_TX` |
| Native USB D− / D+ | 19 / 20 | — |
| BOOT | 0 | shared with the mux select (above) |

## Sidecar ESP32-U4WDH (for completeness; we do not program it)

I2S to DAC: BCK 25, WS 27, DIN 26 · DAC XSMT 32 · knob EC2 19/22 · UART to S3 23/18 ·
UART0 1/3 to the CH340. Stock firmware: A2DP sink + HID volume knob.
