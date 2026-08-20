# OpenOMF Web — One Must Fall 2097 in your browser

A WebAssembly + WebGL2 port of [OpenOMF](https://github.com/omf2097/openomf),
the open-source remake of the 1994 fighting game *One Must Fall: 2097*.

Runs in any modern browser with WebGL2 support — desktop or mobile. Includes
an on-screen touch controller for phones and tablets.

![One Must Fall 2097 running in a browser with the on-screen touch controller](screenshot.png)

## Quick start

```bash
# 1. Build (needs Docker)
./build.sh

# 2. Serve
./serve.sh

# 3. Play
# Open http://localhost:9090/openomf.html
```

That's it. The first build downloads the freeware game assets, compiles all
dependencies (SDL2, SDL2_mixer, libxmp, libconfuse, enet, zlib, libpng) to
WASM, patches the game source for WebGL2, and produces the final
`output/openomf.{html,js,wasm,data}` artifacts.

## Requirements

- **Docker** — the build runs inside the `emscripten/emsdk` container so you
  don't need to install Emscripten locally.
- **Python 3** — for the dev server (`serve.sh`).
- **A WebGL2 browser** — Chrome, Firefox, Safari 15+, Edge.

## What was changed

The original OpenOMF is a desktop C/SDL2/OpenGL 3.3 game. This repo adds the
patches and build infrastructure to cross-compile it to the web.

`openomf/` is a **git subtree** of [`omf2097/openomf`](https://github.com/omf2097/openomf)
with a small patch set applied on top. The full patch lives in
`patches/omf-web-overrides.patch` (git-applyable from the repo root).

### Keeping in sync with upstream

```bash
git subtree pull --prefix openomf https://github.com/omf2097/openomf master --squash
git apply patches/omf-web-overrides.patch   # if it no longer applies, fix the conflicts
./build.sh                                   # rebuild + retest
git add -A && git commit -m "Sync upstream; reapply web overrides" && git push
```

Upstream changes rarely touch the web layer, so the patch usually applies
unmodified; when it doesn't, the conflicts are confined to the files below.

### Source patches (`openomf/`)

| File | Change |
|---|---|
| `src/engine.c` | Refactored the blocking game loop into `emscripten_set_main_loop` so the browser stays responsive |
| `src/main.c` | Deferred `engine_close()` on Emscripten (main returns before the loop ends); forced stderr logging for diagnostics |
| `src/video/renderers/opengl3/sdl_window.c` | Request GLES 3.0 / WebGL2 context under `__EMSCRIPTEN__` instead of GL 3.3 core |
| `src/video/renderers/opengl3/helpers/vbo.c` | Emulated `glMapBufferRange` with a CPU staging buffer (WebGL2 has no buffer mapping) |
| `src/video/renderers/opengl3/helpers/object_array.c` | Unrolled `glMultiDrawArrays` into a loop (not in GLES3 core) |
| `src/video/renderers/opengl3/helpers/render_target.c` | Use `GL_RGBA16F` + `GL_FLOAT` for the paletted framebuffer (GLES3 has no normalised `GL_RGBA16`) |
| `src/audio/music_sources/opus_source.c` | Added stub `opus_load_memory` when opusfile is disabled |
| `cmake-scripts/BuildLanguages.cmake` | Run the languagetool via node cross-compiling emulator under Emscripten |
| `CMakeLists.txt` | Target-specific Emscripten link flags (WebGL2, filesystem, shell, preload); `AUTOLOAD_DYLIBS=0` for tools |
| `src/console/console.c` | Disable debug-console stdin reads on Emscripten (browser `read(0)` fires `window.prompt` dialogs) |
| `shaders/*.vert`, `shaders/*.frag` | Translated all 12 shaders from GLSL 330 core to GLSL ES 3.00 (precision qualifiers, `texture` instead of `textureLod` for `usampler2D`, moved bool initializers into `main`, etc.) |

### Build infrastructure

| File | Purpose |
|---|---|
| `build.sh` | Host entry point — ensures data exists, then launches the Docker build |
| `prepare-data.sh` | Downloads the freeware game assets and assembles `data/` |
| `serve.sh` | Dev server with correct MIME types (`.wasm` → `application/wasm`) |
| `docker/build-wasm.sh` | Runs inside the Emscripten container: builds all deps, then OpenOMF, copies artifacts. Also applies a WASM-only patch to libconfuse: its `cfg_yylex_destroy` return-type mismatch (`void` decl vs `int` def in the flex lexer) is benign natively but makes `cfg_free()` trap on WASM |
| `docker/run-build.sh` | Docker wrapper with volume mounts |
| `shell/shell.html` | Custom HTML shell with loading screen, `ENV` setup, virtual filesystem dirs, touch controller, IDBFS-persisted dropped mods |

Dependency tarballs go in `deps-src/`. Beyond the original set, the build needs
three more for music-mod support:

| Tarball | Source |
|---|---|
| `libogg-1.3.5.tar.gz` | https://downloads.xiph.org/releases/ogg/libogg-1.3.5.tar.gz |
| `opus-1.5.2.tar.gz` | https://downloads.xiph.org/releases/opus/opus-1.5.2.tar.gz |
| `opusfile-0.12.tar.gz` | https://downloads.xiph.org/releases/opus/opusfile-0.12.tar.gz |

### Touch controls

The on-screen controller maps to the game's keyboard inputs:

- **Left stick** — movement (up/down/left/right arrows, 8-way)
- **KICK** — Right Shift
- **PUNCH** — Enter
- **JUMP** — Arrow Up
- **ESC** — Escape (pause / back)

On desktop, the physical keyboard works as normal.

## Mods

The build bundles the official music remix mods (by Shady Monk and DeBisco)
from the [OpenOMF soundtrack](https://github.com/omf2097/openomf-music-mod).
Toggle between original MOD music and remixes in the in-game audio menu.

You can also install your own mods: **drag any `.zip` mod file onto the page**
and it's saved to IndexedDB and loaded on the next page reload. Mods can
replace sprites, backgrounds, pilot portraits, animation data, and music —
see the [OpenOMF mod
docs](https://github.com/omf2097/openomf/releases/tag/0.8.6) for the format.

Dropped mods persist across reloads (via IndexedDB). To remove a mod, clear
the site's IndexedDB data in your browser settings.

## Audio notes

- **iOS / iPadOS**: audio unlocks on the first tap. If you hear nothing, make
  sure the physical **mute switch** (above the volume buttons, or Control
  Center) is off — iOS silently mutes all web audio when it is enabled.
- Browsers require a user gesture before audio starts; the game resumes SDL's
  AudioContext on tap/click.

## Project structure

```
wasm-omf/
├── build.sh              # entry point
├── prepare-data.sh       # downloads freeware assets
├── serve.sh              # dev server
├── .gitignore
├── patches/
│   └── omf-web-overrides.patch   # the 21-file WebGL2 override patch
├── docker/
│   ├── build-wasm.sh     # container build script
│   └── run-build.sh      # docker wrapper
├── shell/
│   └── shell.html        # HTML shell + touch controller
├── openomf/              # git subtree of omf2097/openomf + overrides
│   ├── src/
│   ├── shaders/          # GLSL ES 3.00 shaders
│   ├── cmake-scripts/
│   ├── CMakeLists.txt
│   └── ...
├── data/                 # assembled game data (not in repo)
├── deps-src/             # downloaded dep tarballs (not in repo)
├── build/                # Docker build scratch (not in repo)
├── output/               # final web artifacts (not in repo)
└── web/                  # served copy of the artifacts
```

## Credits

- [OpenOMF](https://github.com/omf2097/openomf) by Tuomas Virtanen, Andrew
  Thompson, Hunter and contributors — MIT License.
- *One Must Fall: 2097* by Diversions Entertainment (1994) — freeware.
- Build dependencies: SDL2, SDL2_mixer, libxmp, libconfuse, enet, zlib, libpng,
  libogg, libopus, opusfile, libepoxy (shimmed).
- Music remixes by Shady Monk and DeBisco (bundled as mods from
  [openomf-music-mod](https://github.com/omf2097/openomf-music-mod)).

## License

The patches in this repo are MIT. OpenOMF itself is MIT. The game data is
freeware from the original publisher.
