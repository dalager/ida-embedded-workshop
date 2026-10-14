// app_upload.c — generates APP_UPLOAD_BYTES of xorshift noise on the fly (nothing is ever buffered
// whole) and pushes it through net_http_put_stream() from a dedicated task with a 6 KB stack —
// enough for esp_http_client + TLS; the LVGL task never blocks on the network.
#include "app_upload.h"
#include "net.h"
#include "wifi_secrets.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>

static const char *TAG = "upload";

typedef struct { uint32_t rng; uint64_t produced; } source_t;

static atomic_bool     s_running;
static atomic_bool     s_done;
static atomic_bool     s_ok;
static _Atomic uint64_t s_sent;

// net calls this before every attempt: a retry re-sends the whole body from byte 0 on a fresh
// connection, so the source must start over. Same seed → same bytes → the receiver can verify.
static void source_rewind(void *ctx)
{
    source_t *s = (source_t *)ctx;
    s->rng = 0x2545F491u;
    s->produced = 0;
}

// Fill `dst` with the next bytes of the stream (0 = end).
static size_t source_read(void *ctx, uint8_t *dst, size_t cap)
{
    source_t *s = (source_t *)ctx;
    uint64_t left = APP_UPLOAD_BYTES - s->produced;
    size_t n = left < cap ? (size_t)left : cap;
    for (size_t i = 0; i < n; i++) {
        s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5;   // xorshift32
        dst[i] = (uint8_t)s->rng;
    }
    s->produced += n;
    return n;
}

static void on_progress(uint64_t sent, uint64_t total)
{
    (void)total;
    atomic_store(&s_sent, sent);
}

// net's result callback (runs on the upload task). Publish, don't draw: the LVGL timer in app.c
// picks these up on the LVGL task.
static void on_result(bool ok)
{
    ESP_LOGI(TAG, "upload %s", ok ? "ok" : "FAILED");
    atomic_store(&s_ok, ok);
    atomic_store(&s_done, true);
}

static void upload_task(void *arg)
{
    (void)arg;
    if (!net_wait_connected(15000)) {
        ESP_LOGW(TAG, "no Wi-Fi after 15 s — giving up");
        on_result(false);
    } else {
        source_t src;
        net_http_put_stream(KNOB_RECEIVER_URL, APP_UPLOAD_BYTES, source_rewind, source_read, &src, on_progress);
    }
    atomic_store(&s_running, false);
    vTaskDelete(NULL);
}

bool app_upload_start(void)
{
    bool expected = false;
    if (!atomic_compare_exchange_strong(&s_running, &expected, true)) return false;
    atomic_store(&s_sent, 0);
    atomic_store(&s_done, false);
    net_set_result_cb(on_result);
    if (xTaskCreate(upload_task, "upload", 6144, NULL, 5, NULL) != pdPASS) {
        atomic_store(&s_running, false);
        return false;
    }
    return true;
}

void app_upload_poll(uint64_t *sent, uint64_t *total, bool *done, bool *ok)
{
    *sent  = atomic_load(&s_sent);
    *total = APP_UPLOAD_BYTES;
    *done  = atomic_exchange(&s_done, false);
    *ok    = atomic_load(&s_ok);
}
