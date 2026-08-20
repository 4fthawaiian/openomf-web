#!/bin/bash
# Downloads and assembles the game data directory (data/) needed for the
# web build. OMF 2097 is freeware; the assets are freely redistributable.
set -euo pipefail
cd "$(dirname "$0")"

# ------------------------------------------------------- game assets ----
if [ -d data/resources ] && [ -f data/resources/openomf.bk ]; then
    echo "Game assets already exist, skipping download."
else
    echo "Downloading OpenOMF assets..."
    mkdir -p /tmp/openomf-assets
    curl -sL --max-time 120 -o /tmp/openomf-assets/openomf-assets.zip \
        https://www.omf2097.com/pub/files/omf/openomf-assets.zip

    echo "Extracting..."
    rm -rf /tmp/openomf-assets/OMF2097
    unzip -o -q /tmp/openomf-assets/openomf-assets.zip -d /tmp/openomf-assets

    echo "Assembling data/ directory..."
    mkdir -p data/resources data/shaders data/mods

    # Game data from the freeware archive
    cp /tmp/openomf-assets/OMF2097/* data/resources/

    # Bundled resources from the source tree
    cp openomf/resources/openomf.bk data/resources/
    cp openomf/resources/gamecontrollerdb/gamecontrollerdb.txt data/resources/
    cp openomf/resources/ENGLISH2.TXT data/resources/
    cp openomf/resources/DANISH.TXT data/resources/
    cp openomf/resources/DANISH2.TXT data/resources/
    cp openomf/resources/GERMAN2.TXT data/resources/

    # Translated WebGL2 shaders
    cp -r openomf/shaders/* data/shaders/
fi

# ------------------------------------------------------- music mods ----
# Download the official music remix mods (by Shady Monk and DeBisco) from
# the OpenOMF desktop release. These are .zip mod files containing .ogg
# (Opus) remixes of the original MOD soundtracks.
OPENOMF_RELEASE="0.8.6"
if [ -d data/mods ] && ls data/mods/*.zip >/dev/null 2>&1; then
    echo "Music mods already present, skipping."
else
    echo "Downloading music remix mods from OpenOMF ${OPENOMF_RELEASE} release..."
    mkdir -p data/mods /tmp/openomf-release
    curl -sL --max-time 120 -o /tmp/openomf-release/openomf.tar.gz \
        "https://github.com/omf2097/openomf/releases/download/${OPENOMF_RELEASE}/openomf_${OPENOMF_RELEASE}_linux_amd64.tar.gz"
    tar xzf /tmp/openomf-release/openomf.tar.gz \
        -C /tmp/openomf-release --strip-components=3 \
        './usr/share/games/openomf/mods/'
    cp /tmp/openomf-release/mods/*.zip data/mods/ 2>/dev/null || true
    rm -rf /tmp/openomf-release
fi

echo "Done. data/ is ready."
