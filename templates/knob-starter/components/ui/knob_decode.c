// knob_decode.c — pure detent decoder. See knob_decode.h for the hardware model
// and the debounce contract. Pure C11: no LVGL / ESP-IDF / FreeRTOS — the esp_timer poll in
// knob.c samples the pins and calls this every 3 ms; the host test drives the same entry
// point with synthetic samples.
#include "knob_decode.h"

// Latch the release phase: drop any half-confirmed pulse and require a confirmed idle.
static void enter_release(knob_decode_state_t *s)
{
    s->waiting_release = true;
    s->pending_dir     = 0;
    s->low_count       = 0;
    s->idle_count      = 0;
}

// Release phase: a detent was emitted (or a both-low bounce latched). Re-arm only after
// KNOB_DEBOUNCE_SAMPLES consecutive idle samples, so release chatter that grazes idle
// can't restart the armed phase and double-count the same physical detent.
static void step_release(knob_decode_state_t *s, bool idle)
{
    if (!idle) {
        s->idle_count = 0;   // still (or again) low — keep waiting
        return;
    }
    if (++s->idle_count >= KNOB_DEBOUNCE_SAMPLES) {
        s->waiting_release = false;
        s->idle_count      = 0;
    }
}

// Armed phase, exactly one channel low: confirm it for KNOB_DEBOUNCE_SAMPLES consecutive
// samples before emitting its direction; returns 0 until then.
static int step_single_low(knob_decode_state_t *s, int dir)
{
    if (dir != s->pending_dir) {
        s->pending_dir = dir;    // new (or switched) channel — restart its confirmation window
        s->low_count   = 1;
        return 0;
    }
    if (++s->low_count < KNOB_DEBOUNCE_SAMPLES) return 0;
    enter_release(s);   // detent confirmed: emit once, then require confirmed idle
    return dir;
}

int knob_decode_step(knob_decode_state_t *s, bool a_low, bool b_low)
{
    bool idle = !a_low && !b_low;

    if (s->waiting_release) {
        step_release(s, idle);
        return 0;
    }

    // Armed phase: confirm a single-channel low for KNOB_DEBOUNCE_SAMPLES consecutive samples
    // before emitting, so closure chatter (low→high→low) and one-sample glitches count zero.
    if (idle) {
        s->pending_dir = 0;
        s->low_count   = 0;
        return 0;
    }

    if (a_low && b_low) {
        // Both low is not a valid detent on this switch pair — treat as bounce: drop any
        // half-confirmed pulse and require a confirmed idle before the next detent.
        enter_release(s);
        return 0;
    }

    return step_single_low(s, a_low ? +1 : -1);   // exactly one channel is low here
}
