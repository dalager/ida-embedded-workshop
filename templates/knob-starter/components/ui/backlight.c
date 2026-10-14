// backlight.c — one LEDC channel on the panel's BLK transistor. 20 kHz keeps the PWM inaudible
// and above the flicker reports some ports have at low frequencies.
#include "backlight.h"
#include "board.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "backlight";

// All on LEDC_LOW_SPEED_MODE (the ESP32-S3 has no high-speed mode).
#define BL_TIMER    LEDC_TIMER_0
#define BL_CHANNEL  LEDC_CHANNEL_0
#define BL_FREQ_HZ  20000u
#define BL_BITS     LEDC_TIMER_10_BIT
#define BL_MAX_DUTY ((1u << 10) - 1u)   // 1023
#define BL_DEFAULT  80u

void backlight_init(void)
{
    ledc_timer_config_t tmr = {
        .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = BL_BITS,
        .timer_num = BL_TIMER, .freq_hz = BL_FREQ_HZ, .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&tmr));

    ledc_channel_config_t ch = {
        .gpio_num = BOARD_LCD_BL, .speed_mode = LEDC_LOW_SPEED_MODE, .channel = BL_CHANNEL,
        .timer_sel = BL_TIMER, .intr_type = LEDC_INTR_DISABLE, .duty = 0, .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));

    backlight_set_percent(BL_DEFAULT);
    ESP_LOGI(TAG, "LEDC on GPIO%d %u Hz 10-bit; level=%u%%", BOARD_LCD_BL, BL_FREQ_HZ, BL_DEFAULT);
}

void backlight_set_percent(uint8_t pct)
{
    if (pct < BACKLIGHT_MIN_PCT) pct = BACKLIGHT_MIN_PCT;
    if (pct > 100)               pct = 100;
    uint32_t duty = (uint32_t)pct * BL_MAX_DUTY / 100u;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_CHANNEL);
}
