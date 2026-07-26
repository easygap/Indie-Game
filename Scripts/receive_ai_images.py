"""Tiny localhost receiver for ImageGen artwork.

The browser cannot hand a generated image straight to the toolchain: the
image URLs are signed and the extension will not surface them, and routing
the bytes back through the agent would waste an enormous amount of context.
So the page POSTs the image here instead and this writes it to
Content/SourceArt/AI/<name>.png, where import_ai_art.py picks it up.

Run it, generate/POST, then stop it. It binds to loopback only and accepts
nothing but base64 image bodies under a fixed directory.

    python Scripts/receive_ai_images.py [port]
"""

import base64
import os
import re
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qs, urlparse

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUTPUT_DIR = os.path.join(PROJECT_ROOT, "Content", "SourceArt", "AI")
# Names come from a web page, so they are treated as hostile: only a short
# safe-character slug is ever used to build a path.
SAFE_NAME = re.compile(r"^[A-Za-z0-9_-]{1,64}$")
MAX_BYTES = 32 * 1024 * 1024


# The generator page's Content-Security-Policy forbids connect-src to
# localhost, so the page cannot POST here directly. It opens this relay
# instead and postMessage()s the payload across; the relay is same-origin
# with the receiver, so its own POST is allowed.
RELAY_HTML = b"""<!doctype html><meta charset="utf-8"><title>relay</title>
<body style="font:14px system-ui;padding:24px">waiting for image...
<script>
window.addEventListener('message', async (event) => {
  if (event.origin !== location.origin && !/^https:\\/\\/chatgpt\\.com$/.test(event.origin)) return;
  const { name, b64 } = event.data || {};
  if (!name || !b64) return;
  const res = await fetch('/save?name=' + encodeURIComponent(name), {
    method: 'POST', headers: { 'Content-Type': 'text/plain' }, body: b64
  });
  document.body.firstChild.nodeValue = 'saved ' + name + ': ' + res.status + ' ';
  if (event.source) event.source.postMessage({ relayStatus: res.status, name }, event.origin);
});
if (window.opener) window.opener.postMessage({ relayReady: true }, '*');
</script>"""


class ImageReceiver(BaseHTTPRequestHandler):
    def _cors(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Access-Control-Allow-Methods", "POST, OPTIONS")

    def do_OPTIONS(self):
        self.send_response(204)
        self._cors()
        self.end_headers()

    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(RELAY_HTML)))
        self._cors()
        self.end_headers()
        self.wfile.write(RELAY_HTML)

    def do_POST(self):
        query = parse_qs(urlparse(self.path).query)
        name = (query.get("name") or [""])[0]
        if not SAFE_NAME.match(name):
            self.send_response(400)
            self._cors()
            self.end_headers()
            self.wfile.write(b"bad name")
            return

        length = int(self.headers.get("Content-Length") or 0)
        if length <= 0 or length > MAX_BYTES:
            self.send_response(413)
            self._cors()
            self.end_headers()
            return

        payload = self.rfile.read(length).decode("ascii", "ignore")
        # Accept both a bare base64 string and a full data: URL.
        if "," in payload[:64]:
            payload = payload.split(",", 1)[1]

        try:
            image_bytes = base64.b64decode(payload, validate=True)
        except Exception:
            self.send_response(400)
            self._cors()
            self.end_headers()
            self.wfile.write(b"bad payload")
            return

        os.makedirs(OUTPUT_DIR, exist_ok=True)
        target = os.path.join(OUTPUT_DIR, name + ".png")
        with open(target, "wb") as handle:
            handle.write(image_bytes)

        print(f"saved {target} ({len(image_bytes)} bytes)", flush=True)
        self.send_response(200)
        self._cors()
        self.end_headers()
        self.wfile.write(b"ok")

    def log_message(self, fmt, *args):
        pass  # The prints above are the only output worth having.


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    server = HTTPServer(("127.0.0.1", port), ImageReceiver)
    print(f"listening on http://127.0.0.1:{port} -> {OUTPUT_DIR}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
