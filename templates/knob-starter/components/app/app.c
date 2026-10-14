// app.c — the state machine and its inputs. Rules this file demonstrates:
//   • knob and touch callbacks run on the LVGL task → touch widgets directly, no lock needed;
//   • the upload runs on its own task and NEVER touches LVGL — a 200 ms lv_timer polls its
//     atomics and updates the arc/status, so there is exactly one owner of the screen.
#include "app.h"
#include "app_screen.h"
#include "app_upload.h"
#include "ui.h"
#include "knob.h"
#include "haptic.h"
#include "net.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>

static const char *TAG = "app";

static app_state_t s_state = APP_IDLE;
static int         s_counter;

static void set_state(app_state_t next)
{
    if (next == s_state) return;
    ESP_LOGI(TAG, "state %d -> %d", (int)s_state, (int)next);
    s_state = next;
    app_screen_set_state(next);
}

// ---- inputs (LVGL task) ----

static void on_knob_step(int dir)
{
    s_counter += dir;
    app_screen_set_counter(s_counter);
}

static void on_touch(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED) {
        haptic_click();
        set_state(s_state == APP_ACTIVE ? APP_IDLE : APP_ACTIVE);
    } else if (code == LV_EVENT_LONG_PRESSED) {
        if (s_state == APP_UPLOADING) return;
        haptic_click();
        if (app_upload_start()) {
            app_screen_set_progress(0, 1);
            set_state(APP_UPLOADING);
        } else {
            set_state(APP_ERROR);
        }
    }
}

// ---- 200 ms status tick (LVGL task) ----

static void show_wifi_status(void)
{
    char line[48];
    if (net_is_connected()) snprintf(line, sizeof(line), "wi-fi %d dBm", net_rssi());
    else                    snprintf(line, sizeof(line), "wi-fi connecting...");
    app_screen_set_status(line);
}

static void on_tick(lv_timer_t *t)
{
    (void)t;
    uint64_t sent, total; bool done, ok;
    app_upload_poll(&sent, &total, &done, &ok);

    if (s_state == APP_UPLOADING) {
        app_screen_set_progress(sent, total);
        char line[48];
        snprintf(line, sizeof(line), "%u / %u KB", (unsigned)(sent / 1024), (unsigned)(total / 1024));
        app_screen_set_status(line);
    } else {
        show_wifi_status();
    }
    if (done) set_state(ok ? APP_DONE : APP_ERROR);
}

void app_init(void)
{
    if (!ui_lock(1000)) { ESP_LOGE(TAG, "no LVGL lock — no screen"); return; }
    app_screen_build();
    lv_obj_add_event_cb(app_screen_touch_target(), on_touch, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(app_screen_touch_target(), on_touch, LV_EVENT_LONG_PRESSED, NULL);
    lv_timer_create(on_tick, 200, NULL);
    ui_unlock();

    knob_init(on_knob_step);   // takes the lock itself
    ESP_LOGI(TAG, "ready: turn = count, tap = toggle, hold = upload %u KB", APP_UPLOAD_BYTES / 1024);
}
