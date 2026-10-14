---
name: backend-integration-patterns
description: >
  An architecture that works for connecting a small device to a backend: pairing a device to a
  user with a short code (RFC 8628 device flow), bearer tokens with refresh, resumable
  multipart upload through presigned URLs, an asynchronous processing pipeline with a
  reconciler, and one wire contract that generates both the server types and the firmware
  header. Use only when someone asks for a backend, a server, an API, authentication, "where
  do I send this", "how does the device log in", or how to process uploads after they land.
  Presented as patterns from a production device, not as the only way.
---

# Backend integration patterns for a connected device

These patterns come from a production recorder on this board that uploads audio to a cloud
backend, gets it transcribed and summarised, and shows the result on the device. They are
vendor-neutral; the last section shows one concrete mapping.

## 1. One contract, generated into both worlds

The most drift-prone facts between firmware (C) and backend (TypeScript) are small literals:
part size, codec, sample rate, route paths, status enums. Keep them in **one package** and
generate from it:

```
packages/contract/
  routes.ts       "/device/pair/start", "/device/pair/poll", "/v1/recordings", ...
  values.ts       PART_SIZE = 8 MiB, CODEC, SAMPLE_RATE, status enums
  auth.ts         pairing + token shapes (schema objects)
  recordings.ts   open / mint part / ack / resume / status / detail shapes
  scripts/gen-c-header.ts   → firmware/main/include/generated/contract.h
```

The schema library (Zod here) validates every request at the edge, emits TS types for the
web app, a JSON Schema for anyone else, and a C header with `#define CONTRACT_PART_SIZE
(8*1024*1024)` and the route strings. The firmware is a conformist: it includes the generated
header and never hand-copies a path. Run the generator before every firmware build.

## 2. Pairing: device flow with a short code (RFC 8628, adapted)

A device with no keyboard proves it belongs to a person like this:

```
device  → POST /device/pair/start {device_id, nonce, fw_version}
backend → {device_code (secret, device-held), user_code (short, shown on screen),
           verification_uri, qr_payload, interval, expires_in}
device  → shows user_code + QR; polls POST /device/pair/poll {device_code} every `interval` s
person  → opens verification_uri on a phone, logs in, types or scans user_code, approves
backend → poll answers "approved" + {access_token, refresh_token, account_id, display_name}
```

Poll statuses: `authorization_pending`, `slow_down`, `denied`, `expired`, `approved`.

Design points that mattered:

- **Possession is the credential.** Holding the device inside a live pairing window is how a
  person gets admitted; there is no separate invite code. A reported-stolen device goes on a
  denylist checked at `pair/start`.
- The `user_code` is resolved server-side to the pending session; it is never a forgeable
  query parameter.
- The device shows the code and the QR; the `device_code` is never displayed.
- Pairing state lives in one object per session (a Durable Object in the mapping below), so
  poll and approve cannot race.

## 3. Tokens: bearer + refresh, and the lesson

Device tokens are a bearer `access_token` with a TTL (30 days in the reference system) and a
`refresh_token`, scoped to `{device_id, account_id}`, revocable server-side.

**The lesson (a real incident).** The firmware stored the access token, discarded what it
needed to refresh, never refreshed, and did not react to `401`. Thirty days after pairing every
upload failed while the screen still said "linked", and the device had to be re-paired by
hand. Rules that follow:

- Refresh **before** expiry on a schedule *and* on the first `401`.
- Keep whatever the refresh endpoint needs (`refresh_token`, or the `device_code` if the
  design uses it) sealed at rest, not discarded after approval.
- A `401` the refresh cannot fix means **unlinked**: show it, prompt re-pairing, stop claiming
  success.
- Seal tokens at rest under a device-held key (the reference uses the PIN-derived key); a
  dumped flash must not yield a usable token.
- Never send the device bearer to a third-party host (presigned storage URLs authenticate
  themselves).

## 4. Upload: resumable multipart through presigned URLs

The device never holds storage credentials. The backend mints one presigned URL per part.

```
POST /v1/recordings            {recording_id, size, sha256, codec, part_size}  → {recording_pk}
POST .../parts                 {part_number}            → {url, expires_in}     (mint)
PUT  url                       body = exactly part_size bytes (last part smaller)
POST .../parts/ack             {part_number, etag}
GET  .../parts                 → {received_parts: [1, 2, 5]}                    (resume)
POST .../complete
```

- Parts are equal-sized (8 MiB; object stores require ≥ 5 MiB) except the last; the device
  streams each one from storage through a 16 KB chunk (see
  `networking-under-memory-constraints`).
- `recording_id` is derived deterministically on the device (device id + card generation +
  file name), so re-opening after a crash is **idempotent**: the same id resumes the same
  session instead of creating a duplicate.
- Resume means: open again, `GET` received parts, skip them, continue. Same-boot and
  cross-reboot use the same path.
- The whole-file SHA-256 travels in the open request as provenance, computed in the same
  streaming pass.

## 5. After the upload: async pipeline with a reconciler

The device must never wait on processing. The invariant that made the reference system
reliable: **every hop is asynchronous, and every non-terminal state is reconcilable. No fixed
timeout may be the only path to a terminal state.**

```
complete → queue message → worker submits to the ASR service → webhook on finish
         → transcript stored → summariser runs → status row updated
device   → GET /v1/recordings/{id} on detail-open and in a boot sweep
         → pulls /transcript and /summary when their status is terminal
cron     → every 5 min: find rows stuck in a non-terminal state, re-drive them
```

- **Submit atomically.** Persist the external job id in the same write that marks the row
  "processing". A submit with no job id is a failure, not an ack.
- **Webhooks are idempotent** and answer 2xx on any handled outcome; 5xx only when your own
  persistence failed, so the sender retries.
- **Heavy work off the ack path.** Return 2xx, then summarise in the background; the webhook
  sender's timeout is shorter than an LLM call.
- **The device fetch is decoupled from the upload.** Fetching a late result is two GETs, never
  a re-upload. A small local marker per recording records what has been fetched.
- **The reconciler is one idempotent sweep**, not a parallel mechanism per state.

## 6. Status model the device can render

`GET /v1/recordings/{id}` returns two small enums the UI maps straight onto colour = state:

| Facet | Values |
|---|---|
| `transcript_status` | `uploaded`, `queued`, `transcribing`, `done`, `error` |
| `summary_status` | `none`, `pending`, `ready`, `error` |

Terminal = `done`/`error` and `ready`/`error`/`none`. The device stops polling only at terminal.

## 7. One concrete mapping (Cloudflare, for reference)

| Need | Service | Why |
|---|---|---|
| HTTP edge, routing, validation | Workers | One Worker, modules per bounded context |
| Pairing session, upload session | Durable Objects | Single-writer state per session; no races on poll/ack |
| Recordings, accounts, status rows | D1 (SQLite) | Relational, cheap, queryable by the reconciler |
| Audio parts, transcripts | R2 | S3-compatible multipart with presigned part URLs |
| Token denylist, brand config | KV | Read-mostly, global |
| Transcription jobs | Queues + a cron Worker | At-least-once delivery plus the 5-minute reconciler |
| ASR | External GPU service via webhook | Long-running; never awaited inline |
| Summaries | AI Gateway → a hosted model | Per-account prompts, budget caps |

Auth across the edge: device bearer for `/device/*` and `/v1/*`; email OTP + session cookie
for humans; account genesis only inside a live pairing window.

Any stack with an HTTP edge, a durable per-session object or a transactional table, an
S3-compatible store, a queue and a scheduler can host the same patterns.

## 8. Minimal backend for the workshop

If all that is needed today is "somewhere to send bytes": `templates/receiver/receiver.py`.
Single file, stdlib only: `PUT /upload/<name>` appends (with `X-Offset` for resume),
`GET /upload/<name>` returns `{"received": n}`. Graduate to §4 when a second device or a
real user appears.
