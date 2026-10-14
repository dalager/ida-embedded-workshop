#pragma once
// theme — the "colour = state" policy. Owns the design tokens (tokens.h) and the state → colour /
// state → motion tables; exposes plain integers so it depends on NOTHING (no LVGL, no ESP-IDF) and
// is host-tested in test/host/test_theme.c. The UI converts a colour with lv_color_hex().
#include <stdint.h>
#include "tokens.h"

typedef enum {
    THEME_STATE_IDLE = 0,
    THEME_STATE_ARMING,
    THEME_STATE_RECORDING,
    THEME_STATE_STOP_CONFIRM,   // hold-to-confirm exit; still "recording" colour
    THEME_STATE_PAUSED,
    THEME_STATE_UPLOADING,
    THEME_STATE_SAVED,
    THEME_STATE_SYNCED,
    THEME_STATE_LINKED,
    THEME_STATE_ERROR,
    THEME_STATE_LOW_BATTERY,
    THEME_STATE_COUNT,
} theme_state_t;

// 24-bit RGB (0xRRGGBB) accent colour for a state. Out-of-range states return HUD_INK.
uint32_t theme_state_color(theme_state_t state);

// Animation period in milliseconds for a state (0 = solid/static). Out-of-range returns 0.
uint32_t theme_state_motion_ms(theme_state_t state);
