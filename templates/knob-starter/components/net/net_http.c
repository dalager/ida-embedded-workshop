// net_http.c — two HTTP primitives built on esp_http_client: a buffered GET for small bodies and a
// streamed PUT for big ones. The streamed path is the lesson: ONE shared 16 KB PSRAM chunk, a
// declared Content-Length, exact byte accounting, fresh connection per retry, and heap logging so
// you can watch what Wi-Fi + TLS cost in internal RAM.
#include "net.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "net";

static net_result_cb_t s_result_cb;
void net_set_result_cb(net_result_cb_t cb) { s_result_cb = cb; }

#define GET_TIMEOUT_MS        8000
#define PUT_TIMEOUT_MS        60000   // a multi-MB body over flaky Wi-Fi needs far more than 8 s
#define PUT_MAX_ATTEMPTS      3
#define PUT_RETRY_DELAY_MS    400

// Internal-RAM posture at a named phase. mbedTLS buffers and lwIP share this heap with everything
// else, so the MINIMUM free size is the number that tells you whether you are about to fail.
static void log_internal_heap(const char *phase)
{
    ESP_LOGI(TAG, "heap[%s]: internal free=%u min=%u largest=%u", phase,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
}

// ------------------------------------------------------------------- GET --

typedef struct { char *buf; size_t cap; size_t len; } body_ctx_t;

// Event handler: append body bytes while they fit; silently drop the rest (the caller sized buf).
static esp_err_t get_evt(esp_http_client_event_t *e)
{
    body_ctx_t *c = (body_ctx_t *)e->user_data;
    if (!c || e->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
    size_t room = c->cap - 1 - c->len;
    size_t n = (size_t)e->data_len < room ? (size_t)e->data_len : room;
    memcpy(c->buf + c->len, e->data, n);
    c->len += n;
    c->buf[c->len] = '\0';
    return ESP_OK;
}

int net_http_get(const char *url, char *buf, size_t cap)
{
    if (!buf || cap == 0) return -1;
    body_ctx_t body = { .buf = buf, .cap = cap, .len = 0 };
    buf[0] = '\0';
    esp_http_client_config_t cfg = {
        .url = url, .method = HTTP_METHOD_GET, .timeout_ms = GET_TIMEOUT_MS,
        .event_handler = get_evt, .user_data = &body,
        .crt_bundle_attach = esp_crt_bundle_attach,   // https validated against the built-in bundle
    };
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    if (!cli) return -1;
    int status = -1;
    if (esp_http_client_perform(cli) == ESP_OK) status = esp_http_client_get_status_code(cli);
    esp_http_client_cleanup(cli);
    return status;
}

// ------------------------------------------------------------ streamed PUT --

typedef struct {
    const char *url;
    uint64_t total;
    net_rewind_cb_t rewind_cb;
    net_read_cb_t read_cb;
    void *ctx;
    net_progress_cb_t progress_cb;
    uint8_t *chunk;      // the one shared NET_CHUNK buffer
} put_t;

// Pull from read_cb and write to the socket until `total` bytes are out. A short read is an error:
// the server was promised Content-Length bytes and will reject anything else.
static bool put_body(esp_http_client_handle_t cli, const put_t *p)
{
    uint64_t sent = 0;
    while (sent < p->total) {
        uint64_t left = p->total - sent;
        size_t want = left < NET_CHUNK ? (size_t)left : NET_CHUNK;
        size_t got = p->read_cb(p->ctx, p->chunk, want);
        if (got == 0) { ESP_LOGE(TAG, "put: source ran dry with %llu B left", (unsigned long long)left); return false; }
        int wrote = esp_http_client_write(cli, (const char *)p->chunk, got);
        if (wrote != (int)got) { ESP_LOGE(TAG, "put: write %d/%u", wrote, (unsigned)got); return false; }
        sent += got;
        if (p->progress_cb) p->progress_cb(sent, p->total);
    }
    return true;
}

// open (TCP connect + TLS handshake, timed) → body → status. One attempt, one connection.
static bool put_once(const put_t *p, int attempt)
{
    esp_http_client_config_t cfg = {
        .url = p->url, .method = HTTP_METHOD_PUT, .timeout_ms = PUT_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 1024, .buffer_size_tx = 2048,   // long URLs + headers overflow the 512 B default
    };
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    if (!cli) return false;
    esp_http_client_set_header(cli, "Content-Type", "application/octet-stream");
    p->rewind_cb(p->ctx);   // every attempt re-sends the whole body from byte 0

    int64_t t0 = esp_timer_get_time();
    // open() declares Content-Length == total. Do NOT also set_post_field: that is the buffered path.
    esp_err_t err = esp_http_client_open(cli, (int)p->total);
    int64_t handshake_ms = (esp_timer_get_time() - t0) / 1000;
    bool ok = false;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "put[%d]: open() failed after %lld ms (connect+TLS)", attempt, (long long)handshake_ms);
    } else if (put_body(cli, p)) {
        // fetch_headers() is mandatory after the write loop: it reads the status line.
        if (esp_http_client_fetch_headers(cli) >= 0) {
            int st = esp_http_client_get_status_code(cli);
            ok = st >= 200 && st < 300;
            if (!ok) ESP_LOGW(TAG, "put[%d]: HTTP %d", attempt, st);
        }
    }
    int64_t total_ms = (esp_timer_get_time() - t0) / 1000;
    ESP_LOGI(TAG, "put[%d]: handshake=%lld ms total=%lld ms bytes=%llu (%u KB/s)", attempt,
             (long long)handshake_ms, (long long)total_ms, (unsigned long long)p->total,
             total_ms > 0 ? (unsigned)(p->total / 1024 * 1000 / (uint64_t)total_ms) : 0u);
    esp_http_client_cleanup(cli);
    return ok;
}

static bool put_args_ok(const char *url, uint64_t total_len, net_rewind_cb_t rewind_cb,
                        net_read_cb_t read_cb)
{
    return url && rewind_cb && read_cb && total_len > 0;
}

bool net_http_put_stream(const char *url, uint64_t total_len, net_rewind_cb_t rewind_cb,
                         net_read_cb_t read_cb, void *ctx, net_progress_cb_t progress_cb)
{
    if (!put_args_ok(url, total_len, rewind_cb, read_cb)) return false;
    put_t p = { .url = url, .total = total_len, .rewind_cb = rewind_cb, .read_cb = read_cb,
                .ctx = ctx, .progress_cb = progress_cb };
    // PSRAM, on purpose: the internal heap is what lwIP and mbedTLS fight over.
    p.chunk = heap_caps_malloc(NET_CHUNK, MALLOC_CAP_SPIRAM);
    if (!p.chunk) { ESP_LOGE(TAG, "put: chunk alloc failed"); return false; }

    log_internal_heap("before put");
    bool ok = false;
    for (int attempt = 1; attempt <= PUT_MAX_ATTEMPTS && !ok; attempt++) {
        ok = put_once(&p, attempt);
        if (!ok && attempt < PUT_MAX_ATTEMPTS) {
            ESP_LOGW(TAG, "put: retrying in %d ms", PUT_RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(PUT_RETRY_DELAY_MS));
        }
    }
    log_internal_heap("after put");
    heap_caps_free(p.chunk);
    if (s_result_cb) s_result_cb(ok);
    return ok;
}
