#!/bin/bash
# Downloads and assembles the game data directory (data/) needed for the
# web build. OMF 2097 is freeware; the assets are freely redistributable.
set -euo pipefail
cd "$(dirname "$0")"

if [ -d data/resources ] && [ -f data/resources/openomf.bk ]; then
    echo "Data directory already exists, skipping."
    exit 0
fi

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

echo "Done. data/ is ready."
