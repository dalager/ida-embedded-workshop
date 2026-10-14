// ui.c — panel + touch + LVGL bring-up. Order matters: QSPI bus → panel IO → ST77916 init →
// esp_lvgl_port (tick, render task, lock) → touch indev. Every pin comes from board.h.
#include "ui.h"
#include "board.h"
#include "ui_panel_init_cmds.h"   // ST77916 vendor init table (pure data)
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st77916.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "ui";

#define LCD_HOST        SPI2_HOST
#define LCD_H_RES       360
#define LCD_V_RES       360
#define LCD_BPP         16
// Draw-buffer height. 40 rows x 360 px x 2 B = 28.8 KB per buffer, two buffers, INTERNAL RAM:
// the active DMA buffer must not live in PSRAM (QSPI/DMA stability), and the QSPI bus — not the
// CPU — is the frame-rate bottleneck, so bigger buffers buy nothing. Partial refresh only.
#define LCD_DRAW_ROWS   40
#define LCD_TRANS_SZ    (LCD_H_RES * LCD_DRAW_ROWS * sizeof(uint16_t))

#define TOUCH_I2C_HOST  I2C_NUM_0

static esp_lcd_touch_handle_t   s_touch;
static i2c_master_bus_handle_t  s_i2c_bus;   // shared with the haptic driver via ui_i2c_bus()

i2c_master_bus_handle_t ui_i2c_bus(void) { return s_i2c_bus; }
bool ui_lock(uint32_t timeout_ms)         { return lvgl_port_lock(timeout_ms); }
void ui_unlock(void)                      { lvgl_port_unlock(); }

// ---------------------------------------------------------------- display --

// QSPI bus + ST77916 panel IO.
static esp_lcd_panel_io_handle_t panel_io_init(void)
{
    spi_bus_config_t buscfg = ST77916_PANEL_BUS_QSPI_CONFIG(
        BOARD_LCD_SCK, BOARD_LCD_D0, BOARD_LCD_D1, BOARD_LCD_D2, BOARD_LCD_D3, LCD_TRANS_SZ);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_config = ST77916_PANEL_IO_QSPI_CONFIG(BOARD_LCD_CS, NULL, NULL);
    io_config.trans_queue_depth = 2;   // let the next buffer queue while one flushes
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io));
    return io;
}

// ST77916 panel driver with the vendor init table: reset → init → display on. The table already
// inverts colours (0x21), so esp_lcd_panel_invert_color() must NOT be called as well.
static esp_lcd_panel_handle_t panel_init(esp_lcd_panel_io_handle_t io)
{
    st77916_vendor_config_t vendor_config = {
        .init_cmds = ui_st77916_init_cmds,
        .init_cmds_size = ui_st77916_init_cmds_count,
        .flags = { .use_qspi_interface = 1 },
    };
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BPP,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77916(io, &panel_config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    return panel;
}

// LVGL via esp_lvgl_port: it owns the tick, the lv_timer_handler task and the flush path.
static lv_display_t *lvgl_init(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t panel)
{
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = LCD_H_RES * LCD_DRAW_ROWS,
        .double_buffer = true,      // render the next dirty region while the previous one flushes
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,   // draw buffers stay in internal RAM (see LCD_DRAW_ROWS)
            .swap_bytes = true,     // RGB565 byte order for the SPI panel
        },
    };
    return lvgl_port_add_disp(&disp_cfg);
}

// ------------------------------------------------------------------ touch --

// I2C0 master bus for the touch controller (and the haptic driver that shares the wires).
static i2c_master_bus_handle_t touch_bus_init(void)
{
    i2c_master_bus_config_t i2c_cfg = {
        .i2c_port = TOUCH_I2C_HOST,
        .sda_io_num = BOARD_TOUCH_SDA,
        .scl_io_num = BOARD_TOUCH_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = 1 },
    };
    i2c_master_bus_handle_t bus = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_cfg, &bus));
    return bus;
}

// One-shot inventory so a missing peripheral shows up in the boot log, not as a silent no-op.
static void i2c_scan(i2c_master_bus_handle_t bus)
{
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        if (i2c_master_probe(bus, addr, 50) == ESP_OK) {
            ESP_LOGI(TAG, "I2C0 scan: device at 0x%02X%s", addr,
                     addr == BOARD_HAPTIC_I2C_ADDR ? " (DRV2605L haptic)" : "");
        }
    }
}

// CST816 controller on the bus; returns its panel IO so we can poke a register.
static esp_lcd_panel_io_handle_t touch_controller_init(i2c_master_bus_handle_t bus)
{
    esp_lcd_panel_io_handle_t tio = NULL;
    esp_lcd_panel_io_i2c_config_t tio_cfg = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(bus, &tio_cfg, &tio));

    esp_lcd_touch_config_t tcfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = BOARD_TOUCH_RST,
        .int_gpio_num = BOARD_TOUCH_INT,
        .levels = { .reset = 0, .interrupt = 0 },
        .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tio, &tcfg, &s_touch));
    ESP_LOGI(TAG, "CST816 touch up (I2C0 sda=%d scl=%d int=%d rst=%d)",
             BOARD_TOUCH_SDA, BOARD_TOUCH_SCL, BOARD_TOUCH_INT, BOARD_TOUCH_RST);
    return tio;
}

static void touch_init(lv_display_t *disp)
{
    s_i2c_bus = touch_bus_init();
    i2c_scan(s_i2c_bus);
    esp_lcd_panel_io_handle_t tio = touch_controller_init(s_i2c_bus);
    // Disable CST816 auto-sleep (reg 0xFE=1): otherwise the first touch after idle only wakes the
    // controller, adding latency before LVGL sees the press. Done before the port starts polling.
    esp_lcd_panel_io_tx_param(tio, 0xFE, (uint8_t[]){0x01}, 1);

    const lvgl_port_touch_cfg_t touch_cfg = { .disp = disp, .handle = s_touch };
    lv_indev_t *indev = lvgl_port_add_touch(&touch_cfg);
    if (indev) lv_indev_set_long_press_time(indev, 700);   // a deliberate hold, not a slow tap
}

// ------------------------------------------------------------------ public --

void ui_init(void)
{
    ESP_LOGI(TAG, "display bring-up (%s): ST77916 QSPI %dx%d", BOARD_NAME, LCD_H_RES, LCD_V_RES);
    esp_lcd_panel_io_handle_t io = panel_io_init();
    esp_lcd_panel_handle_t panel = panel_init(io);
    lv_display_t *disp = lvgl_init(io, panel);
    if (disp == NULL) { ESP_LOGE(TAG, "lvgl_port_add_disp failed"); return; }
    touch_init(disp);
    ESP_LOGI(TAG, "display + touch up");
}
