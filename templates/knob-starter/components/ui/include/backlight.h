#pragma once
// backlight — LEDC PWM on BOARD_LCD_BL. The pin has a pull-down, so the panel is dark until
// backlight_init() runs; init it right after ui_init() or you will think the display is broken.
// Depends on: board, driver/ledc. Safe to call from any task.
#include <stdint.h>

// Never fully black: a readable floor, also useful as the idle-dim level.
#define BACKLIGHT_MIN_PCT 10u

// Configure LEDC (20 kHz, 10-bit) and drive the default 80 %.
void backlight_init(void);

// Set brightness 0-100 %; clamped to BACKLIGHT_MIN_PCT..100.
void backlight_set_percent(uint8_t pct);
