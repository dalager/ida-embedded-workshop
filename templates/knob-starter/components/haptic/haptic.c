// haptic.c — DRV2605L bring-up + effect trigger. Register map from TI SLOS854; LRA library 6.
#include "haptic.h"
#include "board.h"
#include "esp_log.h"

static const char *TAG = "haptic";

#if BOARD_HAS_HAPTIC

// DRV2605L registers (TI SLOS854).
#define EFFECT_STRONG_CLICK 1
#define REG_MODE      0x01   // 0x00 = internal trigger, out of standby
#define REG_LIBRARY   0x03   // 6 = LRA library
#define REG_WAVESEQ1  0x04
#define REG_WAVESEQ2  0x05
#define REG_GO        0x0C
#define REG_FEEDBACK  0x1A   // bit7 N_ERM_LRA = 1 (LRA); lower bits keep the 0x36 reset defaults
#define LRA_LIBRARY   6
#define FEEDBACK_LRA  0xB6
#define I2C_HZ        100000
#define I2C_TMO_MS    50

static i2c_master_dev_handle_t s_dev;   // NULL = haptics unavailable on this unit

static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    uint8_t b[2] = { reg, val };
    return i2c_master_transmit(s_dev, b, sizeof(b), I2C_TMO_MS);
}

static bool configure_lra(void)
{
    return reg_write(REG_MODE, 0x00) == ESP_OK &&
           reg_write(REG_FEEDBACK, FEEDBACK_LRA) == ESP_OK &&
           reg_write(REG_LIBRARY, LRA_LIBRARY) == ESP_OK;
}

void haptic_init(i2c_master_bus_handle_t bus)
{
    if (!bus || i2c_master_probe(bus, BOARD_HAPTIC_I2C_ADDR, I2C_TMO_MS) != ESP_OK) {
        ESP_LOGW(TAG, "no DRV2605L at 0x%02X — haptics off", BOARD_HAPTIC_I2C_ADDR);
        return;
    }
    i2c_device_config_t dc = { .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                               .device_address = BOARD_HAPTIC_I2C_ADDR, .scl_speed_hz = I2C_HZ };
    if (i2c_master_bus_add_device(bus, &dc, &s_dev) != ESP_OK || !configure_lra()) {
        ESP_LOGW(TAG, "DRV2605L setup failed — haptics off");
        s_dev = NULL;
        return;
    }
    ESP_LOGI(TAG, "DRV2605L ready (LRA library %d)", LRA_LIBRARY);
}

// Queue a single effect (sequence slot 2 = end marker) and trigger it.
static bool start_effect(uint8_t effect)
{
    return reg_write(REG_WAVESEQ1, effect) == ESP_OK &&
           reg_write(REG_WAVESEQ2, 0) == ESP_OK &&
           reg_write(REG_GO, 1) == ESP_OK;
}

void haptic_play(uint8_t effect)
{
    if (!s_dev || !effect) return;
    if (!start_effect(effect)) ESP_LOGW(TAG, "effect %u failed", (unsigned)effect);
}

void haptic_click(void) { haptic_play(EFFECT_STRONG_CLICK); }

#else  // !BOARD_HAS_HAPTIC

void haptic_init(i2c_master_bus_handle_t bus) { (void)bus; ESP_LOGI(TAG, "board has no haptic driver"); }
void haptic_play(uint8_t effect) { (void)effect; }
void haptic_click(void) {}

#endif
