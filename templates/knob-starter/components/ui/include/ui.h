#pragma once
// ui — hardware bring-up for the round display: ST77916 over QSPI, CST816 touch on I2C0, LVGL via
// esp_lvgl_port (which owns the tick, the render task and the lock). Depends on: board (pins),
// esp_lcd, esp_lvgl_port. Everything that creates or touches an LVGL object from OUTSIDE the LVGL
// task (app code, timers you create elsewhere, network callbacks) must hold ui_lock() around it.
#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"

// Bring up panel, LVGL and touch. Call once from app_main before anything draws.
void ui_init(void);

// The I2C0 master bus the touch controller lives on. Other I2C peripherals on the same wires
// (the DRV2605L haptic driver) register against this handle. NULL before ui_init().
i2c_master_bus_handle_t ui_i2c_bus(void);

// Take / release the LVGL lock. ui_lock returns false on timeout — then do NOT touch LVGL.
// LVGL event callbacks and lv_timer callbacks already run with the lock held: no lock there.
bool ui_lock(uint32_t timeout_ms);
void ui_unlock(void);
