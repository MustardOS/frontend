#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
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
#include "../state/content_hash.h"
#include "../state/gamestate.h"
#include "../state/macro.h"
#include "../state/manual.h"
#include "../state/patch.h"
#include "../input/hotkeys.h"
#include "muxretro.h"
#include "../ui/options.h"
#include "../ui/ui_loading.h"
#include "core.h"
#include "governor_boost.h"
#include "runahead.h"
#include "../video/hw_render.h"
#include "../video/overlay_bridge.h"
#include "paths.h"
#include "perf.h"
#include "../input/rumble.h"
#include "../settings/settings.h"
#include "../state/sram.h"

#define RESUME_COOLDOWN_MS 1500
#define AUDIO_MAX_CATCHUP  3

static inotify_status *idle_ino = NULL;
static int mux_idle_state_exists = 0;
static unsigned mux_idle_state_changes = 0;
static unsigned last_seen_changes = 0;
static char state_dir[MAX_BUFFER_SIZE];
static char macro_dir[MAX_BUFFER_SIZE];

static volatile sig_atomic_t pending_sleep_signal = 0;
static volatile sig_atomic_t pending_wake_signal = 0;
static double target_fps = 60.0;

static double startup_elapsed_ms(const uint64_t start) {
    return (double) (SDL_GetPerformanceCounter() - start) * 1000.0 / (double) SDL_GetPerformanceFrequency();
}

static void startup_log_stage(const char *stage, uint64_t *stage_start) {
    LOG_INFO(mux_module, "Startup: %s took %.2f ms", stage, startup_elapsed_ms(*stage_start));
    *stage_start = SDL_GetPerformanceCounter();
}

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

static void precache_content(const char *content_path) {
    const int budget_mb = session_settings_content_precache_mb(session_settings.content_precache);
    if (budget_mb <= 0) return;

    const int fd = open(content_path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return;

    struct stat st;
    off_t len = fstat(fd, &st) == 0 ? st.st_size : 0;

    const off_t budget = (off_t) budget_mb * 1024 * 1024;
    if (len > budget) len = budget;

    if (len > 0) {
        posix_fadvise(fd, 0, len, POSIX_FADV_WILLNEED);
        LOG_INFO(mux_module, "Content precache: hinted %lld MiB of '%s'", (long long) (len >> 20), content_path);
    }

    close(fd);
}

static int hard_sync_enabled(void) {
    return session_settings.gpu_hard_sync;
}

static void build_state_dir(const char *core_path_arg, const char *content_path) {
    const char *base = strrchr(content_path, '/');
    base = base ? base + 1 : content_path;

    char content_stem[MAX_BUFFER_SIZE];
    snprintf(content_stem, sizeof(content_stem), "%s", base);
    char *dot = strrchr(content_stem, '.');
    if (dot) *dot = '\0';

    char save_prefix[MAX_BUFFER_SIZE];
    core_content_save_prefix(core_path_arg, content_path, save_prefix, sizeof(save_prefix));

    snprintf(state_dir, sizeof(state_dir), "%s/%s/%s", RETRO_STA_PATH, save_prefix, content_stem);
    create_directories(state_dir, 0);
}

static void build_macro_dir(const char *core_path_arg, const char *content_path) {
    const char *base = strrchr(content_path, '/');
    base = base ? base + 1 : content_path;

    char content_stem[MAX_BUFFER_SIZE];
    snprintf(content_stem, sizeof(content_stem), "%s", base);
    char *dot = strrchr(content_stem, '.');
    if (dot) *dot = '\0';

    char save_prefix[MAX_BUFFER_SIZE];
    core_content_save_prefix(core_path_arg, content_path, save_prefix, sizeof(save_prefix));

    snprintf(macro_dir, sizeof(macro_dir), "%s/%s/%s", RETRO_MAC_PATH, save_prefix, content_stem);
    create_directories(macro_dir, 0);
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

    static unsigned check_countdown = 0;
    if (check_countdown > 0) {
        check_countdown--;
        return;
    }
    check_countdown = 15;

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

static double core_run_ema_ms = 0.0;

static void run_core_batch(const unsigned frames) {
    hw_render_bridge_context_save();

    if (frames == 1) frame_pacer_maybe_wait();

    for (unsigned i = 0; i < frames; i++) {
        const int is_last = i + 1 == frames || environment_av_info_pending();

        input_bridge_begin_run();
        audio_bridge_notify_buffer_status();
        environment_notify_frame_time();

        const uint64_t run_start = SDL_GetPerformanceCounter();
        if (is_last) runahead_before_frame(frames == 1);
        current_core.retro_run();
        const double run_ms =
            (double) (SDL_GetPerformanceCounter() - run_start) * 1000.0 / (double) SDL_GetPerformanceFrequency();
        core_run_ema_ms = core_run_ema_ms <= 0.0 ? run_ms : core_run_ema_ms * 0.9 + run_ms * 0.1;
        perf_record(perf_stage_core, run_ms);

        audio_bridge_flush_sample_fifo();

        if (is_last) break;
    }

    hw_render_bridge_flush_core_commands();
    hw_render_bridge_context_restore();
    video_bridge_set_frame_skip(0);
}

static void pace_core_output(void) {
    static double fps_limit_deadline = 0.0;

    if (hotkeys_is_fast_forward_active()) {
        fps_limit_deadline = 0.0;
        return;
    }

    const uint64_t audio_wait_start = perf_begin();
    audio_bridge_drc_tick();
    audio_bridge_wait_for_headroom();
    perf_end(perf_stage_audio_wait, audio_wait_start);

    const int slowmo_active = hotkeys_is_slow_motion_active();
    if (session_settings.fps_limit != fps_limit_50 && !slowmo_active) {
        fps_limit_deadline = 0.0;
        return;
    }

    double target_ms = session_settings.fps_limit == fps_limit_50 ? 20.0 : 1000.0 / target_fps;
    if (slowmo_active) target_ms /= session_settings_slowmo_speed_value(session_settings.slowmo_speed);

    const double now = SDL_GetTicks();
    if (fps_limit_deadline < now - target_ms) fps_limit_deadline = now;

    fps_limit_deadline += target_ms;
    if (fps_limit_deadline > now) SDL_Delay((uint32_t) (fps_limit_deadline - now));
}

void core_prime_audio(void) {
    if (!audio_bridge_is_active()) return;
    runahead_invalidate();

    const uint32_t target = audio_bridge_low_water_ms();
    const unsigned max_frames = AUDIO_MAX_CATCHUP * 8;

    video_bridge_set_frame_skip(1);

    unsigned primed = 0;
    hw_render_bridge_context_save();
    while (primed < max_frames && audio_bridge_queued_ms() < target) {
        input_bridge_begin_run();
        audio_bridge_notify_buffer_status();
        current_core.retro_run();
        audio_bridge_flush_sample_fifo();
        primed++;
    }
    hw_render_bridge_context_restore();

    video_bridge_set_frame_skip(0);
}

int main(const int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <core.so> <content> [--fresh]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *core_path_arg = argv[1];
    const char *content_path = argv[2];

    int start_fresh = 0;
    int start_restarting = 0;
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--fresh") == 0) start_fresh = 1;
        if (strcmp(argv[i], "--restart") == 0) {
            start_fresh = 1;
            start_restarting = 1;
        }
    }

    const char *startup_message = start_restarting ? lang.muxretro.content_restarting : lang.muxretro.content_loading;
    const uint64_t startup_start = SDL_GetPerformanceCounter();
    uint64_t startup_stage = startup_start;

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

    governor_boost_begin("content startup");
    loading_message_show(startup_message);

    if (core_open(core_path_arg) != 0) {
        LOG_ERROR(mux_module, "Failed to open core: %s", core_path_arg);
        governor_boost_end();
        mux_input_close();
        sdl_cleanup();
        return EXIT_FAILURE;
    }
    LOG_DEBUG(mux_module, "core_open done");
    startup_log_stage("frontend and core initialisation", &startup_stage);

    state_saves_init(core_path_arg);
    patch_manual_init(core_path_arg, content_path);
    perf_init();
    session_settings_init(core_path_arg, content_path);

    build_state_dir(core_path_arg, content_path);
    gamestate_init(state_dir);

    char resume_path[512] = "";
    int load_blocked = 0;
    int has_resume = !start_fresh && state_saves_supported()
                     && gamestate_find_most_recent(resume_path, sizeof(resume_path), &load_blocked) == 0;
    loading_message_show(has_resume ? lang.muxretro.content_resuming : startup_message);

    precache_content(content_path);

    if (core_load_content(content_path) != 0) {
        LOG_ERROR(mux_module, "Failed to load content: %s", content_path);
        governor_boost_end();
        core_unload();
        mux_input_close();
        sdl_cleanup();
        return EXIT_FAILURE;
    }
    LOG_DEBUG(mux_module, "core_load_content done");
    startup_log_stage("content load", &startup_stage);

    sram_bridge_init(core_path_arg, content_path);
    cheats_init(core_path_arg, content_path);
    manual_init(core_path_arg, content_path);
    overlay_bridge_init(core_path_arg, content_path);

    build_macro_dir(core_path_arg, content_path);
    macros_init(macro_dir);

    content_hash_request(content_path);

    options_capture_baseline();
    LOG_DEBUG(mux_module, "options_capture_baseline done, options_count=%d", options_count);

    video_bridge_apply_fps_limit();
    display_set_hard_sync_query(hard_sync_enabled);

    struct retro_system_av_info av_info = {0};
    current_core.retro_get_system_av_info(&av_info);
    video_bridge_set_core_aspect(av_info.geometry.aspect_ratio);

    if (hw_render_bridge_active()) {
        hw_render_bridge_configure(av_info.geometry.max_width, av_info.geometry.max_height);
        LOG_DEBUG(mux_module, "hw_render_bridge_configure done");
    }

    target_fps = av_info.timing.fps > 0 ? av_info.timing.fps : 60.0;

    audio_bridge_open(av_info.timing.sample_rate > 0 ? av_info.timing.sample_rate : 48000.0);
    LOG_DEBUG(mux_module, "audio_bridge_open done");

    video_bridge_init();
    LOG_DEBUG(mux_module, "video_bridge_init done");
    startup_log_stage("bridges and renderer setup", &startup_stage);

    overlay_bridge_apply();
    mux_input_poll();
    input_bridge_suppress_held();

    int state_preserved = 0;
    if (state_saves_supported()) state_preserved = gamestate_protect_mismatched_autosave();

    resume_path[0] = '\0';
    load_blocked = 0;
    has_resume = !start_fresh && state_saves_supported()
                 && gamestate_find_most_recent(resume_path, sizeof(resume_path), &load_blocked) == 0;

    if (has_resume) {
        loading_message_show(lang.muxretro.content_resuming);
        const int warmup_frames = state_saves_warmup_frames();
        video_bridge_set_frame_skip(1);
        audio_bridge_set_muted(1);
        rumble_bridge_set_suppressed(1);

        if (warmup_frames > 0) {
            hw_render_bridge_context_save();
            for (int i = 0; i < warmup_frames; i++) {
                input_bridge_begin_run();
                current_core.retro_run();
            }
            hw_render_bridge_flush_core_commands();
            hw_render_bridge_context_restore();
        }
        audio_bridge_clear_queued();

        video_bridge_set_frame_skip(0);
        audio_bridge_set_muted(0);
        rumble_bridge_set_suppressed(0);

        startup_log_stage("save-state warm-up", &startup_stage);

        if (state_load(resume_path) == 0) {
            LOG_INFO(mux_module, "Auto-loaded most recent save state (after %d warm-up frames)", warmup_frames);
        }
        startup_log_stage("save-state restore", &startup_stage);
    } else {
        startup_log_stage("resume selection (no compatible state)", &startup_stage);
    }

    governor_boost_end();

    pause_menu_init();
    loading_message_hide();
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

    if (state_preserved || load_blocked) {
        pause_menu_toggle();
        gamestate_notice_open();
    }

    LOG_SUCCESS(mux_module, "Running content at %.2f fps / %.0f Hz audio", target_fps, av_info.timing.sample_rate);
    LOG_INFO(mux_module, "Startup: ready in %.2f ms total", startup_elapsed_ms(startup_start));

    int quit = 0;
    int peeking = 0;
    int prev_paused = 0;

    uint32_t fps_frame_count = 0;
    uint32_t fps_last_update = SDL_GetTicks();

    uint32_t sram_flush_deadline = SDL_GetTicks() + (uint32_t) session_settings.sram_flush_seconds * 1000;
    uint32_t status_deadline = SDL_GetTicks() + TIMER_STATUS;

    uint32_t timeline_deadline = 0;
    int timeline_armed_ms = 0;

    while (!quit) {
        int core_ran = 0;

        const uint32_t loop_now = SDL_GetTicks();

        mux_input_poll();
        idle_poll();
        handle_pending_suspend_signals();
        display_check_idle_saver();
        hotkeys_volume_bright_task();

        if (loop_now >= status_deadline) {
            status_task(NULL);
            status_deadline = loop_now + TIMER_STATUS;
        }

        if (loop_now >= sram_flush_deadline) {
            sram_bridge_save();
            sram_flush_deadline = loop_now + (uint32_t) session_settings.sram_flush_seconds * 1000;
        }

        const int paused = pause_menu_is_active();
        if (paused || hotkeys_is_content_paused()) governor_boost_gameplay_idle();
        if (prev_paused && !paused) core_prime_audio();
        prev_paused = paused;
        audio_bridge_set_paused(paused);

        const int timeline_ms = session_settings_timeline_interval_ms();
        if (timeline_ms != timeline_armed_ms) {
            timeline_armed_ms = timeline_ms;
            timeline_deadline = timeline_ms > 0 ? loop_now + (uint32_t) timeline_ms : 0;
        }

        if (timeline_ms > 0 && !paused && !hotkeys_is_content_paused() && state_saves_supported()
            && loop_now >= timeline_deadline) {
            gamestate_timeline_save();
            timeline_deadline = loop_now + (uint32_t) timeline_ms;
        }

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
        } else if (hotkeys_is_manual_requested()) {
            pause_menu_toggle();
            manual_menu_open();
        } else if (hotkeys_is_quit_requested()) {
            quit = 1;
        } else if (hotkeys_is_content_paused()) {
            SDL_Delay(10);
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
                unsigned extra = AUDIO_MAX_CATCHUP;

                if (hw_render_bridge_active()) {
                    extra = 0;
                } else if (core_run_ema_ms > 0.0) {
                    const double headroom = 1000.0 / target_fps / core_run_ema_ms - 1.0;
                    if (headroom <= 0.0) {
                        extra = 0;
                    } else if (headroom < (double) AUDIO_MAX_CATCHUP) {
                        extra = (unsigned) headroom;
                    }
                }

                frames = 1 + extra;
            }

            perf_note_batch(frames);
            run_core_batch(frames);
            core_ran = 1;

            perf_record(perf_stage_frame_delay, frame_pacer_get_delay_ms());

            fps_frame_count += frames;
            const uint32_t now_ticks = SDL_GetTicks();
            const uint32_t fps_elapsed = now_ticks - fps_last_update;
            if (fps_elapsed >= 1000) {
                const double vfps = (double) fps_frame_count * 1000.0 / (double) fps_elapsed;
                if (session_settings.show_fps) {
                    char fps_text[256];
                    if (session_settings.show_fps == show_fps_detailed) {
                        perf_format_hud(fps_text, sizeof(fps_text), vfps);
                    } else {
                        snprintf(fps_text, sizeof(fps_text), "%.2f", vfps);
                    }
                    pause_menu_set_fps_text(fps_text);
                }

                if (slowmo_active) {
                    governor_boost_gameplay_idle();
                } else {
                    const double governor_target = session_settings.fps_limit == fps_limit_50 ? 50.0 : target_fps;
                    governor_boost_gameplay_update(vfps, governor_target, ff_active);
                }

                pause_menu_update_header();
                fps_frame_count = 0;
                fps_last_update = now_ticks;
            }
        }

        pause_menu_toast_tick();
        pause_menu_header_fade_tick();

        const int paused_now = pause_menu_is_active();

        if (paused_now) {
            if (!peeking) display_set_ui_hidden(0);
        } else {
            const int hud_active = pause_menu_gameplay_hud_active();
            display_set_ui_hidden(!hud_active);
        }

        const uint64_t video_start = core_ran ? perf_begin() : 0;
        video_bridge_flush_frame();
        perf_end(perf_stage_video, video_start);
        if (paused_now) lv_task_handler();

        const uint64_t present_start = core_ran ? perf_begin() : 0;
        if (display_ui_is_hidden()) {
            display_composite_frame();
        } else {
            const uint64_t present_before_refresh = display_present_serial();
            if (paused_now) lv_obj_invalidate(ui_screen);
            lv_refr_now(NULL);
            if (display_present_serial() == present_before_refresh) display_composite_frame();
        }
        perf_end(perf_stage_present, present_start);
        if (core_ran) perf_note_present();

        frame_pacer_after_present();

        if (core_ran) pace_core_output();
        perf_frame_complete(core_ran);
    }

    pause_menu_shutdown();
    governor_boost_shutdown();
    runahead_shutdown();
    video_bridge_shutdown();
    overlay_bridge_shutdown();
    audio_bridge_close();
    rumble_bridge_shutdown();

    sram_bridge_save();
    sram_bridge_shutdown();

    core_unload_content();
    core_unload();

    if (dir_exist(RETRO_EXT_PATH)) remove_directory_recursive(RETRO_EXT_PATH);

    mux_input_close();
    sdl_cleanup();

    if (core_restart_requested) {
        // Force a (re)start of pickles with '--fresh'.
        // A restart must NEVER auto load anything otherwise it just reloads the exact frame we quit on
        // and it'll essentially undo the reset and cause all sorts of funky stuff to happen...
        char *restart_argv[] = {argv[0], core_file_path, core_content_path, "--fresh", "--restart", NULL};
        execvp(restart_argv[0], restart_argv);
        LOG_ERROR(mux_module, "Failed to re-exec for restart (%s), exiting instead", strerror(errno));
    }

    return EXIT_SUCCESS;
}
