#!/bin/bash
# Builds OpenOMF for the web (WebAssembly + WebGL2).
# Outputs: output/openomf.{html,js,wasm,data}
set -euo pipefail
cd "$(dirname "$0")"

# Ensure game data is present
if [ ! -f data/resources/openomf.bk ]; then
    echo "Game data not found, downloading..."
    bash prepare-data.sh
fi

exec bash "$PWD/docker/run-build.sh"