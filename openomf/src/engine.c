#include "engine.h"
#include "audio/audio.h"
#include "console/console.h"
#include "controller/controller.h"
#include "formats/altpal.h"
#include "formats/rec.h"
#include "game/game_player.h"
#include "game/game_state.h"
#include "game/gui/osd/osd.h"
#include "game/utils/settings.h"
#include "resources/languages.h"
#include "resources/modmanager.h"
#include "resources/resource_files.h"
#include "resources/script_cache.h"
#include "resources/sounds_loader.h"
#include "utils/allocator.h"
#include "utils/log.h"
#include "utils/miscmath.h"
#include "utils/png_writer.h"
#include "utils/time_fmt.h"
#include "video/vga_state.h"
#include "video/video.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define MAX_TICKS_PER_FRAME 10
#define TICK_EXPIRY_MS 100

static int run = 0;
static int start_timeout = 30;
static int enable_screen_updates = 1;
static int debug_palette_number = 0;

int engine_init(const engine_init_flags *init_flags) {
    const settings *setting = settings_get();

    const int w = setting->video.screen_w;
    const int h = setting->video.screen_h;
    const int fs = setting->video.fullscreen;
    const int vsync = setting->video.vsync;
    const int aspect = setting->video.aspect;
    const int fb_scale = setting->video.fb_scale;
    const int scaling_mode = setting->video.scaling_mode;
    const int framerate_limit = setting->video.framerate_limit;
    const int frequency = setting->sound.sample_rate;
    const int resampler = setting->sound.music_resampler;
    const bool mono = setting->sound.music_mono;
    const float music_volume = setting->sound.music_vol / 10.0;
    const float sound_volume = setting->sound.sound_vol / 10.0;
    const char *player = setting->sound.player;
    const char *renderer = setting->video.renderer;

    if(strlen(init_flags->force_audio_backend) > 0) {
        player = init_flags->force_audio_backend;
    }
    if(strlen(init_flags->force_renderer) > 0) {
        renderer = init_flags->force_renderer;
    }

    // Initialize everything.
    video_scan_renderers();
    audio_scan_backends();
    if(!video_init(renderer, w, h, fs, vsync, aspect, framerate_limit, fb_scale, scaling_mode)) {
        goto exit_0;
    }
    if(!audio_init(player, frequency, mono, resampler, music_volume, sound_volume)) {
        goto exit_1;
    }
    if(!sounds_loader_init()) {
        goto exit_2;
    }
    if(!lang_init()) {
        goto exit_3;
    }
    if(!fonts_init()) {
        goto exit_4;
    }
    if(altpals_init()) {
        goto exit_5;
    }
    if(!console_init()) {
        goto exit_6;
    }
    if(!osd_init()) {
        goto exit_7;
    }
    if(!modmanager_init()) {
        goto exit_8;
    }
    vga_state_init();
    script_cache_init();

    // Return successfully
    run = 1;
    log_info("Engine initialization successful.");
    return 0;

    // If something failed, close in correct order
exit_8:
    osd_close();
exit_7:
    console_close();
exit_6:
    altpals_close();
exit_5:
    fonts_close();
exit_4:
    lang_close();
exit_3:
    sounds_loader_close();
exit_2:
    audio_close();
exit_1:
    video_close();
exit_0:
    return 1;
}

void save_screenshot(const SDL_Rect *r, unsigned char *data, bool flip) {
    char *time = format_time();
    path filename = get_screenshot_filename(time);
    if(write_rgb_png(&filename, r->w, r->h, data, false, flip)) {
        log_info("Got a screenshot: %s", path_c(&filename));
    } else {
        log_error("Screenshot write operation failed (%s)", path_c(&filename));
    }
    omf_free(time);
}

void save_palette_shot(void) {
    char *time = format_time();
    char *filename = omf_malloc(256);
    snprintf(filename, 256, "debug_palette_%s_%d.png", time, debug_palette_number++);
    path tmp;
    path_from_c(&tmp, filename);
    vga_state_debug_screenshot(&tmp);
    log_info("Palette saved: %s", path_c(&tmp));
    omf_free(filename);
    omf_free(time);
}

void save_rec(game_state *gs) {
    char *time = format_time();
    path filename = get_snapshot_rec_filename(time);
    sd_rec_finish(gs->rec, gs->tick);
    sd_rec_save(gs->rec, &filename);
    log_info("REC saved: %s", path_c(&filename));
    omf_free(time);
}

// Shared game loop state. The loop is either driven by a blocking while loop
// (native) or by a requestAnimationFrame callback (Emscripten).
typedef struct {
    SDL_Event event;
    bool check_fs;
    int visual_debugger;
    int debugger_proceed;
    int debugger_render;
    uint64_t mouse_visible_ticks;
    uint64_t frame_start;
    int dynamic_wait;
    int static_wait;
    game_state *gs;
} engine_loop_state;

// One iteration of the game loop: handle events, tick the simulation and
// render a single frame.
static void engine_loop_iteration(engine_loop_state *st) {
    SDL_Event *e = &st->event;

    // Handle events
    while(SDL_PollEvent(e)) {
        // Handle other events
        switch(e->type) {
            case SDL_QUIT:
                run = 0;
                break;
            case SDL_KEYDOWN:
                if(e->key.keysym.sym == SDLK_F1) {
                    video_schedule_screenshot(save_screenshot);
                }
                if(e->key.keysym.sym == SDLK_F2) {
                    save_palette_shot();
                }
                if(e->key.keysym.sym == SDLK_F3) {
                    if(st->gs->rec) {
                        save_rec(st->gs);
                    }
                }
                if(e->key.keysym.sym == SDLK_F9) {
                    video_draw_atlas(true);
                }
                if(e->key.keysym.sym == SDLK_F10) {
                    video_draw_atlas(false);
                }
                if(e->key.keysym.sym == SDLK_F5) {
                    st->visual_debugger = !st->visual_debugger;
                }
                if(st->visual_debugger && !console_window_is_open() && e->key.keysym.sym == SDLK_SPACE) {
                    st->dynamic_wait += 20;
                    st->static_wait += 20;
                } else if(st->visual_debugger && !console_window_is_open() &&
                          (e->key.keysym.sym >= SDLK_1 && e->key.keysym.sym <= SDLK_9)) {
                    st->debugger_proceed = 1 + e->key.keysym.sym - SDLK_1;
                }
                if(!console_window_is_open() && e->key.keysym.sym == SDLK_BACKSPACE) {
                    if(game_state_get_player(st->gs, 0)->ctrl->type == CTRL_TYPE_REC) {
                        controller_rewind(game_state_get_player(st->gs, 0)->ctrl);
                        controller_rewind(game_state_get_player(st->gs, 1)->ctrl);
                        if(st->gs->new_state) {
                            // one of the controllers wants to replace the game state
                            game_state *old_gs = st->gs;
                            game_state *new_gs = st->gs->new_state;
                            st->gs = new_gs;
                            game_state_clone_free(old_gs);
                            omf_free(old_gs);

                            // apply palette transforms
                            game_state_palette_transform(st->gs);
                            vga_state_render();
                        }
                        st->visual_debugger = 1;
                    }
                }
                if(e->key.keysym.sym == SDLK_F6) {
                    st->debugger_render = !st->debugger_render;
                }
                break;
            case SDL_JOYDEVICEADDED:
                joystick_deviceadded(e->jdevice.which);
                break;
            case SDL_JOYDEVICEREMOVED:
                joystick_deviceremoved(e->jdevice.which);
                break;
            case SDL_MOUSEMOTION:
                st->mouse_visible_ticks = 1000;
                SDL_ShowCursor(1);
                break;
            case SDL_WINDOWEVENT:
                switch(e->window.event) {
                    case SDL_WINDOWEVENT_MINIMIZED:
                        log_debug("MINIMIZED");
                        enable_screen_updates = 0;
                        break;
                    case SDL_WINDOWEVENT_HIDDEN:
                        log_debug("HIDDEN");
                        enable_screen_updates = 0;
                        break;
                    case SDL_WINDOWEVENT_MAXIMIZED:
                        log_debug("MAXIMIZED");
                        enable_screen_updates = 1;
                        break;
                    case SDL_WINDOWEVENT_RESTORED:
                        video_get_state(NULL, NULL, &st->check_fs, NULL, NULL, NULL);
                        if(st->check_fs) {
                            video_reinit_renderer();
                        }
                        log_debug("RESTORED");
                        enable_screen_updates = 1;
                        break;
                    case SDL_WINDOWEVENT_SHOWN:
                        enable_screen_updates = 1;
                        log_debug("SHOWN");
                        break;
                }
                break;
        }

        // Console events
        if(e->type == SDL_KEYDOWN) {
            if(console_window_is_open() &&
               (e->key.keysym.scancode == SDL_SCANCODE_GRAVE || e->key.keysym.sym == SDLK_BACKQUOTE ||
                e->key.keysym.sym == SDLK_TAB || e->key.keysym.sym == SDLK_ESCAPE)) {
                console_window_close();
                continue;
            } else if(e->key.keysym.sym == SDLK_TAB || e->key.keysym.sym == SDLK_BACKQUOTE ||
                      e->key.keysym.scancode == SDL_SCANCODE_GRAVE) {
                console_window_open();
                continue;
            }
        }

        // If console windows is open, pass events to console.
        // Otherwise to the objects.
        if(console_window_is_open()) {
            console_event(st->gs, e);
        } else {
            game_state_handle_event(st->gs, e);
        }
    }

    // hide mouse after n ticks
    if(st->mouse_visible_ticks > 0) {
        st->mouse_visible_ticks -= SDL_GetTicks64() - st->frame_start;
        if(st->mouse_visible_ticks <= 0) {
            SDL_ShowCursor(0);
        }
    }

    // Render scene
    uint64_t frame_dt = SDL_GetTicks64() - st->frame_start;
    st->frame_start = SDL_GetTicks64();
    if(!st->visual_debugger) {
        st->dynamic_wait += frame_dt;
        st->static_wait += frame_dt;
    } else {
        console_tick(st->gs);
    }

    // drop ticks if it's been too long since they were due
    st->dynamic_wait = min2(st->dynamic_wait, TICK_EXPIRY_MS);
    st->static_wait = min2(st->static_wait, TICK_EXPIRY_MS);

    // In warp mode, allow more ticks to happen per vsync period.
    bool has_dynamic = true;
    bool has_static = true;
    int tick_limit = MAX_TICKS_PER_FRAME;
    do {
        int dyntick_ms = game_state_ms_per_dyntick(st->gs);
        if(st->debugger_proceed > 0) {
            st->dynamic_wait += dyntick_ms;
            st->static_wait += STATIC_TICKS;
            st->debugger_proceed--;
        }

        // Tick static features. This is a fixed with-rate tick, and is meant for running things
        // that are not dependent on game speed (such as menus).
        has_static = st->static_wait > STATIC_TICKS;
        if(has_static) {
            game_state_static_tick(st->gs, false);
            // check if we need to replace the game state
            if(st->gs->new_state) {
                // one of the controllers wants to replace the game state
                game_state *old_gs = st->gs;
                game_state *new_gs = st->gs->new_state;
                st->gs = new_gs;
                // st->gs->new_state = NULL;
                game_state_clone_free(old_gs);
                omf_free(old_gs);
            }
            console_tick(st->gs);
            osd_tick();
            st->static_wait -= STATIC_TICKS;
        }

        // Tick dynamic features. This is a dynamically changing tick, and it depends on things such as
        // hit-pause, hit slowdown and game-speed slider. It is meant for ticking everything that has to do
        // with the actual gameplay stuff.
        has_dynamic = st->dynamic_wait > dyntick_ms;
        if(has_dynamic) {
            game_state_dynamic_tick(st->gs, false);
            st->dynamic_wait -= dyntick_ms;
            if(st->gs->delay > 0) {
                log_debug("applying delay %d", st->gs->delay);
                SDL_Delay(4);
                st->gs->delay--;
                st->dynamic_wait -= 4;
            }
        }

        // Ensure any pending palette changes are handled after any ticks are made.
        if(has_dynamic || has_static) {
            game_state_palette_transform(st->gs);
            vga_state_render();
        }
    } while(tick_limit-- && (has_dynamic || has_static));

    // Do the actual video rendering jobs
    if(enable_screen_updates) {
        video_render_prepare(game_state_get_framebuffer_options(st->gs));
        game_state_render(st->gs);
        if(st->debugger_render) {
            game_state_debug(st->gs);
        }
        osd_render();
        console_render();
        video_render_finish();
    } else {
        // If screen updates are disabled, then wait
        SDL_Delay(1);
    }
}

static void engine_loop_finish(engine_loop_state *st) {
    joystick_close();

    // Free scene object
    game_state_free(&st->gs);

    log_info(" --- END GAME LOG ---");
}

#ifdef __EMSCRIPTEN__
static engine_loop_state web_loop_state;

static void emscripten_tick(void) {
    engine_loop_state *st = &web_loop_state;
    if(!run || !game_state_is_running(st->gs)) {
        run = 0;
        emscripten_cancel_main_loop();
        engine_loop_finish(st);
        engine_close();
        return;
    }
    engine_loop_iteration(st);
}
#endif

void engine_run(const engine_init_flags *init_flags) {
    SDL_Event e;

    log_info(" --- BEGIN GAME LOG ---");

    // Game start timeout.
    // Wait a moment so that people are mentally prepared
    // (with the recording software on) for the game to start :)
    if(!settings_get()->video.crossfade_on) {
        start_timeout = 0;
    }
    while(start_timeout > 0) {
        start_timeout--;
        while(SDL_PollEvent(&e)) {
            if(e.type == SDL_QUIT) {
                return;
            }
        }
        unsigned framebuffer_options = 0;
        video_render_prepare(framebuffer_options);
        video_render_finish();
    }

    // apply volume settings
    audio_set_sound_volume(settings_get()->sound.sound_vol / 10.0f);

    // Set up game
    engine_loop_state st;
    memset(&st, 0, sizeof(st));
    st.mouse_visible_ticks = 1000;
    st.gs = omf_calloc(1, sizeof(game_state));
    if(game_state_create(st.gs, init_flags)) {
        game_state_free(&st.gs);
        return;
    }

    joystick_init();

    st.frame_start = SDL_GetTicks64(); // Set game tick timer

#ifdef __EMSCRIPTEN__
    // Drive the game loop from the browser's requestAnimationFrame so that the
    // page stays responsive. main() returns immediately; the game continues to
    // run inside the emscripten_tick() callback until it tears itself down.
    web_loop_state = st;
    emscripten_set_main_loop(emscripten_tick, 0, 1);
#else
    // Game loop
    while(run && game_state_is_running(st.gs)) {
        engine_loop_iteration(&st);
    }
    engine_loop_finish(&st);
#endif
}

void engine_close(void) {
    script_cache_close();
    osd_close();
    console_close();
    altpals_close();
    fonts_close();
    lang_close();
    sounds_loader_close();
    audio_close();
    video_close();
    vga_state_close();
    modmanager_shutdown();
    log_info("Engine deinit successful.");
}
