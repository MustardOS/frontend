#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include "../../common/board.h"
#include "../../common/config.h"
#include "../../common/device.h"
#include "../../common/display.h"
#include "../../common/fileio.h"
#include "../../common/init.h"
#include "../../common/input.h"
#include "../../common/inotify.h"
#include "../../common/language.h"
#include "../../common/log.h"
#include "../../common/ui/common.h"
#include "../ui/cheats.h"
#include "../state/gamestate.h"
#include "../input/hotkeys.h"
#include "muxretro.h"
#include "../ui/options.h"
#include "core.h"
#include "../video/overlay_bridge.h"
#include "paths.h"
#include "../input/rumble.h"
#include "../settings/settings.h"
#include "../state/sram.h"

#define RESUME_COOLDOWN_MS     1500
#define AUTOLOAD_WARMUP_FRAMES 60
#define AUDIO_MAX_CATCHUP      3

static inotify_status *idle_ino = NULL;
static int mux_idle_state_exists = 0;
static unsigned mux_idle_state_changes = 0;
static unsigned last_seen_changes = 0;
static char state_dir[MAX_BUFFER_SIZE];

static volatile sig_atomic_t pending_sleep_signal = 0;
static volatile sig_atomic_t pending_wake_signal = 0;
static double target_fps = 60.0;

void core_set_target_fps(const double new_fps) {
    if (new_fps > 0.0) target_fps = new_fps;
}

double core_get_target_fps(void) {
    return target_fps;
}

static void handle_sleep_signal(const int sig) {
    (void) sig;
    pending_sleep_signal = 1;
}

static void handle_wake_signal(const int sig) {
    (void) sig;
    pending_wake_signal = 1;
}

static void install_suspend_signal_handlers(void) {
    struct sigaction sa = {0};
    sa.sa_flags = SA_RESTART;

    sa.sa_handler = handle_sleep_signal;
    sigaction(SIGUSR1, &sa, NULL);

    sa.sa_handler = handle_wake_signal;
    sigaction(SIGUSR2, &sa, NULL);
}

static void handle_pending_suspend_signals(void) {
    if (pending_sleep_signal) {
        pending_sleep_signal = 0;

        LOG_INFO(mux_module, "Received sleep-prepare signal (SIGUSR1): saving SRAM");
        sram_bridge_save();
        if (session_settings_auto_save_on_idle()) gamestate_autosave_save();
        if (!pause_menu_is_active()) pause_menu_toggle();
    }

    if (pending_wake_signal) {
        pending_wake_signal = 0;
        LOG_INFO(mux_module, "Received resume signal (SIGUSR2)");

        if (pause_menu_is_active()) pause_menu_toggle();
    }
}

static void build_state_dir(const char *core_path_arg, const char *content_path) {
    char core_name[MAX_BUFFER_SIZE];
    core_get_name(core_path_arg, core_name, sizeof(core_name));

    const char *base = strrchr(content_path, '/');
    base = base ? base + 1 : content_path;

    snprintf(state_dir, sizeof(state_dir), "%s/%s/%s", RETRO_STA_PATH, core_name, base);
    create_directories(state_dir, 0);
}

static void idle_poll(void) {
    if (!config.settings.power.idle.display) return;

    if (!idle_ino) {
        idle_ino = inotify_create();
        if (idle_ino) {
            inotify_track(idle_ino, "/run/muos", "idle_state", &mux_idle_state_exists, &mux_idle_state_changes);
            last_seen_changes = mux_idle_state_changes;
        }
    }
    if (!idle_ino) return;

    inotify_check(idle_ino);

    static int was_paused = 0;
    static uint32_t resume_cooldown_until = 0;
    const int is_paused = pause_menu_is_active();
    if (was_paused && !is_paused) resume_cooldown_until = SDL_GetTicks() + RESUME_COOLDOWN_MS;
    was_paused = is_paused;

    static unsigned last_logged_changes = (unsigned) -1;
    if (mux_idle_state_changes != last_logged_changes) {
        last_logged_changes = mux_idle_state_changes;
        LOG_DEBUG(
            mux_module, "idle_poll: changes=%u exists=%d last_seen=%u paused=%d cooldown_remaining=%d",
            mux_idle_state_changes, mux_idle_state_exists, last_seen_changes, is_paused,
            (int) (resume_cooldown_until - SDL_GetTicks())
        );
    }

    if (mux_idle_state_exists && mux_idle_state_changes != last_seen_changes && !is_paused
        && SDL_GetTicks() >= resume_cooldown_until) {
        LOG_DEBUG(mux_module, "idle_poll: triggering pause_menu_toggle + SRAM save");
        pause_menu_toggle();

        sram_bridge_save();
        if (session_settings_auto_save_on_idle()) gamestate_autosave_save();
    }
    last_seen_changes = mux_idle_state_changes;
}

static void run_core_batch(const unsigned frames) {
    for (unsigned i = 0; i < frames; i++) {
        const int visible = i + 1 == frames;
        video_bridge_set_frame_skip(!visible);
        if (visible) frame_pacer_maybe_wait();
        input_bridge_begin_run();
        audio_bridge_notify_buffer_status();
        environment_notify_frame_time();
        current_core.retro_run();
        audio_bridge_flush_sample_fifo();
    }
    video_bridge_set_frame_skip(0);
}

void core_prime_audio(void) {
    if (!audio_bridge_is_active()) return;

    const uint32_t target = audio_bridge_low_water_ms();
    const unsigned max_frames = AUDIO_MAX_CATCHUP * 8;

    video_bridge_set_frame_skip(1);

    unsigned primed = 0;
    while (primed < max_frames && audio_bridge_queued_ms() < target) {
        input_bridge_begin_run();
        audio_bridge_notify_buffer_status();
        current_core.retro_run();
        audio_bridge_flush_sample_fifo();
        primed++;
    }

    video_bridge_set_frame_skip(0);
}

int main(const int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <core.so> <content> [--fresh]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *core_path_arg = argv[1];
    const char *content_path = argv[2];

    const int start_fresh = argc >= 4 && strcmp(argv[3], "--fresh") == 0;

    install_suspend_signal_handlers();

    load_device(&device);
    load_config(&config);

    init_module("muxretro");
    LOG_DEBUG(mux_module, "init_module done");

    init_theme(1, 0);
    LOG_DEBUG(mux_module, "init_theme done");

    init_display();
    LOG_DEBUG(mux_module, "init_display done, renderer=%p", (void *) display_get_renderer());

    board_init(device.board.name);
    mux_input_open();
    LOG_DEBUG(mux_module, "board_init/mux_input_open done");

    create_directories(RETRO_SRM_PATH "/", 1);
    options_init_paths(core_path_arg, content_path);

    if (core_open(core_path_arg) != 0) {
        LOG_ERROR(mux_module, "Failed to open core: %s", core_path_arg);
        mux_input_close();
        sdl_cleanup();
        return EXIT_FAILURE;
    }
    LOG_DEBUG(mux_module, "core_open done");

    if (core_load_content(content_path) != 0) {
        LOG_ERROR(mux_module, "Failed to load content: %s", content_path);
        core_unload();
        mux_input_close();
        sdl_cleanup();
        return EXIT_FAILURE;
    }
    LOG_DEBUG(mux_module, "core_load_content done");

    sram_bridge_init(core_path_arg, content_path);
    cheats_init(core_path_arg, content_path);
    overlay_bridge_init(core_path_arg, content_path);

    build_state_dir(core_path_arg, content_path);
    gamestate_init(state_dir);
    options_capture_baseline();
    LOG_DEBUG(mux_module, "options_capture_baseline done, options_count=%d", options_count);

    session_settings_init(core_path_arg, content_path);
    video_bridge_apply_fps_limit();

    struct retro_system_av_info av_info = {0};
    current_core.retro_get_system_av_info(&av_info);
    video_bridge_set_core_aspect(av_info.geometry.aspect_ratio);

    target_fps = av_info.timing.fps > 0 ? av_info.timing.fps : 60.0;

    audio_bridge_open(av_info.timing.sample_rate > 0 ? av_info.timing.sample_rate : 48000.0);
    LOG_DEBUG(mux_module, "audio_bridge_open done");

    video_bridge_init();
    LOG_DEBUG(mux_module, "video_bridge_init done");

    overlay_bridge_apply();

    if (!start_fresh) {
        video_bridge_set_frame_skip(1);
        audio_bridge_set_muted(1);
        rumble_bridge_set_suppressed(1);

        for (int i = 0; i < AUTOLOAD_WARMUP_FRAMES; i++) {
            input_bridge_begin_run();
            current_core.retro_run();
        }
        audio_bridge_clear_queued();

        video_bridge_set_frame_skip(0);
        audio_bridge_set_muted(0);
        rumble_bridge_set_suppressed(0);

        if (gamestate_load_most_recent() == 0) {
            LOG_INFO(
                mux_module, "Auto-loaded most recent save state (after %d warm-up frames)", AUTOLOAD_WARMUP_FRAMES
            );
        }
    }

    pause_menu_init();
    LOG_DEBUG(
        mux_module, "pause_menu_init done: ui_screen=%p ui_pnl_header=%p ui_pnl_content=%p ui_pnl_footer=%p",
        (void *) ui_screen, (void *) ui_pnl_header, (void *) ui_pnl_content, (void *) ui_pnl_footer
    );

    if (core_active_patch_count > 0) {
        char patch_toast[64];
        snprintf(
            patch_toast, sizeof(patch_toast), lang.muxretro.information_screen.loaded_patches_toast,
            core_active_patch_count
        );
        pause_menu_show_toast(patch_toast);
    }

    LOG_SUCCESS(mux_module, "Running content at %.2f fps / %.0f Hz audio", target_fps, av_info.timing.sample_rate);

    int quit = 0;
    int peeking = 0;
    int prev_paused = 0;

    uint32_t fps_frame_count = 0;
    uint32_t fps_last_update = SDL_GetTicks();

    double fps_limit_deadline = 0.0;
    uint32_t sram_flush_deadline = SDL_GetTicks() + (uint32_t) session_settings.sram_flush_seconds * 1000;
    uint32_t status_deadline = SDL_GetTicks() + TIMER_STATUS;

    while (!quit) {
        mux_input_poll();
        idle_poll();
        handle_pending_suspend_signals();
        display_check_idle_saver();
        hotkeys_volume_bright_task();

        if (SDL_GetTicks() >= status_deadline) {
            status_task(NULL);
            status_deadline = SDL_GetTicks() + TIMER_STATUS;
        }

        if (SDL_GetTicks() >= sram_flush_deadline) {
            sram_bridge_save();
            sram_flush_deadline = SDL_GetTicks() + (uint32_t) session_settings.sram_flush_seconds * 1000;
        }

        const int paused = pause_menu_is_active();
        if (prev_paused && !paused) core_prime_audio();
        prev_paused = paused;
        audio_bridge_set_paused(paused);

        if (paused) {
            const int peek = pause_menu_peek_allowed() && mux_input_pressed(mux_input_menu);
            if (peek != peeking) {
                peeking = peek;
                display_set_ui_hidden(peek);
                if (!peek) pause_menu_sync_input_mask();
            }

            if (!peek) {
                if (pause_menu_tick()) quit = 1;
                lv_obj_invalidate(ui_screen);
            }

            SDL_Delay(10);
        } else if (peeking) {
            peeking = 0;
            display_set_ui_hidden(0);
        } else if (hotkeys_task()) {
            LOG_DEBUG(mux_module, "main: menu released without a hotkey combo, toggling pause");
            pause_menu_toggle();
        } else if (hotkeys_is_quit_requested()) {
            quit = 1;
        } else {
            audio_bridge_apply_pending_min_latency();
            environment_apply_pending_av_info();

            const int ff_active = hotkeys_is_fast_forward_active();
            const int slowmo_active = hotkeys_is_slow_motion_active();

            unsigned frames = 1;
            if (ff_active) {
                const unsigned ff_batch = (unsigned) session_settings_ff_speed_value(session_settings.ff_speed);
                frames = ff_batch > 0 ? ff_batch : 1;
            } else if (session_settings.fps_limit != fps_limit_50 && !slowmo_active && audio_bridge_is_active()
                       && audio_bridge_queued_ms() < audio_bridge_low_water_ms()) {
                frames = 1 + AUDIO_MAX_CATCHUP;
            }

            run_core_batch(frames);
            int frames_run = (int) frames;

            if (ff_active) {
                fps_limit_deadline = 0.0;
            } else if (session_settings.fps_limit == fps_limit_50 || slowmo_active) {
                audio_bridge_wait_for_headroom();

                double target_ms = session_settings.fps_limit == fps_limit_50 ? 20.0 : 1000.0 / target_fps;
                if (slowmo_active) target_ms /= session_settings_slowmo_speed_value(session_settings.slowmo_speed);

                const double now = SDL_GetTicks();

                if (fps_limit_deadline < now - target_ms) fps_limit_deadline = now;

                fps_limit_deadline += target_ms;
                if (fps_limit_deadline > now) SDL_Delay((uint32_t) (fps_limit_deadline - now));
            } else {
                fps_limit_deadline = 0.0;
                audio_bridge_wait_for_headroom();
            }

            fps_frame_count += (uint32_t) frames_run;
            const uint32_t now_ticks = SDL_GetTicks();
            const uint32_t fps_elapsed = now_ticks - fps_last_update;
            if (fps_elapsed >= 1000) {
                if (session_settings.show_fps) {
                    char fps_text[16];
                    const double vfps = (double) fps_frame_count * 1000.0 / (double) fps_elapsed;
                    snprintf(fps_text, sizeof(fps_text), "%.2f", vfps);
                    pause_menu_set_fps_text(fps_text);
                }
                pause_menu_update_header();
                fps_frame_count = 0;
                fps_last_update = now_ticks;
            }
        }

        pause_menu_toast_tick();
        pause_menu_header_fade_tick();

        const int paused_now = pause_menu_is_active();

        int hud_dirty = paused_now;
        if (paused_now) {
            if (!peeking) display_set_ui_hidden(0);
        } else {
            const int hud_active = pause_menu_gameplay_hud_active();
            hud_dirty = pause_menu_gameplay_hud_dirty();
            display_set_ui_hidden(!hud_active);
        }

        video_bridge_flush_frame();
        if (hud_dirty) lv_refr_now(NULL);

        if (paused_now) lv_task_handler();
        display_composite_frame();
        frame_pacer_after_present();
    }

    pause_menu_shutdown();
    video_bridge_shutdown();
    overlay_bridge_shutdown();
    audio_bridge_close();
    rumble_bridge_shutdown();

    sram_bridge_save();
    sram_bridge_shutdown();

    core_unload_content();
    core_unload();

    mux_input_close();
    sdl_cleanup();

    return EXIT_SUCCESS;
}
