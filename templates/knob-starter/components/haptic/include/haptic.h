#pragma once
// haptic — DRV2605L LRA driver (I2C 0x5A on the touch bus). Depends on: board (address, capability
// flag), esp_driver_i2c (the bus handle comes from ui_i2c_bus()). Every call is non-blocking and
// safe from any task; before haptic_init() or without a chip every call is a silent no-op.
#include <stdint.h>
#include "driver/i2c_master.h"

// Bring up the DRV2605L on `bus` (pass ui_i2c_bus()). Probes the chip first; if it does not answer
// (or BOARD_HAS_HAPTIC is 0) every later call is a no-op.
void haptic_init(i2c_master_bus_handle_t bus);

// Fire one DRV2605L library effect (1..123, TI SLOS854 table 11.2); 0 does nothing. The chip plays
// the waveform on its own after GO, so this returns immediately.
void haptic_play(uint8_t effect);

// The one cue most UIs need: a short strong click (library effect 1).
void haptic_click(void);
