#pragma once
// net — Wi-Fi station + a small HTTP client that is honest about memory. Depends on: esp_wifi,
// esp_http_client, mbedtls (cert bundle), nvs_flash, wifi_secrets.h. Internal RAM is the scarce
// resource here: lwIP and mbedTLS allocate from it, so this module streams through ONE shared
// 16 KB chunk in PSRAM and never buffers a whole payload. Every function is blocking and must be
// called from your own task — never from an LVGL callback (the UI would freeze for seconds).
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Size of the one shared streaming buffer (PSRAM). Peak heap of an upload is independent of
// payload size because of it.
#define NET_CHUNK 16384

// ------------------------------------------------------------------ Wi-Fi --

// Start Wi-Fi STA with credentials from NVS (saved by net_wifi_join) or wifi_secrets.h, and keep
// reconnecting in the background. Returns immediately; poll net_is_connected().
void net_init(void);

// True once the station has an IP.
bool net_is_connected(void);

// Block until connected or `timeout_ms` elapses; true when connected.
bool net_wait_connected(uint32_t timeout_ms);

// Current AP signal strength in dBm (0 if not connected).
int net_rssi(void);

// Switch to another network: reconfigures the driver, reconnects, persists the credentials to NVS
// (namespace "wifi") for the next boot. The result is observable via net_is_connected() over the
// following seconds. Never logs the password.
bool net_wifi_join(const char *ssid, const char *pass);

// ------------------------------------------------------------------- HTTP --

// GET `url` into buf[cap] (NUL-terminated; bytes past cap-1 are dropped). Returns the HTTP status,
// or -1 on transport failure. 8 s timeout. Good for small JSON; never for a file.
int net_http_get(const char *url, char *buf, size_t cap);

// Supplies the next bytes of the body: copy up to `cap` bytes into dst, return how many (0 = end).
typedef size_t (*net_read_cb_t)(void *ctx, uint8_t *dst, size_t cap);
// Called after every chunk hits the socket, on the calling task. Keep it cheap; no LVGL here
// unless you take ui_lock().
typedef void (*net_progress_cb_t)(uint64_t sent, uint64_t total);
// Called before EVERY attempt (including the first): reset the source to byte 0. A retry runs on
// a fresh connection and re-sends the whole body, so a source that cannot rewind cannot retry.
typedef void (*net_rewind_cb_t)(void *ctx);

// Stream exactly `total_len` bytes to `url` with HTTP PUT and a declared Content-Length, pulling
// them from `read_cb` through the shared NET_CHUNK buffer. Retries the whole transfer up to 3
// times: each attempt is a fresh connection and starts with `rewind_cb(ctx)`, so the source must
// be restartable. 60 s socket timeout. Logs the internal-RAM low-water mark before and after so
// you can see what TLS costs. Returns true on HTTP 2xx, and also reports the outcome through the
// result callback below.
bool net_http_put_stream(const char *url, uint64_t total_len, net_rewind_cb_t rewind_cb,
                         net_read_cb_t read_cb, void *ctx, net_progress_cb_t progress_cb);

// Result callback: fired on the uploading task when net_http_put_stream() finishes (either way).
// This is the app↔net seam: net knows nothing about screens; the app registers a handler and
// decides what to show. The handler must not block and must not touch LVGL without ui_lock().
typedef void (*net_result_cb_t)(bool ok);
void net_set_result_cb(net_result_cb_t cb);
