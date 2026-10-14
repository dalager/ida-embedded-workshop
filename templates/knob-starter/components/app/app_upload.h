#pragma once
// app_upload — the upload exercise (private to the app component): streams a deterministic
// pseudo-random payload to KNOB_RECEIVER_URL on its own FreeRTOS task. Depends on net only.
// Progress and result are published through atomics; the LVGL task polls them (see app.c).
#include <stdbool.h>
#include <stdint.h>

#define APP_UPLOAD_BYTES (2u * 1024u * 1024u)   // 2 MiB: far more than the internal heap

// Kick off one upload. Returns false if one is already running.
bool app_upload_start(void);

// Snapshot for the UI. `*done` flips true exactly once per upload, with `*ok` the outcome.
void app_upload_poll(uint64_t *sent, uint64_t *total, bool *done, bool *ok);
