// knob_decode.h — pure detent decoder for the round knob display's rotary knob.
// No LVGL / ESP-IDF / FreeRTOS dependencies — host-testable (test/host/test_knob_decode.c).
//
// The knob is NOT a quadrature encoder: it is two detector switches to GND (channels A and B),
// idle HIGH. One detent = one clean LOW pulse on exactly ONE channel; WHICH channel pulsed
// encodes the direction. This decoder
// turns a 3 ms stream of (a_low, b_low) samples into signed step events, one per detent.
//
// Debounce (matches the vendor bidi_switch_knob.c DEBOUNCE_TICKS=2 discipline): a step is
// emitted only after KNOB_DEBOUNCE_SAMPLES consecutive single-channel-low samples, and the
// decoder re-arms only after KNOB_DEBOUNCE_SAMPLES consecutive both-high samples. This rejects
// closure chatter (low→high→low across the poll cadence would otherwise double-count a detent)
// and sub-6 ms glitches, at the cost of ~6 ms extra latency — imperceptible on a scroll.
#pragma once
#include <stdbool.h>

// Consecutive 3 ms samples a level must hold to count (2 ≈ 6 ms, the vendor's minimum pulse).
#define KNOB_DEBOUNCE_SAMPLES 2

// Zero-initialised = idle (both channels released/high, armed for the next detent).
typedef struct {
    bool waiting_release;   // a detent was emitted (or bounce latched); confirm idle to re-arm
    int  pending_dir;       // channel being debounced while armed: +1 = A low, -1 = B low, 0 = none
    int  low_count;         // consecutive samples pending_dir has been low
    int  idle_count;        // consecutive both-high samples while waiting_release
} knob_decode_state_t;

// Feed one 3 ms sample. `a_low`/`b_low` are true when that channel is pulled LOW (active pulse).
// Returns +1 when channel A alone holds low for KNOB_DEBOUNCE_SAMPLES samples from idle, -1 for
// channel B, 0 otherwise. Both-low is contact bounce: no step, and the decoder blocks until a
// confirmed return to idle. A step fires exactly once per detent.
int knob_decode_step(knob_decode_state_t *s, bool a_low, bool b_low);
