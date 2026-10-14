#pragma once
// app — the "hello world" state machine: knob turns a counter, tap toggles IDLE/ACTIVE, long press
// streams a 2 MiB generated payload to the receiver. Depends on ui, theme, haptic, net; nothing
// depends on app. Everything LVGL happens on the LVGL task; the upload runs on its own task and
// reports back through atomics that a 200 ms LVGL timer reads (app.c explains why).
#include <stdint.h>
#include <stddef.h>

typedef enum {
    APP_IDLE = 0,      // teal, breathing
    APP_ACTIVE,        // red, solid
    APP_UPLOADING,     // blue, progress arc
    APP_DONE,          // green
    APP_ERROR,         // red blink
} app_state_t;

// Build the screen, wire knob + touch, start the status timer. Call last in app_main.
void app_init(void);

// Test hooks (components/testctl): run the SAME handlers hardware input runs. Caller holds
// ui_lock(). Add one per new input; the console never reaches into this component's statics.
void app_inject_knob(int steps);                   // sign = direction, |steps| detents
void app_inject_tap(void);                         // short tap
void app_inject_hold(void);                        // long press
void app_inject_reset(void);                       // boot state: APP_IDLE, counter 0
void app_state_line(char *buf, size_t cap);        // "state=idle counter=N"
