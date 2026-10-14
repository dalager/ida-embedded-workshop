#pragma once
// Host-test stub for components/theme/include/tokens.h. Every macro theme.c references gets a
// DISTINCT SENTINEL so test_theme.c pins the state→token WIRING, not the real design values.
#define HUD_ST_IDLE         0x010101u
#define HUD_ST_ARMING       0x020202u
#define HUD_ST_RECORDING    0x030303u
#define HUD_REC             0x040404u
#define HUD_ST_PAUSED       0x050505u
#define HUD_ST_UPLOADING    0x060606u
#define HUD_ST_SAVED        0x070707u
#define HUD_ST_SYNCED       0x080808u
#define HUD_ST_LINKED       0x090909u
#define HUD_ST_ERROR        0x0A0A0Au
#define HUD_ST_LOW_BATTERY  0x0B0B0Bu
#define HUD_INK             0x0C0C0Cu

#define HUD_MS_IDLE_BREATHE  1001u
#define HUD_MS_ARMING_PULSE  1002u
#define HUD_MS_RECORDING     1003u
#define HUD_MS_STOPCONFIRM   1004u
#define HUD_MS_PAUSED_PULSE  1005u
#define HUD_MS_UPLOAD_SPIN   1006u
#define HUD_MS_SUCCESS_PULSE 1007u
#define HUD_MS_ERROR_BLINK   1008u
