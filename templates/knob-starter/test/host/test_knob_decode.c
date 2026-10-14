// Host unit tests for the rotary detent decoder (ui/knob_decode.c).
// Pure C, no LVGL / ESP-IDF.
//
// The knob is two detector switches (channels A/B), idle HIGH, one clean LOW pulse per detent on
// one channel; the decoder turns a 3 ms sample stream into one signed step per detent. Invariants
// (debounced, KNOB_DEBOUNCE_SAMPLES = 2 — vendor bidi_switch_knob.c parity):
//   • idle (both high) never steps
//   • a step fires exactly once, on the 2nd consecutive single-channel-low sample
//   • one-sample glitches and closure chatter (low→high→low) count ZERO — the classic
//     double-count failure mode
//   • re-arm needs 2 consecutive idle samples: release chatter can't re-trigger the same detent
//   • both-low simultaneously is contact bounce → no step, then a confirmed idle is required
#include "unity.h"
#include "knob_decode.h"

static knob_decode_state_t s;

void setUp(void)    { s = (knob_decode_state_t){0}; }   // zero-init = idle, armed
void tearDown(void) {}

// Drive one clean, debounce-length detent pulse + confirmed release; return the summed steps.
static int clean_detent(bool a)
{
    int total = 0;
    for (int i = 0; i < KNOB_DEBOUNCE_SAMPLES; i++)
        total += knob_decode_step(&s, a, !a);            // hold the channel low long enough
    for (int i = 0; i < KNOB_DEBOUNCE_SAMPLES; i++)
        total += knob_decode_step(&s, false, false);     // confirmed return to idle
    return total;
}

// ── zero-init idle: sampling "both high" produces no step ─────────────────────
void test_zero_init_idle_no_step(void)
{
    for (int i = 0; i < 10; i++)
        TEST_ASSERT_EQUAL_INT(0, knob_decode_step(&s, false, false));
}

// ── A pulse: emits exactly one +1, on the 2nd consecutive low sample ──────────
void test_a_pulse_is_plus_one_after_confirm(void)
{
    TEST_ASSERT_EQUAL_INT(0,  knob_decode_step(&s, false, false));  // idle
    TEST_ASSERT_EQUAL_INT(0,  knob_decode_step(&s, true,  false));  // 1st low: confirming
    TEST_ASSERT_EQUAL_INT(+1, knob_decode_step(&s, true,  false));  // 2nd low: confirmed → +1
    TEST_ASSERT_EQUAL_INT(0,  knob_decode_step(&s, false, false));  // releasing
    TEST_ASSERT_EQUAL_INT(0,  knob_decode_step(&s, false, false));  // re-armed, still no repeat
}

// ── B pulse: symmetric, yields -1 ─────────────────────────────────────────────
void test_b_pulse_is_minus_one_after_confirm(void)
{
    TEST_ASSERT_EQUAL_INT(0,  knob_decode_step(&s, false, true));
    TEST_ASSERT_EQUAL_INT(-1, knob_decode_step(&s, false, true));
    TEST_ASSERT_EQUAL_INT(0,  knob_decode_step(&s, false, false));
}

// ── closure chatter: low→high→low around one detent = exactly ONE step ────────
// The trace both reviews flagged as a double-count in the undebounced decoder:
// (A low), (idle), (A low), (A low held). Chatter never reaches 2 consecutive lows
// until the contact settles, so only the settled pulse emits.
void test_closure_chatter_counts_once(void)
{
    int total = 0;
    total += knob_decode_step(&s, true,  false);   // bounce low (1 sample)
    total += knob_decode_step(&s, false, false);   // bounce high
    total += knob_decode_step(&s, true,  false);   // low again — restart confirm
    total += knob_decode_step(&s, true,  false);   // settled → the ONE step fires here
    TEST_ASSERT_EQUAL_INT(+1, total);
    // Remainder of the settled pulse + release: nothing more.
    total = 0;
    total += knob_decode_step(&s, true,  false);
    total += knob_decode_step(&s, false, false);
    total += knob_decode_step(&s, false, false);
    TEST_ASSERT_EQUAL_INT(0, total);
}

// ── release chatter: idle→low blip while releasing does NOT re-trigger ────────
void test_release_chatter_no_retrigger(void)
{
    // Immediately after a confirmed detent, release chatter grazes idle for a single sample:
    knob_decode_step(&s, true, false);
    TEST_ASSERT_EQUAL_INT(+1, knob_decode_step(&s, true, false));   // confirmed detent
    TEST_ASSERT_EQUAL_INT(0,  knob_decode_step(&s, false, false));  // 1 idle sample (not confirmed)
    TEST_ASSERT_EQUAL_INT(0,  knob_decode_step(&s, true,  false));  // release bounce re-low
    TEST_ASSERT_EQUAL_INT(0,  knob_decode_step(&s, true,  false));  // still held — NO second step
    TEST_ASSERT_EQUAL_INT(0,  knob_decode_step(&s, false, false));
    TEST_ASSERT_EQUAL_INT(0,  knob_decode_step(&s, false, false));  // confirmed idle → re-armed
    TEST_ASSERT_EQUAL_INT(0,  knob_decode_step(&s, true,  false));
    TEST_ASSERT_EQUAL_INT(+1, knob_decode_step(&s, true,  false));  // next real detent counts
}

// ── one-sample glitch (EMI/contact tick) is rejected outright ──────────────────
void test_single_sample_glitch_is_noise(void)
{
    TEST_ASSERT_EQUAL_INT(0, knob_decode_step(&s, true,  false));   // 1-sample low
    for (int i = 0; i < 10; i++)
        TEST_ASSERT_EQUAL_INT(0, knob_decode_step(&s, false, false)); // back to idle forever → 0
}

// ── holding a channel low across many samples = still a single step ───────────
void test_held_low_yields_single_step(void)
{
    int total = 0;
    for (int i = 0; i < 20; i++)
        total += knob_decode_step(&s, true, false);
    TEST_ASSERT_EQUAL_INT(1, total);                     // one step no matter how long the hold
    TEST_ASSERT_EQUAL_INT(0, knob_decode_step(&s, false, false));
}

// ── channel switch mid-confirm restarts the window (no cross-channel step) ────
void test_channel_switch_restarts_confirmation(void)
{
    TEST_ASSERT_EQUAL_INT(0,  knob_decode_step(&s, true,  false));  // A: 1 of 2
    TEST_ASSERT_EQUAL_INT(0,  knob_decode_step(&s, false, true));   // B: restart, 1 of 2
    TEST_ASSERT_EQUAL_INT(-1, knob_decode_step(&s, false, true));   // B confirmed → -1
}

// ── both-low bounce: no step, and a confirmed idle required afterwards ────────
void test_both_low_bounce_is_zero(void)
{
    TEST_ASSERT_EQUAL_INT(0, knob_decode_step(&s, true, true));     // both low → bounce latch
    TEST_ASSERT_EQUAL_INT(0, knob_decode_step(&s, true, false));    // still not idle
    TEST_ASSERT_EQUAL_INT(0, knob_decode_step(&s, false, false));   // idle 1 of 2
    TEST_ASSERT_EQUAL_INT(0, knob_decode_step(&s, false, false));   // idle 2 of 2 → re-armed
    TEST_ASSERT_EQUAL_INT(0,  knob_decode_step(&s, true, false));
    TEST_ASSERT_EQUAL_INT(+1, knob_decode_step(&s, true, false));   // next detent counts
}

// ── both-low mid-confirm drops the half-confirmed pulse ───────────────────────
void test_both_low_mid_confirm_drops_pulse(void)
{
    TEST_ASSERT_EQUAL_INT(0, knob_decode_step(&s, true, false));    // A: 1 of 2
    TEST_ASSERT_EQUAL_INT(0, knob_decode_step(&s, true, true));     // bounce → dropped
    TEST_ASSERT_EQUAL_INT(0, knob_decode_step(&s, true, false));    // A again, but blocked
    TEST_ASSERT_EQUAL_INT(0, knob_decode_step(&s, true, false));    // still blocked (no idle yet)
}

// ── alternating detents in opposite directions ───────────────────────────────
void test_alternating_detents(void)
{
    TEST_ASSERT_EQUAL_INT(+1, clean_detent(true));
    TEST_ASSERT_EQUAL_INT(-1, clean_detent(false));
    TEST_ASSERT_EQUAL_INT(+1, clean_detent(true));
}

// ── long one-direction run accumulates one step per pulse ─────────────────────
void test_long_run_one_step_per_pulse(void)
{
    int total = 0;
    for (int i = 0; i < 50; i++)
        total += clean_detent(true);
    TEST_ASSERT_EQUAL_INT(50, total);   // exactly one +1 per detent, no drift
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_zero_init_idle_no_step);
    RUN_TEST(test_a_pulse_is_plus_one_after_confirm);
    RUN_TEST(test_b_pulse_is_minus_one_after_confirm);
    RUN_TEST(test_closure_chatter_counts_once);
    RUN_TEST(test_release_chatter_no_retrigger);
    RUN_TEST(test_single_sample_glitch_is_noise);
    RUN_TEST(test_held_low_yields_single_step);
    RUN_TEST(test_channel_switch_restarts_confirmation);
    RUN_TEST(test_both_low_bounce_is_zero);
    RUN_TEST(test_both_low_mid_confirm_drops_pulse);
    RUN_TEST(test_alternating_detents);
    RUN_TEST(test_long_run_one_step_per_pulse);
    return UNITY_END();
}
