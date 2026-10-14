// Host unit tests for theme.c — state→color and state→motion_ms mapping.
// No ESP-IDF; uses vendored Unity. The generated token header is replaced by
// stubs/tokens.h (distinct sentinels) so we pin the
// state→token wiring rather than the real design values.
#include "unity.h"
#include "theme.h"
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

// ── theme_state_color ─────────────────────────────────────────────────────────

void test_color_idle(void) {
    TEST_ASSERT_EQUAL_HEX32(HUD_ST_IDLE, theme_state_color(THEME_STATE_IDLE));
}

void test_color_arming(void) {
    TEST_ASSERT_EQUAL_HEX32(HUD_ST_ARMING, theme_state_color(THEME_STATE_ARMING));
}

void test_color_recording(void) {
    TEST_ASSERT_EQUAL_HEX32(HUD_ST_RECORDING, theme_state_color(THEME_STATE_RECORDING));
}

void test_color_stop_confirm_stays_on_rec(void) {
    // S19 handoff: halo keeps the solid-red recording colour during hold-to-confirm.
    TEST_ASSERT_EQUAL_HEX32(HUD_REC, theme_state_color(THEME_STATE_STOP_CONFIRM));
}

void test_color_paused(void) {
    TEST_ASSERT_EQUAL_HEX32(HUD_ST_PAUSED, theme_state_color(THEME_STATE_PAUSED));
}

void test_color_uploading(void) {
    TEST_ASSERT_EQUAL_HEX32(HUD_ST_UPLOADING, theme_state_color(THEME_STATE_UPLOADING));
}

void test_color_saved(void) {
    TEST_ASSERT_EQUAL_HEX32(HUD_ST_SAVED, theme_state_color(THEME_STATE_SAVED));
}

void test_color_synced(void) {
    TEST_ASSERT_EQUAL_HEX32(HUD_ST_SYNCED, theme_state_color(THEME_STATE_SYNCED));
}

void test_color_linked(void) {
    TEST_ASSERT_EQUAL_HEX32(HUD_ST_LINKED, theme_state_color(THEME_STATE_LINKED));
}

void test_color_error(void) {
    TEST_ASSERT_EQUAL_HEX32(HUD_ST_ERROR, theme_state_color(THEME_STATE_ERROR));
}

void test_color_low_battery(void) {
    TEST_ASSERT_EQUAL_HEX32(HUD_ST_LOW_BATTERY, theme_state_color(THEME_STATE_LOW_BATTERY));
}

void test_color_unknown_state_returns_ink(void) {
    // THEME_STATE_COUNT is out-of-range → default: return HUD_INK.
    TEST_ASSERT_EQUAL_HEX32(HUD_INK, theme_state_color(THEME_STATE_COUNT));
}

// ── theme_state_motion_ms ─────────────────────────────────────────────────────

void test_motion_idle(void) {
    TEST_ASSERT_EQUAL_UINT32(HUD_MS_IDLE_BREATHE, theme_state_motion_ms(THEME_STATE_IDLE));
}

void test_motion_arming(void) {
    TEST_ASSERT_EQUAL_UINT32(HUD_MS_ARMING_PULSE, theme_state_motion_ms(THEME_STATE_ARMING));
}

void test_motion_recording(void) {
    // Solid interlock — maps to HUD_MS_RECORDING token (real value is 0, sentinel is 1003).
    TEST_ASSERT_EQUAL_UINT32(HUD_MS_RECORDING, theme_state_motion_ms(THEME_STATE_RECORDING));
}

void test_motion_stop_confirm(void) {
    TEST_ASSERT_EQUAL_UINT32(HUD_MS_STOPCONFIRM, theme_state_motion_ms(THEME_STATE_STOP_CONFIRM));
}

void test_motion_paused(void) {
    TEST_ASSERT_EQUAL_UINT32(HUD_MS_PAUSED_PULSE, theme_state_motion_ms(THEME_STATE_PAUSED));
}

void test_motion_uploading(void) {
    TEST_ASSERT_EQUAL_UINT32(HUD_MS_UPLOAD_SPIN, theme_state_motion_ms(THEME_STATE_UPLOADING));
}

void test_motion_saved_synced_linked_share_success_pulse(void) {
    // All three post-capture success states share the same motion token.
    TEST_ASSERT_EQUAL_UINT32(HUD_MS_SUCCESS_PULSE, theme_state_motion_ms(THEME_STATE_SAVED));
    TEST_ASSERT_EQUAL_UINT32(HUD_MS_SUCCESS_PULSE, theme_state_motion_ms(THEME_STATE_SYNCED));
    TEST_ASSERT_EQUAL_UINT32(HUD_MS_SUCCESS_PULSE, theme_state_motion_ms(THEME_STATE_LINKED));
}

void test_motion_error(void) {
    TEST_ASSERT_EQUAL_UINT32(HUD_MS_ERROR_BLINK, theme_state_motion_ms(THEME_STATE_ERROR));
}

void test_motion_low_battery_uses_paused_pulse(void) {
    // Amber pulse — reuses the PAUSED_PULSE token.
    TEST_ASSERT_EQUAL_UINT32(HUD_MS_PAUSED_PULSE, theme_state_motion_ms(THEME_STATE_LOW_BATTERY));
}

void test_motion_unknown_state_returns_zero(void) {
    TEST_ASSERT_EQUAL_UINT32(0, theme_state_motion_ms(THEME_STATE_COUNT));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_color_idle);
    RUN_TEST(test_color_arming);
    RUN_TEST(test_color_recording);
    RUN_TEST(test_color_stop_confirm_stays_on_rec);
    RUN_TEST(test_color_paused);
    RUN_TEST(test_color_uploading);
    RUN_TEST(test_color_saved);
    RUN_TEST(test_color_synced);
    RUN_TEST(test_color_linked);
    RUN_TEST(test_color_error);
    RUN_TEST(test_color_low_battery);
    RUN_TEST(test_color_unknown_state_returns_ink);
    RUN_TEST(test_motion_idle);
    RUN_TEST(test_motion_arming);
    RUN_TEST(test_motion_recording);
    RUN_TEST(test_motion_stop_confirm);
    RUN_TEST(test_motion_paused);
    RUN_TEST(test_motion_uploading);
    RUN_TEST(test_motion_saved_synced_linked_share_success_pulse);
    RUN_TEST(test_motion_error);
    RUN_TEST(test_motion_low_battery_uses_paused_pulse);
    RUN_TEST(test_motion_unknown_state_returns_zero);
    return UNITY_END();
}
