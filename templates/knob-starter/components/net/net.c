// net.c — Wi-Fi lifecycle: driver init, STA join, reconnect-on-disconnect, RSSI, and the NVS-backed
// credential override. HTTP lives in net_http.c.
#include "net.h"
#include "wifi_secrets.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "net";

#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t s_wifi_eg;

// Credentials saved by net_wifi_join() (NVS namespace "wifi"). True when a non-empty SSID exists.
static bool load_creds(char *ssid_out, size_t ssid_cap, char *pass_out, size_t pass_cap)
{
    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READONLY, &h) != ESP_OK) return false;
    bool ok = (nvs_get_str(h, "ssid", ssid_out, &ssid_cap) == ESP_OK && ssid_out[0] != '\0');
    if (ok && nvs_get_str(h, "pass", pass_out, &pass_cap) != ESP_OK) pass_out[0] = '\0';
    nvs_close(h);
    return ok;
}

static void save_creds(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, "ssid", ssid);
    nvs_set_str(h, "pass", pass);
    nvs_commit(h);
    nvs_close(h);
}

// The whole reconnect policy: on start connect, on disconnect connect again. The Wi-Fi driver
// rate-limits this itself; for a battery device you would add a backoff here.
static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_eg, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "disconnected — reconnecting");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "got IP " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_wifi_eg, WIFI_CONNECTED_BIT);
    }
}

static void apply_sta_config(const char *ssid, const char *pass)
{
    wifi_config_t wc = {0};
    snprintf((char *)wc.sta.ssid,     sizeof(wc.sta.ssid),     "%s", ssid);
    snprintf((char *)wc.sta.password, sizeof(wc.sta.password), "%s", pass);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
}

void net_init(void)
{
    esp_err_t nv = nvs_flash_init();   // idempotent if app_main already did it
    if (nv == ESP_ERR_NVS_NO_FREE_PAGES || nv == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    s_wifi_eg = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

    char nv_ssid[33] = {0}, nv_pass[65] = {0};
    bool has_nv = load_creds(nv_ssid, sizeof(nv_ssid), nv_pass, sizeof(nv_pass));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    apply_sta_config(has_nv ? nv_ssid : KNOB_WIFI_SSID, has_nv ? nv_pass : KNOB_WIFI_PASS);
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi STA starting (ssid=%s [%s])", has_nv ? nv_ssid : KNOB_WIFI_SSID,
             has_nv ? "NVS" : "wifi_secrets.h");
}

bool net_is_connected(void)
{
    return s_wifi_eg && (xEventGroupGetBits(s_wifi_eg) & WIFI_CONNECTED_BIT);
}

bool net_wait_connected(uint32_t timeout_ms)
{
    if (!s_wifi_eg) return false;
    EventBits_t bits = xEventGroupWaitBits(s_wifi_eg, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE,
                                           pdMS_TO_TICKS(timeout_ms));
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

int net_rssi(void)
{
    wifi_ap_record_t ap;
    return (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) ? ap.rssi : 0;
}

bool net_wifi_join(const char *ssid, const char *pass)
{
    if (!ssid || ssid[0] == '\0') { ESP_LOGE(TAG, "join: empty ssid"); return false; }
    if (!pass) pass = "";
    apply_sta_config(ssid, pass);
    if (esp_wifi_disconnect() != ESP_OK) return false;   // the event handler reconnects with the new config
    ESP_LOGI(TAG, "join: ssid='%s', reconnecting", ssid);  // password intentionally not logged
    save_creds(ssid, pass);
    return true;
}
