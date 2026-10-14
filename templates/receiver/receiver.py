#!/usr/bin/env python3
"""Reference upload receiver for the knob-starter — Python 3 stdlib only, no install.

    python3 receiver.py [--port 8080] [--dir ./received]

    PUT /upload/<name>      body with Content-Length → written to <dir>/<name> (truncated unless X-Offset is set, then appended at that offset)
                            optional header  X-Offset: <bytes>  → write from that offset (resume)
    GET /upload/<name>      → {"received": <bytes on disk>}   (ask before resuming)
    GET /                   → {"ok": true}                    (connectivity check)

Prints bytes and MB/s per request. Point KNOB_RECEIVER_URL in wifi_secrets.h at
http://<this machine's LAN IP>:8080/upload/knob.bin and keep the laptop on the same Wi-Fi.
"""
import argparse, json, os, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

CHUNK = 64 * 1024


class Handler(BaseHTTPRequestHandler):
    root = "./received"

    def _path(self):
        name = os.path.basename(self.path.split("?", 1)[0])
        if not self.path.startswith("/upload/") or not name:
            return None
        return os.path.join(self.root, name)

    def _json(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/":
            return self._json(200, {"ok": True})
        p = self._path()
        if p is None:
            return self._json(404, {"error": "use /upload/<name>"})
        size = os.path.getsize(p) if os.path.exists(p) else 0
        self._json(200, {"received": size})

    def do_PUT(self):
        p = self._path()
        if p is None:
            return self._json(404, {"error": "use /upload/<name>"})
        length = int(self.headers.get("Content-Length", "0"))
        offset = int(self.headers.get("X-Offset", "0"))
        os.makedirs(self.root, exist_ok=True)
        t0 = time.time()
        got = 0
        mode = "r+b" if (offset and os.path.exists(p)) else "wb"
        with open(p, mode) as f:
            f.seek(offset)
            while got < length:
                data = self.rfile.read(min(CHUNK, length - got))
                if not data:
                    break
                f.write(data)
                got += len(data)
        dt = max(time.time() - t0, 1e-6)
        print(f"PUT {os.path.basename(p)} offset={offset} bytes={got}/{length} "
              f"{got / dt / 1e6:.2f} MB/s in {dt:.1f} s", flush=True)
        if got != length:
            return self._json(400, {"error": "short body", "received": got})
        self._json(200, {"received": offset + got})

    def log_message(self, *_):  # keep stdout to the one line per request above
        pass


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--dir", default="./received")
    a = ap.parse_args()
    Handler.root = a.dir
    srv = ThreadingHTTPServer(("0.0.0.0", a.port), Handler)
    print(f"receiver on http://0.0.0.0:{a.port}/upload/<name>  → {a.dir}/", flush=True)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
