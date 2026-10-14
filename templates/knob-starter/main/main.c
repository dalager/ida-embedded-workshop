// main.c — boot order for the knob-starter. Each init owns one piece of hardware; app_init wires
// them together. NVS first (Wi-Fi credentials live there), display before anything that draws,
// backlight right after the display (the panel is dark until the LEDC channel drives it).
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "ui.h"
#include "backlight.h"
#include "haptic.h"
#include "net.h"
#include "app.h"

static const char *TAG = "main";

static void nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
}

static void log_heap(const char *phase)
{
    ESP_LOGI(TAG, "heap[%s]: internal free=%u, psram free=%u", phase,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

void app_main(void)
{
    log_heap("boot");
    nvs_init();
    ui_init();                     // ST77916 + CST816 + LVGL (owns the lock from here on)
    backlight_init();              // panel visible
    haptic_init(ui_i2c_bus());     // DRV2605L on the touch bus; no-op if absent
    net_init();                    // Wi-Fi STA in the background
    app_init();                    // screen + inputs + state machine
    log_heap("after init");
}
