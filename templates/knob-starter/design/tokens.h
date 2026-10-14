/* knob-starter design tokens (Rev B) — round 360x360 panel, RGB565.
 * 24-bit hex (lv_color_hex) + pre-computed RGB565 for direct framebuffer use.
 * Token NAMES are stable: every UI element references a token, never a literal colour.
 */
#ifndef KNOB_TOKENS_H
#define KNOB_TOKENS_H

/* ---- Palette ---- */
#define HUD_REC      0xC0392B      /* 0xB9C5  Recording / arming / error — the consent state */
#define HUD_TEAL     0x008945      /* 0x0448  Idle / ready / neutral navigation (brand green) */
#define HUD_AMBER    0xE8A93C      /* 0xED47  Paused / low battery / caution */
#define HUD_BLUE     0x317766      /* 0x33AC  Uploading / syncing in progress (muted teal) */
#define HUD_GREEN    0xDAFDA3      /* 0xDFF4  Saved / synced / linked — success (lime) */
#define HUD_INK      0xF3EFEF      /* 0xF77D  Primary text on panel */
#define HUD_SUB      0xE4DFDF      /* 0xE6FB  Secondary text */
#define HUD_FAINT    0x317766      /* 0x33AC  Tertiary / mono labels */
#define HUD_OFF      0x2B2A2A      /* 0x2945  Disabled / off track (faded black) */
#define HUD_LINE     0x085945      /* 0x0AC8  Hairline dividers / borders */
#define HUD_PANEL    0x0A3A2D      /* 0x09C5  Card / panel surface */
#define HUD_BG       0x052D23      /* 0x0964  Deepest background */

/* ---- State -> color ---- */
#define HUD_ST_IDLE         HUD_TEAL
#define HUD_ST_ARMING       HUD_REC
#define HUD_ST_RECORDING    HUD_REC
#define HUD_ST_PAUSED       HUD_AMBER
#define HUD_ST_UPLOADING    HUD_BLUE
#define HUD_ST_SAVED        HUD_GREEN
#define HUD_ST_SYNCED       HUD_GREEN
#define HUD_ST_LINKED       HUD_GREEN
#define HUD_ST_ERROR        HUD_REC
#define HUD_ST_LOW_BATTERY  HUD_AMBER

/* ---- Geometry ---- */
#define HUD_PANEL_DIA     360
#define HUD_SAFE_DIA      340
#define HUD_LED_COUNT     13

/* ---- Motion (ms; 0 = solid/static) ---- */
#define HUD_MS_IDLE_BREATHE   3800
#define HUD_MS_ARMING_PULSE    700
#define HUD_MS_RECORDING         0
#define HUD_MS_PAUSED_PULSE   1900
#define HUD_MS_STOPCONFIRM    1600
#define HUD_MS_SUCCESS_PULSE  2600
#define HUD_MS_UPLOAD_SPIN    1700
#define HUD_MS_REC_DOT        1400
#define HUD_MS_ERROR_BLINK     900

#endif /* KNOB_TOKENS_H */
