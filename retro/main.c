#include <stdint.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include "../common/board.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/display.h"
#include "../common/fileio.h"
#include "../common/init.h"
#include "../common/input.h"
#include "../common/inotify.h"
#include "../common/language.h"
#include "../common/log.h"
#include "../common/ui/common.h"
#include "gamestate.h"
#include "hotkeys.h"
#include "muxretro.h"
#include "options.h"
#include "core.h"
#include "paths.h"
#include "rumble.h"
#include "settings.h"

#define RESUME_COOLDOWN_MS     1500
#define AUTOLOAD_WARMUP_FRAMES 5
#define AUDIO_LOW_WATER_MS     64
#define AUDIO_HIGH_WATER_MS    80
#define AUDIO_MAX_CATCHUP      3

static inotify_status *idle_ino = NULL;
static int mux_idle_state_exists = 0;
static unsigned mux_idle_state_changes = 0;
static unsigned last_seen_changes = 0;
static char state_dir[MAX_BUFFER_SIZE];

static void build_state_dir(const char *content_path) {
    const char *base = strrchr(content_path, '/');
    base = base ? base + 1 : content_path;
    snprintf(state_dir, sizeof(state_dir), "%s/%s", RETRO_STA_PATH, base);
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
        LOG_DEBUG(mux_module, "idle_poll: triggering pause_menu_toggle + gamestate_autosave_save");
        pause_menu_toggle();
        gamestate_autosave_save();
    }
    last_seen_changes = mux_idle_state_changes;
}

int main(const int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <core.so> <content> [--fresh]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *core_path_arg = argv[1];
    const char *content_path = argv[2];

    const int start_fresh = argc >= 4 && strcmp(argv[3], "--fresh") == 0;

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

    build_state_dir(content_path);
    gamestate_init(state_dir);
    options_capture_baseline();
    LOG_DEBUG(mux_module, "options_capture_baseline done, options_count=%d", options_count);

    session_settings_init(core_path_arg, content_path);
    video_bridge_apply_fps_limit();

    struct retro_system_av_info av_info = {0};
    current_core.retro_get_system_av_info(&av_info);
    video_bridge_set_core_aspect(av_info.geometry.aspect_ratio);

    const double fps = av_info.timing.fps > 0 ? av_info.timing.fps : 60.0;

    audio_bridge_open(av_info.timing.sample_rate > 0 ? av_info.timing.sample_rate : 48000.0);
    LOG_DEBUG(mux_module, "audio_bridge_open done");

    video_bridge_init();
    LOG_DEBUG(mux_module, "video_bridge_init done");

    if (!start_fresh) {
        for (int i = 0; i < AUTOLOAD_WARMUP_FRAMES; i++)
            current_core.retro_run();
        audio_bridge_clear_queued();

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

    LOG_SUCCESS(mux_module, "Running content at %.2f fps / %.0f Hz audio", fps, av_info.timing.sample_rate);

    int quit = 0;

    uint32_t fps_frame_count = 0;
    uint32_t fps_last_update = SDL_GetTicks();

    double fps_limit_deadline = 0.0;

    while (!quit) {
        mux_input_poll();
        idle_poll();

        const int paused = pause_menu_is_active();
        audio_bridge_set_paused(paused);

        if (paused) {
            if (pause_menu_tick()) quit = 1;
            lv_obj_invalidate(ui_screen);

            SDL_Delay(10);
        } else if (hotkeys_task()) {
            LOG_DEBUG(mux_module, "main: menu released without a hotkey chord, toggling pause");
            pause_menu_toggle();
        } else {
            current_core.retro_run();
            int frames_run = 1;

            const int ff_active = hotkeys_is_fast_forward_active();
            const int slowmo_active = hotkeys_is_slow_motion_active();

            if (ff_active) {
                const int batch = (int) session_settings_ff_speed_value(session_settings.ff_speed);
                while (frames_run < batch) {
                    current_core.retro_run();
                    frames_run++;
                }
                fps_limit_deadline = 0.0;
            } else if (session_settings.fps_limit == fps_limit_50 || slowmo_active) {
                while (audio_bridge_queued_ms() > AUDIO_HIGH_WATER_MS)
                    SDL_Delay(2);

                double target_ms = session_settings.fps_limit == fps_limit_50 ? 20.0 : 1000.0 / fps;
                if (slowmo_active) target_ms /= session_settings_slowmo_speed_value(session_settings.slowmo_speed);

                const double now = SDL_GetTicks();

                if (fps_limit_deadline < now - target_ms) fps_limit_deadline = now;

                fps_limit_deadline += target_ms;
                if (fps_limit_deadline > now) SDL_Delay((uint32_t) (fps_limit_deadline - now));
            } else {
                fps_limit_deadline = 0.0;

                if (audio_bridge_is_active()) {
                    while (frames_run <= AUDIO_MAX_CATCHUP && audio_bridge_queued_ms() < AUDIO_LOW_WATER_MS) {
                        current_core.retro_run();
                        frames_run++;
                    }
                }

                while (audio_bridge_queued_ms() > AUDIO_HIGH_WATER_MS)
                    SDL_Delay(2);
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

        video_bridge_flush_frame();
        lv_refr_now(NULL);

        if (paused) lv_task_handler();
        display_composite_frame();
    }

    pause_menu_shutdown();
    video_bridge_shutdown();
    audio_bridge_close();
    rumble_bridge_shutdown();

    core_unload_content();
    core_unload();

    mux_input_close();
    sdl_cleanup();

    return EXIT_SUCCESS;
}
