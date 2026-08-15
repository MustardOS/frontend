#include <errno.h>
#include <fcntl.h>
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
#include "../../common/strutil.h"
#include "../../common/ui/common.h"
#include "../../common/ui/nav.h"
#include "../ui/cheats.h"
#include "../cheevo/cheevo.h"
#include "../coreinfo/coreinfo.h"
#include "../state/content_hash.h"
#include "../state/gamestate.h"
#include "../macro/macro.h"
#include "../netplay/netplay.h"
#include "../link/link.h"
#include "subsystem.h"
#include "../state/manual.h"
#include "../state/patch.h"
#include "../input/hotkeys.h"
#include "muxretro.h"
#include "../ui/options.h"
#include "../ui/ui_loading.h"
#include "core.h"
#include "governor_boost.h"
#include "runahead.h"
#include "startup.h"
#include "../video/hw_render.h"
#include "../video/image_writer.h"
#include "../video/overlay_bridge.h"
#include "paths.h"
#include "perf.h"
#include "power.h"
#include "../input/rumble.h"
#include "../settings/settings.h"
#include "../state/sram.h"

#define RESUME_COOLDOWN_MS  1500
#define AUDIO_MAX_CATCHUP   3
#define UI_TASK_INTERVAL_MS 16

#define PERF_AUTODUMP_INTERVAL_MS 15000
#define SLOW_CORE_PACE_RATIO      0.98

#define PACE_VSYNC_TOLERANCE 1.10
#define REFRESH_NOTICE_TLC   0.05

static inotify_status *idle_ino = NULL;
static int mux_idle_state_exists = 0;
static unsigned mux_idle_state_changes = 0;
static unsigned last_seen_changes = 0;
static char state_dir[MAX_BUFFER_SIZE];
static char macro_dir[MAX_BUFFER_SIZE];

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

static int build_state_dir(const char *core_path_arg, const char *content_path) {
    const char *base = strrchr(content_path, '/');
    base = base ? base + 1 : content_path;

    char content_stem[MAX_BUFFER_SIZE];
    if (!str_copy_checked(content_stem, sizeof(content_stem), base)) return 0;
    char *dot = strrchr(content_stem, '.');
    if (dot) *dot = '\0';

    char save_prefix[MAX_BUFFER_SIZE];
    if (!core_content_save_prefix(core_path_arg, content_path, save_prefix, sizeof(save_prefix))) return 0;

    const char *parts[] = {RETRO_STA_PATH, save_prefix, content_stem};
    if (!path_join_checked(state_dir, sizeof(state_dir), parts, A_SIZE(parts))) return 0;
    create_directories(state_dir, 0);
    return 1;
}

static int build_macro_dir(const char *core_path_arg, const char *content_path) {
    const char *base = strrchr(content_path, '/');
    base = base ? base + 1 : content_path;

    char content_stem[MAX_BUFFER_SIZE];
    if (!str_copy_checked(content_stem, sizeof(content_stem), base)) return 0;
    char *dot = strrchr(content_stem, '.');
    if (dot) *dot = '\0';

    char save_prefix[MAX_BUFFER_SIZE];
    if (!core_content_save_prefix(core_path_arg, content_path, save_prefix, sizeof(save_prefix))) return 0;

    const char *parts[] = {RETRO_MAC_PATH, save_prefix, content_stem};
    if (!path_join_checked(macro_dir, sizeof(macro_dir), parts, A_SIZE(parts))) return 0;
    create_directories(macro_dir, 0);
    return 1;
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
        if (!netplay_is_active() && session_settings_auto_save_on_idle()) gamestate_autosave_save();
    }
    last_seen_changes = mux_idle_state_changes;
}

static double core_run_ema_ms = 0.0;

static unsigned run_core_batch(const unsigned frames) {
    hw_render_bridge_context_save();
    unsigned ran = 0;

    if (frames == 1) frame_pacer_maybe_wait();

    for (unsigned i = 0; i < frames; i++) {
        const int is_last = i + 1 == frames || environment_av_info_pending();

        video_bridge_set_frame_skip(!is_last);
        input_bridge_begin_run();
        audio_bridge_notify_buffer_status();

        const int network_active = netplay_is_active();
        const int network_frame = network_active && netplay_is_playing();
        if (network_frame && !netplay_before_frame()) break;
        environment_notify_frame_time();
        if (is_last && !network_active && !cheevo_restricted()) runahead_before_frame(frames == 1);

        const uint64_t run_start = SDL_GetPerformanceCounter();
        current_core.retro_run();
        const double run_ms =
            (double) (SDL_GetPerformanceCounter() - run_start) * 1000.0 / (double) SDL_GetPerformanceFrequency();
        ran++;

        const uint64_t cheevo_frame_start = perf_begin();
        cheevo_do_frame();
        perf_end(perf_stage_cheevo_frame, cheevo_frame_start);

        if (network_frame) netplay_after_frame();

        core_run_ema_ms = core_run_ema_ms <= 0.0 ? run_ms : core_run_ema_ms * 0.9 + run_ms * 0.1;
        perf_record(perf_stage_core, run_ms);

        audio_bridge_flush_sample_fifo();

        if (is_last) break;
    }

    hw_render_bridge_flush_core_commands();
    hw_render_bridge_context_restore();
    video_bridge_set_frame_skip(0);
    return ran;
}

int core_content_needs_pacing(void) {
    if (session_settings.fps_limit != fps_limit_auto) return 0;

    const double locked = audio_bridge_locked_content_fps();
    if (locked <= 0.0) return 0;

    const int reported = display_panel_refresh_hz();
    const double panel = reported > 0 ? (double) reported : (double) frame_pacer_get_refresh_hz();

    return locked < panel * SLOW_CORE_PACE_RATIO;
}

static void pace_core_output(const uint64_t frame_start) {
    static double fps_limit_deadline = 0.0;

    if (hotkeys_is_fast_forward_active()) {
        fps_limit_deadline = 0.0;
        return;
    }

    const double budget_ms = target_fps > 0.0 ? 1000.0 / target_fps : 1000.0 / 60.0;
    const double spent_ms =
        (double) (SDL_GetPerformanceCounter() - frame_start) * 1000.0 / (double) SDL_GetPerformanceFrequency();
    const double slack_ms = budget_ms - spent_ms;

    const uint64_t audio_wait_start = perf_begin();
    audio_bridge_drc_tick();
    perf_record(perf_stage_audio_queue, audio_bridge_queued_ms());
    if (!link_is_engaged()) audio_bridge_wait_for_headroom(slack_ms > 0.0 ? (uint32_t) slack_ms : 0);
    perf_end(perf_stage_audio_wait, audio_wait_start);

    const int slowmo_active = hotkeys_is_slow_motion_active();
    double audio_target_ms =
        session_settings.fps_limit == fps_limit_auto && !link_is_engaged() ? audio_bridge_pace_target_ms() : 0.0;

    if (audio_target_ms > 0.0 && !slowmo_active) {
        const int reported = display_panel_refresh_hz();
        const double panel = reported > 0 ? (double) reported : (double) frame_pacer_get_refresh_hz();
        if (panel > 0.0 && audio_target_ms <= 1000.0 / panel * PACE_VSYNC_TOLERANCE) audio_target_ms = 0.0;
    }

    if (session_settings.fps_limit != fps_limit_50 && !slowmo_active && audio_target_ms <= 0.0) {
        fps_limit_deadline = 0.0;
        return;
    }

    double target_ms = session_settings.fps_limit == fps_limit_50 ? 20.0
                       : audio_target_ms > 0.0                    ? audio_target_ms
                                                                  : 1000.0 / target_fps;
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
    audio_bridge_note_core_frames(primed);
}

static int abort_startup(const int core_opened) {
    governor_boost_end();
    if (core_opened) core_unload();
    mux_input_close();
    sdl_cleanup();
    return EXIT_FAILURE;
}

int main(const int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        startup_options_print_usage(stdout, argv[0]);
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("muxretro interface 1\n");
        return EXIT_SUCCESS;
    }
    startup_options startup;
    if (!startup_options_parse(argc, argv, &startup)) {
        if (startup.unknown_option) fprintf(stderr, "Unknown option: %s\n", startup.unknown_option);
        startup_options_print_usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    const char *core_path_arg = startup.core_path;
    const char *content_path = startup.content_path;
    const char *startup_message = startup.restarting ? lang.muxretro.content_restarting : lang.muxretro.content_loading;
    const uint64_t startup_start = SDL_GetPerformanceCounter();
    uint64_t startup_stage = startup_start;

    power_session_init();

    load_device(&device);
    load_config(&config);
    const int show_startup_messages = config.visual.pickles_startup_messages;

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
    if (show_startup_messages) loading_message_show(startup_message);

    if (core_open(core_path_arg) != 0) {
        LOG_ERROR(mux_module, "Failed to open core: %s", core_path_arg);
        return abort_startup(0);
    }
    LOG_DEBUG(mux_module, "core_open done");
    startup_log_stage("frontend and core initialisation", &startup_stage);

    state_saves_init(core_path_arg);
    patch_manual_init(core_path_arg, content_path);
    perf_init();
    session_settings_init(core_path_arg, content_path);
    if (!coreinfo_feature_enabled(coreinfo_feature_run_ahead)) session_settings.run_ahead = 0;

    if (!build_state_dir(core_path_arg, content_path)) {
        LOG_ERROR(mux_module, "Save-state path is too long");
        return abort_startup(1);
    }
    if (!gamestate_init(state_dir)) {
        LOG_ERROR(mux_module, "Save-state path exceeds the supported length");
        return abort_startup(1);
    }

    char resume_path[512] = "";
    int load_blocked = 0;
    int has_resume = !startup.fresh && state_saves_supported()
                     && gamestate_find_most_recent(resume_path, sizeof(resume_path), &load_blocked) == 0;
    if (show_startup_messages) loading_message_show(has_resume ? lang.muxretro.content_resuming : startup_message);

    precache_content(content_path);

    if (startup.subsystem_ident[0]) {
        const char *slots[SUBSYSTEM_ROM_MAX];
        const int index = subsystem_find(startup.subsystem_ident);
        const int wanted = index >= 0 ? subsystem_list[index].rom_count : 0;

        for (int i = 0; i < wanted && i < SUBSYSTEM_ROM_MAX; i++)
            slots[i] = content_path;

        if (startup.single_screen) link_single_screen_set(1);
        link_set_mode(link_mode_local);

        if (wanted <= 0 || !subsystem_select(startup.subsystem_ident, slots, wanted))
            LOG_ERROR(
                mux_module, "Core has no '%s' subsystem, loading the single file instead", startup.subsystem_ident
            );
    }

    if (core_load_content(content_path) != 0) {
        LOG_ERROR(mux_module, "Failed to load content: %s", content_path);
        return abort_startup(1);
    }
    LOG_DEBUG(mux_module, "core_load_content done");
    startup_log_stage("content load", &startup_stage);

    pause_menu_playtime_reset();

    if (device.board.has_network) cheevo_init(core_resolved_content_path);

    sram_bridge_init(core_path_arg, content_path);
    cheats_init(core_path_arg, content_path);
    manual_init(core_path_arg, content_path);
    overlay_bridge_init(core_path_arg, content_path);

    if (!build_macro_dir(core_path_arg, content_path)) {
        LOG_ERROR(mux_module, "Macro path is too long");
        return abort_startup(1);
    }
    macros_init(macro_dir);

    content_hash_request(content_path, core_resolved_content_path);

    options_capture_baseline();
    LOG_DEBUG(mux_module, "options_capture_baseline done, options_count=%d", options_count);

    if (device.board.has_network && coreinfo_feature_enabled(coreinfo_feature_netplay)
        && netplay_init(core_path_arg, content_path) != 0)
        LOG_WARN(mux_module, "Network Play secure transport could not be initialised");

    video_bridge_apply_fps_limit();
    display_set_hard_sync_query(hard_sync_enabled);
    display_set_idle_saver_suppressed_query(netplay_is_active);

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

    int cheevo_connecting_background = 0;
    if (cheevo_is_starting()) {
        if (show_startup_messages) loading_message_show(lang.muxretro.cheevo.connecting);
        const uint32_t cheevo_deadline = SDL_GetTicks() + 1200;
        while (cheevo_is_starting() && !SDL_TICKS_PASSED(SDL_GetTicks(), cheevo_deadline)) {
            cheevo_tick();
            lv_task_handler();
            lv_refr_now(NULL);
            SDL_Delay(10);
        }
        if (cheevo_is_starting()) {
            cheevo_connecting_background = 1;
            LOG_INFO(mux_module, "RetroAchievements connection is continuing in the background");
        }
        if (show_startup_messages) loading_message_show(has_resume ? lang.muxretro.content_resuming : startup_message);
    }

    overlay_bridge_apply();
    mux_input_poll();
    input_bridge_suppress_held();

    int state_preserved = 0;
    if (state_saves_supported()) state_preserved = gamestate_protect_mismatched_autosave();

    resume_path[0] = '\0';
    load_blocked = 0;
    has_resume = !startup.fresh && state_saves_supported()
                 && gamestate_find_most_recent(resume_path, sizeof(resume_path), &load_blocked) == 0;

    if (has_resume) {
        if (show_startup_messages) loading_message_show(lang.muxretro.content_resuming);
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

        if (state_load(resume_path, show_startup_messages) == 0) {
            LOG_INFO(mux_module, "Auto-loaded most recent save state (after %d warm-up frames)", warmup_frames);
        }
        startup_log_stage("save-state restore", &startup_stage);
    } else {
        startup_log_stage("resume selection (no compatible state)", &startup_stage);
    }

    governor_boost_end();

    pause_menu_init();
    loading_message_hide();

    if (show_startup_messages && cheevo_connecting_background)
        pause_menu_show_toast_timed(lang.muxretro.cheevo.connecting_background, tst_wait_s);

    if (device.board.has_network && coreinfo_feature_enabled(coreinfo_feature_netplay)) {
        if (startup.netplay_invalid) {
            pause_menu_show_toast(lang.muxretro.netplay.startup_invalid);
        } else if (startup.netplay_host) {
            if (netplay_host(startup.netplay_port) != 0)
                pause_menu_show_toast(lang.muxretro.netplay.hosting_start_failed);
        } else if (startup.netplay_address[0]) {
            if (netplay_join(startup.netplay_address, startup.netplay_port) != 0)
                pause_menu_show_toast(lang.muxretro.netplay.startup_join_failed);
        }
    }
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

    const cheevo_status startup_cheevo_status = cheevo_get_status();
    if (show_startup_messages && !cheevo_connecting_background
        && (startup_cheevo_status == cheevo_status_active_softcore
            || startup_cheevo_status == cheevo_status_active_hardcore)) {
        cheevo_info startup_cheevo_info;
        cheevo_get_info(&startup_cheevo_info);
        if (startup_cheevo_info.notifications) pause_menu_show_toast_timed(lang.muxretro.cheevo.active, tst_wait_s);
    }

    LOG_SUCCESS(mux_module, "Running content at %.2f fps / %.0f Hz audio", target_fps, av_info.timing.sample_rate);
    LOG_INFO(mux_module, "Startup: ready in %.2f ms total", startup_elapsed_ms(startup_start));

    int quit = 0;
    int peeking = 0;
    int prev_paused = 0;
    int netplay_wait_visible = 0;
    int netplay_governor_active = 0;
    int refresh_notice_shown = 0;

    uint32_t fps_frame_count = 0;
    uint32_t fps_last_update = SDL_GetTicks();

    uint32_t sram_flush_deadline = SDL_GetTicks() + (uint32_t) session_settings.sram_flush_seconds * 1000;
    uint32_t status_deadline = SDL_GetTicks() + TIMER_STATUS;

    uint32_t timeline_deadline = 0;
    uint32_t ui_task_deadline = 0;
    uint32_t perf_autodump_deadline = SDL_GetTicks() + PERF_AUTODUMP_INTERVAL_MS;
    int timeline_armed_ms = 0;

    while (!quit) {
        int core_ran = 0;

        const uint64_t frame_start = SDL_GetPerformanceCounter();
        const uint32_t loop_now = SDL_GetTicks();
        const uint64_t services_start = perf_begin();

        mux_input_poll();
        const uint64_t cheevo_tick_start = perf_begin();
        cheevo_tick();
        perf_end(perf_stage_cheevo_tick, cheevo_tick_start);
        const uint64_t netplay_tick_start = perf_begin();
        netplay_tick();
        perf_end(perf_stage_netplay_tick, netplay_tick_start);
        const int netplay_active = netplay_is_active();
        if (netplay_active != netplay_governor_active) {
            if (netplay_active)
                governor_boost_begin("Network Play session");
            else
                governor_boost_end();
            netplay_governor_active = netplay_active;
        }
        idle_poll();
        perf_end(perf_stage_services, services_start);

        const uint64_t maintenance_start = perf_begin();
        if (power_session_poll()) {
            perf_end(perf_stage_maintenance, maintenance_start);
            quit = 1;
            continue;
        }
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
        const int network_menu_paused = netplay_menu_paused();
        const int show_netplay_wait = network_menu_paused && !paused;

        if (show_netplay_wait != netplay_wait_visible) {
            if (show_netplay_wait)
                loading_message_show(lang.muxretro.netplay.pause_menu_open);
            else
                loading_message_hide();
            netplay_wait_visible = show_netplay_wait;
        }

        const int content_paused = (paused && !netplay_is_playing() && !link_is_engaged()) || network_menu_paused;

        if (!netplay_active && (content_paused || hotkeys_is_content_paused())) governor_boost_gameplay_idle();
        if (prev_paused && !content_paused) core_prime_audio();

        prev_paused = content_paused;
        audio_bridge_set_paused(content_paused);

        const int timeline_ms = session_settings_timeline_interval_ms();
        if (timeline_ms != timeline_armed_ms) {
            timeline_armed_ms = timeline_ms;
            timeline_deadline = timeline_ms > 0 ? loop_now + (uint32_t) timeline_ms : 0;
        }

        if (timeline_ms > 0 && !paused && !hotkeys_is_content_paused() && state_saves_supported() && !netplay_active
            && !cheevo_restricted() && loop_now >= timeline_deadline) {
            gamestate_timeline_save();
            timeline_deadline = loop_now + (uint32_t) timeline_ms;
        }
        perf_end(perf_stage_maintenance, maintenance_start);

        int run_gameplay = 0;
        uint64_t control_start = perf_begin();
        if (paused) {
            if (!netplay_is_playing()) cheevo_idle();
            const int peek = pause_menu_peek_allowed() && mux_input_pressed(mux_input_menu);
            if (peek != peeking) {
                peeking = peek;
                display_set_ui_hidden(peek);

                if (!peek) {
                    pause_menu_sync_input_mask();
                    lv_obj_invalidate(ui_screen);
                }
            }

            if (!peek) {
                if (pause_menu_tick()) quit = 1;
            }

            if (!quit && (netplay_is_playing() || link_is_engaged()) && !network_menu_paused)
                run_gameplay = 1;
            else {
                perf_end(perf_stage_control, control_start);
                control_start = 0;
                SDL_Delay(10);
            }
        } else if (peeking) {
            peeking = 0;
            display_set_ui_hidden(0);
            lv_obj_invalidate(ui_screen);
        } else if (hotkeys_task()) {
            LOG_DEBUG(mux_module, "main: menu released without a hotkey combo, toggling pause");
            pause_menu_toggle();
        } else if (hotkeys_is_manual_requested()) {
            pause_menu_toggle();
            manual_menu_open();
        } else if (hotkeys_is_quit_requested()) {
            fade_out_screen_forced();
            quit = 1;
        } else if (netplay_blocks_core()) {
            perf_end(perf_stage_control, control_start);
            control_start = 0;
            SDL_Delay(5);
        } else if (network_menu_paused) {
            perf_end(perf_stage_control, control_start);
            control_start = 0;
            SDL_Delay(10);
        } else if (hotkeys_is_content_paused()) {
            perf_end(perf_stage_control, control_start);
            control_start = 0;
            SDL_Delay(10);
        } else {
            run_gameplay = 1;
        }
        perf_end(perf_stage_control, control_start);

        if (run_gameplay) {
            audio_bridge_apply_pending_min_latency();
            environment_apply_pending_av_info();

            const int ff_active = !netplay_active && hotkeys_is_fast_forward_active();
            const int slowmo_active = !netplay_active && hotkeys_is_slow_motion_active();

            unsigned frames = 1;
            if (ff_active) {
                const unsigned ff_batch = (unsigned) session_settings_ff_speed_value(session_settings.ff_speed);
                frames = ff_batch > 0 ? ff_batch : 1;
            } else if (!netplay_active && audio_bridge_is_prefilling()
                       && session_settings.fps_limit != fps_limit_50 && !slowmo_active
                       && audio_bridge_is_active() && audio_bridge_queued_ms() < audio_bridge_low_water_ms()) {
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

            const unsigned ran_frames = run_core_batch(frames);
            audio_bridge_note_core_frames(ran_frames);
            core_ran = ran_frames > 0;

            if (!refresh_notice_shown) {
                const double content_hz = audio_bridge_content_fps();
                const int reported_hz = display_panel_refresh_hz();
                const double panel_hz = reported_hz > 0 ? (double) reported_hz : (double) frame_pacer_get_refresh_hz();
                const double delta = content_hz > panel_hz ? content_hz - panel_hz : panel_hz - content_hz;

                if (content_hz > 0.0 && panel_hz > 0.0 && delta > panel_hz * REFRESH_NOTICE_TLC) {
                    refresh_notice_shown = 1;

                    char notice[MAX_BUFFER_SIZE];
                    snprintf(
                        notice, sizeof(notice), lang.muxretro.settings_screen.refresh_mismatch, (int) (content_hz + 0.5)
                    );
                    pause_menu_show_toast_timed(notice, tst_wait_s);
                    LOG_INFO(mux_module, "Content runs at %.2f Hz on a %.2f Hz display", content_hz, panel_hz);
                }
            }

            if (core_ran) {
                perf_note_batch(ran_frames);
                perf_record(perf_stage_frame_delay, frame_pacer_get_delay_ms());
            }

            fps_frame_count += ran_frames;
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

                if (!netplay_active) {
                    if (slowmo_active) {
                        governor_boost_gameplay_idle();
                    } else {
                        const double governor_target = session_settings.fps_limit == fps_limit_50 ? 50.0 : target_fps;
                        governor_boost_gameplay_update(vfps, governor_target, ff_active);
                    }
                }

                pause_menu_update_header();
                fps_frame_count = 0;
                fps_last_update = now_ticks;
            }
        }

        const uint64_t ui_tick_start = perf_begin();
        pause_menu_service_tick(loop_now);

        const int paused_now = pause_menu_is_active();
        const int ui_visible = paused_now || netplay_wait_visible;

        if (ui_visible) {
            if (!peeking) display_set_ui_hidden(0);
        } else {
            const int hud_active = pause_menu_gameplay_hud_active();
            display_set_ui_hidden(!hud_active);
        }
        perf_end(perf_stage_ui_logic, ui_tick_start);

        const uint64_t video_start = core_ran ? perf_begin() : 0;
        video_bridge_flush_frame();
        perf_end(perf_stage_video, video_start);
        if (ui_visible && SDL_TICKS_PASSED(loop_now, ui_task_deadline)) {
            const uint64_t ui_task_start = perf_begin();
            lv_task_handler();
            perf_end(perf_stage_ui_task, ui_task_start);
            ui_task_deadline = loop_now + UI_TASK_INTERVAL_MS;
        } else if (!ui_visible) {
            ui_task_deadline = loop_now;
        }

        const uint64_t present_start = core_ran ? perf_begin() : 0;
        if (display_ui_is_hidden()) {
            display_composite_frame();
        } else {
            const uint64_t present_before_refresh = display_present_serial();
            lv_refr_now(NULL);
            if (core_ran && display_present_serial() == present_before_refresh) display_composite_frame();
        }
        perf_end(perf_stage_present, present_start);
        if (core_ran) perf_note_present();

        frame_pacer_after_present();
        cheevo_present_tick();

        if (perf_capture_is_automatic() && loop_now >= perf_autodump_deadline) {
            perf_export_trace(RETRO_SHARE_PATH "performance.csv");
            perf_autodump_deadline = loop_now + PERF_AUTODUMP_INTERVAL_MS;
        }

        if (core_ran) pace_core_output(frame_start);
        perf_frame_complete(core_ran);
    }

    if (perf_is_capture_active()) {
        if (perf_export_trace(RETRO_SHARE_PATH "performance.csv") == 0)
            LOG_INFO(mux_module, "Performance capture written to " RETRO_SHARE_PATH "performance.csv");
    }

    if (netplay_wait_visible) loading_message_hide();
    if (netplay_governor_active) governor_boost_end();

    pause_menu_shutdown();
    netplay_shutdown();
    cheevo_shutdown();
    image_writer_shutdown();
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
        char subsystem_arg[80] = "";
        if (link_local_pending()) snprintf(subsystem_arg, sizeof(subsystem_arg), "--subsystem=%s", link_local_ident());

        char fresh_arg[] = "--fresh";
        char restart_arg[] = "--restart";
        char single_arg[] = "--single-screen";

        char *restart_argv[8];
        int arg_count = 0;

        restart_argv[arg_count++] = argv[0];
        restart_argv[arg_count++] = core_file_path;
        restart_argv[arg_count++] = core_content_path;
        restart_argv[arg_count++] = fresh_arg;
        restart_argv[arg_count++] = restart_arg;

        if (subsystem_arg[0]) restart_argv[arg_count++] = subsystem_arg;
        if (link_single_screen_setting()) restart_argv[arg_count++] = single_arg;

        restart_argv[arg_count] = NULL;
        execvp(restart_argv[0], restart_argv);
        LOG_ERROR(mux_module, "Failed to re-exec for restart (%s), exiting instead", strerror(errno));
    }

    return EXIT_SUCCESS;
}
