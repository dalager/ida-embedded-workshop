#pragma once
// Guition JC3636K518C (Waveshare ESP32-S3-Knob-Touch-LCD-1.8, same PCB) — pins and capabilities.
// Source of truth: the Waveshare ESP32-S3-Knob-Touch-LCD-1.8 schematic + demos (same PCB), verified
// on-device 2026-10-13). Pure macros only so host tests can include this header directly.
// A pin of -1 means "not connected on this board".

#define BOARD_NAME              "JC3636K518C"

// --- ST77916 display, QSPI on SPI2 (same panel and init table as the K718) ---
#define BOARD_LCD_SCK           13
#define BOARD_LCD_CS            14
#define BOARD_LCD_D0            15
#define BOARD_LCD_D1            16
#define BOARD_LCD_D2            17
#define BOARD_LCD_D3            18
#define BOARD_LCD_RST           21
#define BOARD_LCD_TE            (-1) // LCD_TE net is not routed to a GPIO
#define BOARD_LCD_BL            47   // AO3400A low-side switch, 10k pull-down: dark until driven

// --- CST816D touch, I2C0 (also carries the DRV2605L at 0x5A); same driver as the CST816S ---
#define BOARD_TOUCH_SDA         11
#define BOARD_TOUCH_SCL         12
#define BOARD_TOUCH_INT         9
#define BOARD_TOUCH_RST         10

// --- Rotary knob: the S3's detector-switch pair (EC1). The second pair (EC2) is on the ESP32
// sidecar and is not needed: direction decodes from EC1 alone (verified on-device 2026-10-13). ---
#define BOARD_KNOB_A            8
#define BOARD_KNOB_B            7
// The one sign-flip point for rotation direction: flip it here if CW turns out to step DOWN.
#define BOARD_KNOB_DIRECTION    (-1)

// --- microSD, SDMMC 4-bit (10k pull-ups on CMD/D0-D3 on-board) ---
#define BOARD_SD_CLK            4
#define BOARD_SD_CMD            3
#define BOARD_SD_D0             5
#define BOARD_SD_D1             6
#define BOARD_SD_D2             42
#define BOARD_SD_D3             2

// --- PDM microphone (MSM261D4030H1CPM, same part as the K718) ---
#define BOARD_MIC_CLK           45
#define BOARD_MIC_DIN           46

// --- System-rail sense: "5V" / 2, not the cell (unused by the firmware today) ---
#define BOARD_BAT_ADC           1

// --- Pins the firmware must leave alone (reserved so nothing else can claim them) ---
#define BOARD_I2S_MUX_GPIO      0    // CH445P select + BOOT strap + header pin: NEVER driven
#define BOARD_SIDECAR_UART_RX   48   // from the ESP32 sidecar (its IO23)
#define BOARD_SIDECAR_UART_TX   38   // to the ESP32 sidecar (its IO18)
#define BOARD_DAC_BCK           39   // PCM5100A line-out via the mux (unused by the firmware)
#define BOARD_DAC_WS            40
#define BOARD_DAC_DOUT          41

// --- Capabilities (gate board-specific code on these, never on BOARD_NAME) ---
#define BOARD_HAS_WS2812_RING   0    // no LED ring on this board
#define BOARD_RING_GPIO         (-1)
#define BOARD_RING_LEDS         0
#define BOARD_HAS_SPEAKER_AMP   0    // line-out jack only
#define BOARD_AMP_MUTE          (-1)
#define BOARD_HAS_HAPTIC        1
#define BOARD_HAPTIC_I2C_ADDR   0x5A

// Every GPIO the firmware may touch or must reserve, for test/host/test_board.c.
#define BOARD_ALL_GPIOS { \
    BOARD_LCD_SCK, BOARD_LCD_CS, BOARD_LCD_D0, BOARD_LCD_D1, BOARD_LCD_D2, BOARD_LCD_D3, \
    BOARD_LCD_RST, BOARD_LCD_TE, BOARD_LCD_BL, \
    BOARD_TOUCH_SDA, BOARD_TOUCH_SCL, BOARD_TOUCH_INT, BOARD_TOUCH_RST, \
    BOARD_KNOB_A, BOARD_KNOB_B, \
    BOARD_SD_CLK, BOARD_SD_CMD, BOARD_SD_D0, BOARD_SD_D1, BOARD_SD_D2, BOARD_SD_D3, \
    BOARD_MIC_CLK, BOARD_MIC_DIN, BOARD_BAT_ADC, \
    BOARD_I2S_MUX_GPIO, BOARD_SIDECAR_UART_RX, BOARD_SIDECAR_UART_TX, \
    BOARD_DAC_BCK, BOARD_DAC_WS, BOARD_DAC_DOUT }
