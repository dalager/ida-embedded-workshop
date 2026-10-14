---
name: networking-under-memory-constraints
description: >
  Wi-Fi and HTTP on the ESP32-S3 knob board without running out of internal RAM: join a
  network, fetch JSON, and stream multi-megabyte uploads through one 16 KB chunk with resume
  and bounded retries. Use whenever firmware talks to anything over the network, when an
  upload or download is designed, when TLS is added, when the board reboots or stalls during a
  transfer, when "out of memory", "ESP_ERR_NO_MEM", "Out of buffer", "tcp_write failed" or a
  task watchdog appears while networking, and for exercise rungs 7–10. Built from a production
  recorder on this board that streams multi-megabyte audio to the cloud.
---

# Networking under memory constraints

The board has 8 MB of PSRAM and almost no spare internal SRAM once Wi-Fi, lwIP, mbedTLS and
LVGL are up. Every rule here comes from a failure the reference firmware had on this exact
board while uploading recordings over HTTPS.

## 1. The three rules

1. **Stream, never buffer.** One shared chunk, reused for everything. Peak heap must be
   independent of payload size.
2. **Internal RAM is what runs out.** Networking buffers *must* be internal (DMA, lwIP,
   mbedTLS). Everything else must get out of the way.
3. **Chunk, resume, bound.** Fixed-size parts, ask the server what it already has, retry a
   few times with a delay, then give up loudly.

## 2. Make room: sdkconfig and placement

These lines in `sdkconfig.defaults` are the difference between an upload that works and one
that stalls at 8 KB free:

```
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y   # big static arrays may live in PSRAM
CONFIG_LV_MEM_SIZE_KILOBYTES=128                 # LVGL pool, placed in PSRAM via LV_ATTRIBUTE_LARGE_RAM_ARRAY
CONFIG_LWIP_TCP_SND_BUF_DEFAULT=16384            # default 5760 B = more fill/drain cycles
CONFIG_LWIP_TCP_WND_DEFAULT=16384
```

The observed failure: with LVGL's 128 KB pool in internal SRAM, an HTTPS PUT logged
**free-internal 8 KB, largest block 1.5 KB** and lwIP could not allocate its send buffer.
Moving the pool to PSRAM fixed it, and only then was a 16 KB TCP window allocatable.

Log the posture at named phases and keep the numbers in your notes:

```c
static void log_internal_heap(const char *phase) {
    ESP_LOGI(TAG, "heap[%s]: free-internal=%u min-free-internal=%u", phase,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),   // esp_heap_caps.h
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
}
```

Call it after `net_init()`, before the first TLS session, and after each part. The
**min-free-internal** value is your floor; a feature that pushes it under ~20 KB will fail in
the field even if it passes on the bench.

## 3. Wi-Fi station (what `net.c` does)

- `esp_netif` + default event loop + `esp_wifi` in STA mode; credentials from NVS if a
  previous `net_wifi_join()` stored them, else from `wifi_secrets.h`.
- Reconnect loop in the event handler: `WIFI_EVENT_STA_DISCONNECTED` → `esp_wifi_connect()`
  again, forever. An event-group bit set on `IP_EVENT_STA_GOT_IP` is what `net_is_connected()`
  reads; `net_wait_connected(timeout_ms)` blocks a worker on it.
- Start SNTP once on the first IP if you need real timestamps.
- `net_rssi()` reads `esp_wifi_sta_get_ap_info()`; 0 means not associated.
- Never call `net_wifi_join()` or a scan from the LVGL task; a blocking scan leaves the channel
  for hundreds of milliseconds.

Secrets: commit `wifi_secrets.example.h`, gitignore `wifi_secrets.h`, never log the password.

## 4. A streamed PUT (the shape of `net_http_put_stream`)

```c
// One 16 KB chunk in PSRAM, allocated once per transfer (or once at init) and reused.
#define NET_CHUNK (16 * 1024)
uint8_t *chunk = heap_caps_malloc(NET_CHUNK, MALLOC_CAP_SPIRAM);

esp_http_client_config_t cfg = {
    .url = url, .method = HTTP_METHOD_PUT,
    .timeout_ms = 60000,          // a streamed body needs far more than the 8 s used for JSON
    .buffer_size = 1024,          // RX: response headers
    .buffer_size_tx = 2048,       // TX: a long URL overflows the 512 B default ("Out of buffer")
    .crt_bundle_attach = esp_crt_bundle_attach,   // https only; harmless for http
};
esp_http_client_handle_t cli = esp_http_client_init(&cfg);
esp_http_client_set_header(cli, "Content-Type", "application/octet-stream");

// open() declares Content-Length; do NOT also set a post field (that is the buffered path).
if (esp_http_client_open(cli, (int)total_len) != ESP_OK) goto fail;
uint64_t remaining = total_len;
while (remaining) {
    size_t want = remaining < NET_CHUNK ? (size_t)remaining : NET_CHUNK;
    size_t got  = read_cb(ctx, chunk, want);              // from SD, a generator, a ring...
    if (!got) goto fail;                                  // short source: abort, never pad
    if (esp_http_client_write(cli, (const char *)chunk, got) != (int)got) goto fail;
    remaining -= got;
    if (progress_cb) progress_cb(total_len - remaining, total_len);
}
if (esp_http_client_fetch_headers(cli) < 0) goto fail;    // mandatory: reads status + headers
int status = esp_http_client_get_status_code(cli);
esp_http_client_cleanup(cli);
```

Points that bit the reference firmware:

- The server rejects a body whose length differs from the declared `Content-Length`. Write
  exactly `total_len` bytes or abort.
- `fetch_headers()` after the write loop is mandatory; skipping it leaves status 0.
- A retry re-sends the whole body on a fresh connection, so the source must be restartable.
  The template's `net_http_put_stream` takes a `rewind_cb` and calls it before every attempt;
  a source that has already run dry turns the second attempt into "source ran dry".
- A half-written stream cannot be resumed on the same client. Each retry uses a **fresh client
  and a fresh source position**.
- `esp_http_client_open()` includes TCP connect and the full TLS handshake; time it. On this
  board a TLS handshake takes on the order of a second and tens of KB of internal heap while it runs; measure it.

## 5. Parts, resume, retries

Keep the arithmetic pure and tested (`upload_plan.c`, `test_upload_plan.c`):

```c
upload_plan_t plan = upload_plan(file_size, PART_SIZE);   // n_parts, last_part_size
uint64_t off = part_offset(pn, PART_SIZE);                 // (pn-1) * PART_SIZE
uint64_t len = part_len(pn, &plan, PART_SIZE);             // PART_SIZE except the last
```

Protocol shape the reference firmware uses (8 MiB parts, matching object-store multipart):
open → for each part: mint URL → PUT → ack ETag → complete. Before re-sending after a failure,
`GET` the list of received parts and skip them. The receiver in `templates/receiver/receiver.py`
offers the minimal version: `PUT /upload/<name>` with an optional `X-Offset` header appends
from that offset, `GET /upload/<name>` answers `{"received": n}`.

Bounds that work: **3 attempts per part, 400 ms apart; 3 attempts for the whole transfer,
1 s apart; 60 s request timeout**; 15 s wait for Wi-Fi before starting. A device that is
genuinely offline must surrender in seconds, not minutes, and report it.

Resume across reboots needs a durable marker of what was sent (a small file or NVS key per
transfer); the server's view of received parts is the truth.

## 6. TLS budget

- `esp_crt_bundle_attach` is the easy path; it costs flash, not RAM. A pinned single
  certificate saves flash and a little handshake time.
- mbedTLS allocates from the internal heap by default (`CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC`).
  If the floor is still too low after §2, try `CONFIG_MBEDTLS_DYNAMIC_BUFFER=y` (frees buffers
  between handshakes) or `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y` (PSRAM; slower handshake).
- One TLS connection at a time. Two concurrent handshakes is the fastest way to find the floor.
- Session reuse (`keep_alive_enable`) saves the handshake on consecutive parts if the server
  allows it.

## 7. Task hygiene

- One network task, 8 KB stack with TLS, priority below the LVGL port task. Jobs arrive on a
  queue; results go back through a callback registered by `app` (`net_set_result_cb`) and are
  applied to the UI under `ui_lock()`.
- Never network inside an LVGL event handler or timer.
- Progress is a number, not a string: pass `sent/total`, let the UI task format it.
- The reference firmware tripped the task watchdog when the app task spun in `lv_malloc`
  while the port task held the lock during an upload. If you see `task_wdt`, look for a lock
  held across I/O.

## 8. Exercise ladder (network half)

7. **Join Wi-Fi and show RSSI.** The template already shows `wi-fi N dBm` from its 200 ms
   tick. Move it to its own 1 s `lv_timer`, append "weak" below −75 dBm, and log the internal
   heap once after the first IP (`#include "esp_heap_caps.h"`).
8. **GET a JSON value and render it.** `net_http_get(url, buf, cap)` from the network task,
   `cJSON` parse, push one number to the UI via callback. Keep `buf` ≤ 4 KB and in PSRAM.
9. **Stream ≥ 2 MB with a progress arc.** Start `receiver.py` on the laptop; generate the
   payload in `read_cb` (a counter pattern, no file needed); `net_http_put_stream()` with a
   progress callback that updates an `lv_arc`. Log heap before, during and after; note the
   min-free-internal.
10. **Survive a Wi-Fi drop.** Pull the AP mid-transfer. Expect: the PUT fails, the reconnect
    loop rejoins, the retry asks the receiver how much it has and resumes from that offset,
    the arc continues. No reboot, no leak (heap after == heap before).

## 9. Checklist for any networking change

- [ ] Peak internal heap independent of payload size (one shared chunk).
- [ ] `min-free-internal` logged and above the floor you set.
- [ ] Every request has a timeout; every retry is bounded and delayed.
- [ ] Fresh client per attempt; no reuse of a half-written stream.
- [ ] Nothing network-related runs on the LVGL task.
- [ ] Secrets out of git; password never logged.
- [ ] Resume tested by actually breaking the link once.
