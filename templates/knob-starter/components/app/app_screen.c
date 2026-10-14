// app_screen.c — widget construction and recolouring. Layout stays inside the 340 px safe circle
// (HUD_SAFE_DIA); the accent ring is the "colour = state" surface.
#include "app_screen.h"
#include "theme.h"
#include "fonts.h"
#include <stdio.h>

static lv_obj_t *s_ring, *s_counter, *s_status, *s_arc, *s_title;

static theme_state_t theme_for(app_state_t s)
{
    switch (s) {
    case APP_ACTIVE:    return THEME_STATE_RECORDING;
    case APP_UPLOADING: return THEME_STATE_UPLOADING;
    case APP_DONE:      return THEME_STATE_SAVED;
    case APP_ERROR:     return THEME_STATE_ERROR;
    default:            return THEME_STATE_IDLE;
    }
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, lv_align_t align, int y)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_align(l, align, 0, y);
    return l;
}

void app_screen_build(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(HUD_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(scr, false);
    lv_obj_set_clickable(scr, true);

    // Accent ring just inside the panel edge: a 6 px border on a transparent circle.
    s_ring = lv_obj_create(scr);
    lv_obj_set_size(s_ring, HUD_SAFE_DIA, HUD_SAFE_DIA);
    lv_obj_center(s_ring);
    lv_obj_set_style_radius(s_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ring, 6, 0);
    lv_obj_set_clickable(s_ring, false);    // taps fall through to the screen
    lv_obj_set_scrollable(s_ring, false);

    // Upload progress arc, hidden until needed, drawn just inside the ring.
    s_arc = lv_arc_create(scr);
    lv_obj_set_size(s_arc, HUD_SAFE_DIA - 24, HUD_SAFE_DIA - 24);
    lv_obj_center(s_arc);
    lv_arc_set_rotation(s_arc, 270);
    lv_arc_set_bg_angles(s_arc, 0, 360);
    lv_arc_set_range(s_arc, 0, 100);
    lv_arc_set_value(s_arc, 0);
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(s_arc, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(HUD_LINE), LV_PART_MAIN);
    lv_obj_set_clickable(s_arc, false);
    lv_obj_set_hidden(s_arc, true);

    s_title   = make_label(scr, &font_muli_28, HUD_SUB, LV_ALIGN_TOP_MID, 70);
    s_counter = make_label(scr, &font_muli_28, HUD_INK, LV_ALIGN_CENTER, -6);
    s_status  = make_label(scr, LV_FONT_DEFAULT, HUD_FAINT, LV_ALIGN_BOTTOM_MID, -70);
    lv_label_set_text(s_title, "knob");
    lv_label_set_text(s_status, "");
    app_screen_set_counter(0);
    app_screen_set_state(APP_IDLE);
}

void app_screen_set_state(app_state_t state)
{
    lv_color_t c = lv_color_hex(theme_state_color(theme_for(state)));
    lv_obj_set_style_border_color(s_ring, c, 0);
    lv_obj_set_style_arc_color(s_arc, c, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(s_title, c, 0);
    static const char *const NAMES[] = { "idle", "active", "uploading", "done", "error" };
    lv_label_set_text(s_title, NAMES[state]);
    lv_obj_set_hidden(s_arc, state != APP_UPLOADING);
}

void app_screen_set_counter(int value)
{
    lv_label_set_text_fmt(s_counter, "%d", value);
}

void app_screen_set_status(const char *text)
{
    lv_label_set_text(s_status, text ? text : "");
}

void app_screen_set_progress(uint64_t sent, uint64_t total)
{
    int pct = total ? (int)(sent * 100 / total) : 0;
    lv_arc_set_value(s_arc, pct);
}

lv_obj_t *app_screen_touch_target(void)
{
    return lv_screen_active();
}
