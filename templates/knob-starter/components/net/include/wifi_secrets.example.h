#pragma once
// Copy this file to `wifi_secrets.h` (gitignored — never commit the real one) and fill it in.
// Credentials saved later with net_wifi_join() live in NVS and override these at boot.

#define KNOB_WIFI_SSID      "your-ssid"
#define KNOB_WIFI_PASS      "your-password"

// Where the upload exercise streams to: the reference receiver on your laptop
// (`python3 templates/receiver/receiver.py --port 8080`), reachable from the same Wi-Fi.
// Plain http is fine on a LAN; https works too (TLS validated against the mbedTLS cert bundle).
#define KNOB_RECEIVER_URL   "http://192.168.1.10:8080/upload/knob.bin"
