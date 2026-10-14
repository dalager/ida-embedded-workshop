#pragma once
// knob — rotary knob driver. The knob is NOT a quadrature encoder: two detector switches to GND,
// one LOW pulse on exactly one channel per detent, and WHICH channel encodes direction. A 3 ms
// esp_timer poll feeds the pure knob_decode.c state machine; steps are delivered to your callback
// from the LVGL task (lock already held, so you may touch LVGL objects directly inside it).
// Depends on: board (BOARD_KNOB_A/B, BOARD_KNOB_DIRECTION), esp_timer, driver/gpio, lvgl.

// Start polling. `on_step` is called once per detent with dir = +1 (clockwise) or -1, on the LVGL
// task. Call after ui_init(). Pass NULL to just count (see knob_take_steps).
void knob_init(void (*on_step)(int dir));

// Atomically read-and-clear the detents accumulated since the last call (positive = CW).
// Only needed when no callback was given.
int knob_take_steps(void);
