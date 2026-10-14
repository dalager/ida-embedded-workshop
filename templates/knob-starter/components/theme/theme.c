#include "theme.h"
#include <stdbool.h>

// State -> token tables (designated initializers keep each row next to its state name).
static const uint32_t STATE_COLOR[THEME_STATE_COUNT] = {
    [THEME_STATE_IDLE]         = HUD_ST_IDLE,
    [THEME_STATE_ARMING]       = HUD_ST_ARMING,
    [THEME_STATE_RECORDING]    = HUD_ST_RECORDING,
    [THEME_STATE_STOP_CONFIRM] = HUD_REC,          // still recording; accent stays solid red
    [THEME_STATE_PAUSED]       = HUD_ST_PAUSED,
    [THEME_STATE_UPLOADING]    = HUD_ST_UPLOADING,
    [THEME_STATE_SAVED]        = HUD_ST_SAVED,
    [THEME_STATE_SYNCED]       = HUD_ST_SYNCED,
    [THEME_STATE_LINKED]       = HUD_ST_LINKED,
    [THEME_STATE_ERROR]        = HUD_ST_ERROR,
    [THEME_STATE_LOW_BATTERY]  = HUD_ST_LOW_BATTERY,
};

static const uint32_t STATE_MOTION_MS[THEME_STATE_COUNT] = {
    [THEME_STATE_IDLE]         = HUD_MS_IDLE_BREATHE,
    [THEME_STATE_ARMING]       = HUD_MS_ARMING_PULSE,
    [THEME_STATE_RECORDING]    = HUD_MS_RECORDING,      // 0 — solid interlock
    [THEME_STATE_STOP_CONFIRM] = HUD_MS_STOPCONFIRM,
    [THEME_STATE_PAUSED]       = HUD_MS_PAUSED_PULSE,
    [THEME_STATE_UPLOADING]    = HUD_MS_UPLOAD_SPIN,
    [THEME_STATE_SAVED]        = HUD_MS_SUCCESS_PULSE,
    [THEME_STATE_SYNCED]       = HUD_MS_SUCCESS_PULSE,
    [THEME_STATE_LINKED]       = HUD_MS_SUCCESS_PULSE,
    [THEME_STATE_ERROR]        = HUD_MS_ERROR_BLINK,
    [THEME_STATE_LOW_BATTERY]  = HUD_MS_PAUSED_PULSE,   // amber pulse
};

static inline bool state_in_range(theme_state_t state)
{
    return (unsigned)state < (unsigned)THEME_STATE_COUNT;
}

uint32_t theme_state_color(theme_state_t state)
{
    return state_in_range(state) ? STATE_COLOR[state] : HUD_INK;
}

uint32_t theme_state_motion_ms(theme_state_t state)
{
    return state_in_range(state) ? STATE_MOTION_MS[state] : 0;
}
