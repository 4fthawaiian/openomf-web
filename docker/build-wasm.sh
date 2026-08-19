#!/bin/bash
# Builds OpenOMF to WebAssembly (WebGL2) inside the emscripten/emsdk container.
# Mounts:
#   /src       -> openomf source (patched)
#   /deps-src  -> dependency tarballs
#   /data-src  -> bundled game data (resources/, shaders/)
#   /shell     -> custom html shell + touch controller
#   /work      -> scratch (builds, wasm sysroot)
#   /output    -> final artifacts (.html/.js/.wasm/.data)
set -euo pipefail

WORK=/work
DEPSRC=/deps-src
PREFIX=$WORK/wasm-sysroot
mkdir -p $PREFIX $WORK/build $WORK/extracted

JOBS=$(nproc)
echo "== Using $(emcc --version | head -1)"

# ---------------------------------------------------------------- deps ----
build_dep_cmake() {
    local name=$1
    shift
    local bdir=$WORK/build/$name
    rm -rf "$bdir"
    mkdir -p "$bdir"
    echo "=== building $name ==="
    emcmake cmake -S "$WORK/extracted/$name" -B "$bdir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$PREFIX \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_PREFIX_PATH=$PREFIX \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        "$@"
    cmake --build "$bdir" -j"$JOBS"
    cmake --install "$bdir"
}

# extract all tarballs
cd "$WORK/extracted"
for f in $DEPSRC/*; do
    case "$f" in
        *.tar.xz) tar xf "$f" ;;
        *.tar.gz) tar xzf "$f" ;;
    esac
done
# canonical source dir names so build_dep_cmake can find them (idempotent)
for n in SDL2 SDL2_mixer libxmp enet confuse zlib libpng; do
    if [ ! -d "$WORK/extracted/$n" ]; then
        mv -f "$WORK/extracted/${n}-"*/ "$WORK/extracted/$n"
    fi
done
# libpng extracts as 'libpng-v1.6.43' (tag prefix), fix that
if [ ! -d "$WORK/extracted/libpng" ] && [ -d "$WORK/extracted/libpng-v"* ]; then
    mv -f "$WORK/extracted/libpng-v"*/ "$WORK/extracted/libpng"
fi
ls -d $WORK/extracted/*/

build_dep_cmake SDL2 \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF \
    -DSDL_SSE=OFF -DSDL_SSE3=OFF -DSDL_SNDIO=OFF

build_dep_cmake libxmp \
    -DBUILD_SHARED=OFF -DBUILD_STATIC=ON -DBUILD_JAVASCRIPT=OFF \
    -DBUILD_EXAMPLES=OFF -DDISABLE_DEPRECATED=OFF

build_dep_cmake enet

build_dep_cmake zlib

build_dep_cmake libpng \
    -DZLIB_INCLUDE_DIR=$PREFIX/include \
    -DZLIB_LIBRARY=$PREFIX/lib/libz.a \
    -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TESTS=OFF

# confuse ships autotools only; build it with emconfigure + the bundled configure
echo "=== building confuse ==="
if [ ! -f "$PREFIX/lib/libconfuse.a" ]; then
    cd "$WORK/extracted/confuse"
    emconfigure ./configure --prefix=$PREFIX --disable-shared --enable-static \
        --disable-nls --disable-dependency-tracking
    emmake make -j"$JOBS"
    make install
    cd "$WORK/extracted"
else
    echo "confuse already built, skipping"
fi

build_dep_cmake SDL2_mixer \
    -DSDL2_LIBRARY=$PREFIX/lib/libSDL2.a \
    -DSDL2_INCLUDE_DIR=$PREFIX/include/SDL2 \
    -DSDL2MIXER_STATIC=ON -DSDL2MIXER_SHARED=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DSDL2MIXER_MOD=OFF \
    -DSDL2MIXER_MOD_XMP=OFF -DSDL2MIXER_MOD_MODPLUG=OFF \
    -DSDL2MIXER_FLAC=OFF -DSDL2MIXER_FLAC_DRFLAC=OFF \
    -DSDL2MIXER_MP3=OFF -DSDL2MIXER_MP3_DRMP3=OFF \
    -DSDL2MIXER_OGG=OFF -DSDL2MIXER_OPUS=OFF \
    -DSDL2MIXER_WAVPACK=OFF -DSDL2MIXER_GME=OFF \
    -DSDL2MIXER_MIDI=OFF -DSDL2MIXER_MIDI_NATIVE=OFF \
    -DSDL2MIXER_MIDI_TIMIDITY=OFF -DSDL2MIXER_STB_VORBIS=OFF

# enet installs to a non-standard subdir; make sure the Find module sees it
if [ -f "$PREFIX/lib/static/libenet.a" ] && [ ! -f "$PREFIX/lib/libenet.a" ]; then
    cp "$PREFIX/lib/static/libenet.a" "$PREFIX/lib/libenet.a"
fi

# --------------------------------------------------- epoxy stub ----
# The renderer only includes <epoxy/gl.h> on the desktop build. In the browser,
# the gl* symbols are provided by Emscripten's WebGL implementation directly, so
# a small header shim + empty archive is enough to satisfy the Find module.
echo "=== creating epoxy shim ==="
mkdir -p "$PREFIX/include/epoxy" "$PREFIX/lib"
cat > "$PREFIX/include/epoxy/gl.h" <<'EOF'
#ifndef EPOXY_GL_H
#define EPOXY_GL_H
#include <stdbool.h>
#include <GLES3/gl3.h>
/* GLES3 / WebGL2 has no normalised GL_RGBA16; use half-float instead. */
#ifndef GL_RGBA16
#define GL_RGBA16 GL_RGBA16F
#endif
#endif
EOF
if [ ! -f "$PREFIX/lib/libepoxy.a" ]; then
    echo 'int epoxy_wasm_dummy;' > "$WORK/epoxy_dummy.c"
    emcc -fPIC -c "$WORK/epoxy_dummy.c" -o "$WORK/epoxy_dummy.o"
    emar rcs "$PREFIX/lib/libepoxy.a" "$WORK/epoxy_dummy.o"
fi

# --------------------------------------------------- openomf ----
echo "=== configuring openomf ==="
OBDIR=$WORK/build/openomf
rm -rf "$OBDIR"
mkdir -p "$OBDIR"
emcmake cmake -S /src -B "$OBDIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=$PREFIX \
    -DCMAKE_FIND_ROOT_PATH=$PREFIX \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DOPENOMF_SHELL_FILE=/shell/shell.html \
    -DOPENOMF_DATA_DIR=/data-src \
    -DUSE_MINIUPNPC=OFF -DUSE_NATPMP=OFF -DUSE_OPUSFILE=OFF \
    -DUSE_TESTS=OFF -DUSE_TOOLS=OFF -DBUILD_LANGUAGES=ON

echo "=== building openomf ==="
cmake --build "$OBDIR" -j"$JOBS"

echo "=== copying artifacts ==="
ls -la "$OBDIR"/openomf.html "$OBDIR"/openomf.js "$OBDIR"/openomf.wasm "$OBDIR"/openomf.data
cp "$OBDIR"/openomf.html "$OBDIR"/openomf.js "$OBDIR"/openomf.wasm "$OBDIR"/openomf.data /output/
echo "=== BUILD OK ==="