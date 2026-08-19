#!/bin/bash
# Serves the OpenOMF web build with correct MIME types.
# Usage: ./serve.sh [port]
set -euo pipefail
cd "$(dirname "$0")"
PORT="${1:-9090}"

if [ ! -f openomf.html ]; then
    echo "openomf.html not found. Run ./build.sh first." >&2
    exit 1
fi

# Python http.server with wasm MIME type
exec python3 -c "
import http.server, socketserver

class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        '.wasm': 'application/wasm',
        '.data': 'application/octet-stream',
        '.js': 'application/javascript',
    }
    def end_headers(self):
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        super().end_headers()

with socketserver.TCPServer(('', $PORT), Handler) as s:
    print(f'Serving OpenOMF at http://localhost:$PORT/')
    s.serve_forever()
"