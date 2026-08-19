#!/bin/bash
# Host-side wrapper: runs the OpenOMF -> WebAssembly build in an emscripten
# container. Results land in ./output/ (serve this directory over http).
set -euo pipefail

cd "$(dirname "$0")/.."

if ! docker info >/dev/null 2>&1; then
    echo "Docker is not running." >&2
    exit 1
fi

mkdir -p output build

exec docker run --rm \
    -v "$PWD/openomf:/src" \
    -v "$PWD/deps-src:/deps-src" \
    -v "$PWD/data:/data-src" \
    -v "$PWD/shell:/shell" \
    -v "$PWD/build:/work" \
    -v "$PWD/output:/output" \
    -v "$PWD/docker/build-wasm.sh:/scripts/build-wasm.sh:ro" \
    emscripten/emsdk:latest \
    bash /scripts/build-wasm.sh