// Board definition sanity: unique, usable GPIOs + capability consistency. Compiled against board.h
// (stubs/sdkconfig.h selects the board) or, with -DBOARD_HEADER="...", against one header directly, so a
// pin clash fails scripts/test.sh instead of surfacing as a dead peripheral on the bench.
#include "unity.h"
#include <stdbool.h>
#ifdef BOARD_HEADER
#include BOARD_HEADER
#else
#include "board.h"
#endif

static const int s_pins[] = BOARD_ALL_GPIOS;
#define PIN_COUNT ((int)(sizeof(s_pins) / sizeof(s_pins[0])))

void setUp(void) {}
void tearDown(void) {}

// No two signals may share a GPIO (-1 = not connected, may repeat).
static void test_no_gpio_is_assigned_twice(void)
{
    for (int i = 0; i < PIN_COUNT; i++) {
        if (s_pins[i] < 0) continue;
        for (int j = i + 1; j < PIN_COUNT; j++) {
            TEST_ASSERT_NOT_EQUAL_MESSAGE(s_pins[i], s_pins[j], "GPIO assigned to two signals");
        }
    }
}

// ESP32-S3 exposes GPIO0..48. On the S3R8, 26..32 are the flash/PSRAM SPI bus and 33..37 the
// extra octal-PSRAM data lines + DQS: driving any of them corrupts PSRAM.
static bool on_memory_bus(int p)
{
    return p >= 26 && p <= 37;
}

static void test_pins_are_usable_s3_gpios(void)
{
    for (int i = 0; i < PIN_COUNT; i++) {
        int p = s_pins[i];
        if (p < 0) continue;
        TEST_ASSERT_TRUE_MESSAGE(p <= 48, "GPIO beyond ESP32-S3 range");
        TEST_ASSERT_FALSE_MESSAGE(on_memory_bus(p), "GPIO on the flash/octal-PSRAM bus");
    }
}

static void test_knob_direction_is_a_sign(void)
{
    TEST_ASSERT_TRUE(BOARD_KNOB_DIRECTION == 1 || BOARD_KNOB_DIRECTION == -1);
    TEST_ASSERT_NOT_EQUAL(BOARD_KNOB_A, BOARD_KNOB_B);
}

// A board without a WS2812 ring must not claim a ring pin.
static void test_ring_pin_matches_capability(void)
{
#if BOARD_HAS_WS2812_RING
    TEST_ASSERT_TRUE(BOARD_RING_GPIO >= 0);
    TEST_ASSERT_TRUE(BOARD_RING_LEDS > 0);
#else
    TEST_ASSERT_EQUAL_INT(-1, BOARD_RING_GPIO);
#endif
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_gpio_is_assigned_twice);
    RUN_TEST(test_pins_are_usable_s3_gpios);
    RUN_TEST(test_knob_direction_is_a_sign);
    RUN_TEST(test_ring_pin_matches_capability);
    return UNITY_END();
}
