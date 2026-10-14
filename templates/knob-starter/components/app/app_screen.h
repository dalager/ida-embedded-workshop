#pragma once
// app_screen — the LVGL widgets for the hello-world screen (private to the app component).
// Every function here assumes the LVGL lock is held (call them from LVGL callbacks/timers or
// inside ui_lock()). Colours come from theme tokens; nothing here knows about Wi-Fi or tasks.
#include <stdint.h>
#include "lvgl.h"
#include "app.h"

// Create the screen on lv_screen_active(): background, accent ring, counter, status line, arc.
void app_screen_build(void);

// Recolour ring/arc/labels for `state`; arc visible only while uploading.
void app_screen_set_state(app_state_t state);

void app_screen_set_counter(int value);
void app_screen_set_status(const char *text);   // bottom line: Wi-Fi/RSSI or upload result
void app_screen_set_progress(uint64_t sent, uint64_t total);

// The object that receives taps/long presses (the whole screen).
lv_obj_t *app_screen_touch_target(void);
