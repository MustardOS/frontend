#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include "../../common/audio.h"
#include "../../common/device.h"
#include "../../common/input.h"
#include "../../common/ui/common.h"
#include "../../common/ui/list_frame.h"
#include "../../module/muxshare.h"
#include "../core/core.h"
#include "../core/muxretro.h"
#include "../core/paths.h"
#include "../coreinfo/coreinfo.h"
#include "../input/nav_repeat.h"
#include "../input/rumble.h"
#include "../settings/settings.h"
#include "../state/bios_check.h"
#include "../state/content_hash.h"
#include "../video/hw_render.h"
#include "ui_loading.h"

#define INFORMATION_ROW_MAX   96
#define INFORMATION_FRAME_MAX 7

typedef enum {
    screen_check,
    screen_core,
    screen_content,
    screen_hash,
    screen_video,
    screen_audio,
    screen_bios
} screen_state_t;

static int active;
static uint64_t prev_nav_mask;
static int bios_count;
static long long save_space_bytes = -1;
static int save_space_low;
static int rumble_row_index = -1;
static int hash_row_index[content_hash_count];
static int hash_row_shown_ready[content_hash_count];
static int checklist_scanned;
static int information_row_count;
static int information_frame_count;
static lv_obj_t *information_panels[INFORMATION_ROW_MAX];
static lv_obj_t *information_labels[INFORMATION_ROW_MAX];
static lv_obj_t *information_glyphs[INFORMATION_ROW_MAX];
static lv_obj_t *information_values[INFORMATION_ROW_MAX];
static list_frame information_frames[INFORMATION_FRAME_MAX];
static screen_state_t information_frame_screens[INFORMATION_FRAME_MAX];
static screen_state_t screen_state = screen_check;
static nav_repeat_t rpt_up;
static nav_repeat_t rpt_down;

static uint64_t current_nav_mask(void) {
    const int back = mux_input_pressed(mux_input_b);
    const int test_rumble = mux_input_pressed(mux_input_y);

    return nav_dir_bits() | (back ? BIT(5) : 0) | (test_rumble ? BIT(6) : 0) | nav_mask_page();
}

static int build_info_row(const char *label, const char *value, const char *glyph, const int full_width) {
    if (information_row_count >= INFORMATION_ROW_MAX) return -1;

    const int row = information_row_count++;
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label_obj = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value_obj = lv_label_create(panel);

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label_obj, label, !full_width);
    apply_theme_list_glyph(&theme, icon, "muxretro", glyph);
    apply_theme_list_value(&theme, value_obj, full_width ? "" : value);
    apply_size_to_content(&theme, ui_pnl_content, label_obj, icon, label);
    apply_text_long_dot(&theme, label_obj);

    information_panels[row] = panel;
    information_labels[row] = label_obj;
    information_glyphs[row] = icon;
    information_values[row] = value_obj;

    return row;
}

static void begin_rows(void) {
    list_frame_reset();
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    information_row_count = 0;
    information_frame_count = 0;
    ui_count_static = 0;
    current_item_index = 0;
    rumble_row_index = -1;

    memset(information_panels, 0, sizeof(information_panels));
    memset(information_labels, 0, sizeof(information_labels));
    memset(information_glyphs, 0, sizeof(information_glyphs));
    memset(information_values, 0, sizeof(information_values));

    for (int i = 0; i < content_hash_count; i++) {
        hash_row_index[i] = -1;
        hash_row_shown_ready[i] = 0;
    }
}

static void add_frame(const screen_state_t screen, const char *label, const int first) {
    if (information_frame_count >= INFORMATION_FRAME_MAX || information_row_count <= first) return;

    information_frames[information_frame_count] =
        (list_frame) {.label = label, .first = first, .count = information_row_count - first};
    information_frame_screens[information_frame_count] = screen;
    information_frame_count++;
}

static int content_came_from_archive(void) {
    return core_resolved_content_path[0] && strcmp(core_resolved_content_path, core_content_path) != 0;
}

static void format_bytes(const long long bytes, char *out, const size_t out_size) {
    if (bytes >= 1024LL * 1024 * 1024) {
        snprintf(out, out_size, "%.1f GB", (double) bytes / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024 * 1024) {
        snprintf(out, out_size, "%.1f MB", (double) bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        snprintf(out, out_size, "%.1f KB", (double) bytes / 1024.0);
    } else {
        snprintf(out, out_size, "%lld B", bytes);
    }
}

static void scan_checklist(void) {
    bios_count = bios_check_scan(core_file_path);

    save_space_bytes = -1;
    save_space_low = 0;
    struct statvfs storage;
    if (statvfs(RETRO_SHARE_PATH, &storage) == 0) {
        const unsigned long long available =
            (unsigned long long) storage.f_bavail * (unsigned long long) storage.f_frsize;
        save_space_bytes = available > (unsigned long long) LLONG_MAX ? LLONG_MAX : (long long) available;
        save_space_low = available < 32ULL * 1024ULL * 1024ULL;
    }
}

static const char *capability_reason(const enum coreinfo_feature feature) {
    switch (coreinfo_feature_reason_for(feature)) {
        case coreinfo_reason_packaged_rule:
            return lang.muxretro.information_screen.capability_packaged;
        case coreinfo_reason_requires_save_states:
            return lang.muxretro.information_screen.capability_states_required;
        default:
            return lang.muxretro.information_screen.capability_core_unsupported;
    }
}

static void build_checklist_rows(void) {
    const int states_available = state_saves_supported();
    build_info_row(
        lang.muxretro.information_screen.save_protection,
        states_available ? lang.muxretro.information_screen.available : capability_reason(coreinfo_feature_save_states),
        states_available ? "valid" : "info", 0
    );

    char save_space[32];
    if (save_space_bytes >= 0) {
        format_bytes(save_space_bytes, save_space, sizeof(save_space));
    } else {
        snprintf(save_space, sizeof(save_space), "%s", lang.generic.unknown);
    }
    build_info_row(lang.muxretro.information_screen.save_space, save_space, save_space_low ? "info" : "valid", 0);

    if (device.board.has_network) {
        const int netplay_available = states_available && coreinfo_feature_enabled(coreinfo_feature_netplay);
        build_info_row(
            lang.muxretro.information_screen.network_play,
            netplay_available   ? lang.muxretro.information_screen.available
            : !states_available ? lang.muxretro.information_screen.capability_states_required
                                : capability_reason(coreinfo_feature_netplay),
            netplay_available ? "valid" : "info", 0
        );
    }

    const int settings_recovered = session_settings_launch_recovered();
    build_info_row(
        lang.muxretro.information_screen.settings_recovery,
        settings_recovered ? lang.muxretro.information_screen.recovery_restored
                           : lang.muxretro.information_screen.recovery_not_needed,
        settings_recovered ? "settings" : "valid", 0
    );
}

static void build_core_rows(void) {
    struct retro_system_info info = {0};
    if (current_core.retro_get_system_info) current_core.retro_get_system_info(&info);

    build_info_row(
        lang.muxretro.information_screen.core_name, info.library_name ? info.library_name : lang.generic.unknown,
        "core", 0
    );
    build_info_row(
        lang.muxretro.information_screen.core_version,
        info.library_version ? info.library_version : lang.generic.unknown, "version", 0
    );

    char api[16];
    snprintf(api, sizeof(api), "%s", lang.generic.unknown);
    if (current_core.retro_api_version) snprintf(api, sizeof(api), "%u", current_core.retro_api_version());
    build_info_row(lang.muxretro.information_screen.core_api, api, "version", 0);

    build_info_row(
        lang.muxretro.information_screen.extensions,
        current_core.valid_extensions && current_core.valid_extensions[0] ? current_core.valid_extensions
                                                                          : lang.generic.unknown,
        "content", 0
    );

    const size_t state_size =
        state_saves_supported() && current_core.retro_serialize_size ? current_core.retro_serialize_size() : 0;
    char state_text[32];
    if (state_size > 0) {
        format_bytes((long long) state_size, state_text, sizeof(state_text));
    } else {
        snprintf(state_text, sizeof(state_text), "%s", lang.muxretro.information_screen.not_supported);
    }
    build_info_row(lang.muxretro.information_screen.save_states, state_text, "state", 0);

    const size_t sram_size =
        current_core.retro_get_memory_size ? current_core.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM) : 0;
    char sram_text[32];
    if (sram_size > 0) {
        format_bytes((long long) sram_size, sram_text, sizeof(sram_text));
    } else {
        snprintf(sram_text, sizeof(sram_text), "%s", lang.muxretro.information_screen.patches_none);
    }
    build_info_row(lang.muxretro.information_screen.save_memory, sram_text, "sram", 0);

    rumble_row_index = build_info_row(
        lang.muxretro.information_screen.rumble_support,
        device.board.rumble[0] ? lang.generic.enabled : lang.generic.disabled, "rumble", 0
    );
}

static void build_content_rows(void) {
    const char *content_name = strrchr(core_content_path, '/');
    content_name = content_name ? content_name + 1 : core_content_path;
    build_info_row(lang.generic.content, content_name, "content", 0);

    char location[PATH_MAX];
    snprintf(location, sizeof(location), "%s", core_content_path);
    char *slash = strrchr(location, '/');
    if (slash) *slash = '\0';

    const char *location_name = strrchr(location, '/');
    location_name = location_name ? location_name + 1 : location;
    build_info_row(lang.muxretro.information_screen.content_location, location_name, "folder", 0);

    struct stat st;
    char size_text[32];
    if (stat(core_content_path, &st) == 0) {
        format_bytes(st.st_size, size_text, sizeof(size_text));
    } else {
        snprintf(size_text, sizeof(size_text), "%s", lang.generic.unknown);
    }
    build_info_row(lang.muxretro.information_screen.content_size, size_text, "storagesettings", 0);

    build_info_row(
        lang.muxretro.information_screen.loaded_via,
        core_content_load_method[0] ? core_content_load_method : lang.generic.unknown, "loaded", 0
    );

    char patches_text[16];
    if (core_active_patch_count > 0) {
        snprintf(patches_text, sizeof(patches_text), "%d", core_active_patch_count);
    } else {
        snprintf(patches_text, sizeof(patches_text), "%s", lang.muxretro.information_screen.patches_none);
    }
    build_info_row(lang.muxretro.information_screen.active_patches, patches_text, "patch", 0);

    const int disc_count = mux_retro_disk_get_num_images();
    if (disc_count > 1) {
        char discs[8];
        snprintf(discs, sizeof(discs), "%d", disc_count);
        build_info_row(lang.muxretro.information_screen.disc_count, discs, "disc", 0);
    }
}

static void build_hash_row(const enum content_hash_kind kind, const char *label, const char *glyph) {
    hash_row_shown_ready[kind] = content_hash_is_ready(kind);
    build_info_row(label, "", glyph, 1);
    hash_row_index[kind] = build_info_row(
        hash_row_shown_ready[kind] ? content_hash_get(kind) : lang.muxretro.information_screen.calculating, "", "", 1
    );
}

static void build_hash_rows(void) {
    if (content_came_from_archive()) {
        build_hash_row(content_hash_archive, lang.muxretro.information_screen.archive_hash, "archivehash");
    }

    build_hash_row(content_hash_content, lang.muxretro.information_screen.content_hash, "contenthash");
    build_hash_row(content_hash_cheevo, lang.muxretro.information_screen.cheevo_hash, "achievementhash");
}

static void build_video_rows(void) {
    struct retro_system_av_info av_info = {0};
    if (current_core.retro_get_system_av_info) current_core.retro_get_system_av_info(&av_info);

    int frame_w = 0;
    int frame_h = 0;
    video_bridge_get_frame_size(&frame_w, &frame_h);
    char resolution[32];
    snprintf(resolution, sizeof(resolution), "%s", lang.generic.unknown);
    if (frame_w > 0 && frame_h > 0) snprintf(resolution, sizeof(resolution), "%dx%d", frame_w, frame_h);
    build_info_row(lang.muxretro.information_screen.resolution, resolution, "resolution", 0);

    char max_resolution[32];
    snprintf(max_resolution, sizeof(max_resolution), "%s", lang.generic.unknown);
    if (av_info.geometry.max_width > 0 && av_info.geometry.max_height > 0) {
        snprintf(
            max_resolution, sizeof(max_resolution), "%ux%u", av_info.geometry.max_width, av_info.geometry.max_height
        );
    }
    build_info_row(lang.muxretro.information_screen.max_resolution, max_resolution, "resolution", 0);

    char aspect[32];
    snprintf(aspect, sizeof(aspect), "%s", lang.generic.unknown);
    if (av_info.geometry.aspect_ratio > 0.0f) {
        snprintf(aspect, sizeof(aspect), "%.3f", (double) av_info.geometry.aspect_ratio);
    } else if (frame_w > 0 && frame_h > 0) {
        snprintf(aspect, sizeof(aspect), "%.3f", (double) frame_w / (double) frame_h);
    }
    build_info_row(lang.muxretro.settings_screen.aspect_ratio, aspect, "aspectratio", 0);

    const char *pixel_format;
    switch (mux_retro_get_pixel_format()) {
        case RETRO_PIXEL_FORMAT_XRGB8888:
            pixel_format = "XRGB8888";
            break;
        case RETRO_PIXEL_FORMAT_RGB565:
            pixel_format = "RGB565";
            break;
        default:
            pixel_format = "0RGB1555";
            break;
    }
    build_info_row(lang.muxretro.information_screen.pixel_format, pixel_format, "screeninfo", 0);

    int dest_w = 0;
    int dest_h = 0;
    video_bridge_get_dest_size(&dest_w, &dest_h);
    char display_output[64];
    snprintf(display_output, sizeof(display_output), "%s", lang.generic.unknown);
    if (dest_w > 0 && dest_h > 0) {
        snprintf(
            display_output, sizeof(display_output), "%dx%d (%s)", dest_w, dest_h,
            session_settings_scale_name(session_settings.scaling_mode)
        );
    }
    build_info_row(lang.muxretro.information_screen.display_output, display_output, "display", 0);

    const char *renderer = hw_render_bridge_description();
    build_info_row(
        lang.muxretro.information_screen.renderer,
        renderer ? renderer : lang.muxretro.information_screen.renderer_software, "videosettings", 0
    );

    char fps[16];
    snprintf(fps, sizeof(fps), "%s", lang.generic.unknown);
    if (av_info.timing.fps > 0) snprintf(fps, sizeof(fps), "%.2f", av_info.timing.fps);
    build_info_row(lang.muxretro.information_screen.target_fps, fps, "fps", 0);
}

static void build_audio_rows(void) {
    int audio_freq = 0;
    int audio_channels = 0;
    audio_bridge_get_info(&audio_freq, &audio_channels);
    char audio_output[32];
    snprintf(audio_output, sizeof(audio_output), "%s", lang.generic.unknown);
    if (audio_freq > 0) {
        snprintf(
            audio_output, sizeof(audio_output), "%d Hz %s", audio_freq,
            audio_channels >= 2 ? lang.muxretro.information_screen.stereo : lang.muxretro.information_screen.mono
        );
    }
    build_info_row(lang.muxretro.information_screen.audio_output, audio_output, "audio", 0);

    build_info_row(
        lang.muxretro.settings_screen.audio_latency,
        session_settings_audio_latency_name(session_settings.audio_latency_profile), "audiolatency", 0
    );

    char buffer_text[16];
    snprintf(buffer_text, sizeof(buffer_text), "%u ms", audio_bridge_high_water_ms());
    build_info_row(lang.muxretro.information_screen.audio_buffer, buffer_text, "audioperiod", 0);

    build_info_row(
        lang.muxretro.settings_screen.audio_filter, session_settings_audio_filter_name(session_settings.audio_filter),
        "audiofilter", 0
    );
}

static void build_bios_rows(void) {
    for (int i = 0; i < bios_count; i++) {
        const bios_entry_t *entry = bios_check_get(i);
        if (!entry) continue;

        char value[64];
        snprintf(
            value, sizeof(value), "%s (%s)",
            entry->present ? lang.muxretro.information_screen.bios_valid
                           : lang.muxretro.information_screen.bios_missing,
            entry->optional ? lang.muxretro.information_screen.bios_optional
                            : lang.muxretro.information_screen.bios_required
        );
        build_info_row(entry->desc, value, entry->present ? "valid" : "missing", 0);
    }
}

static void build_information_sections(void) {
    begin_rows();

    if (!checklist_scanned) {
        scan_checklist();
        checklist_scanned = 1;
    }

    content_hash_request(core_content_path, core_resolved_content_path);

    int first = information_row_count;
    build_core_rows();
    add_frame(screen_core, lang.muxretro.information_screen.section_core, first);

    first = information_row_count;
    build_content_rows();
    add_frame(screen_content, lang.muxretro.information_screen.section_content, first);

    first = information_row_count;
    build_hash_rows();
    add_frame(screen_hash, lang.muxretro.information_screen.section_hash, first);

    first = information_row_count;
    build_video_rows();
    add_frame(screen_video, lang.muxretro.information_screen.section_video, first);

    first = information_row_count;
    build_audio_rows();
    add_frame(screen_audio, lang.muxretro.information_screen.section_audio, first);

    if (bios_count > 0) {
        first = information_row_count;
        build_bios_rows();
        add_frame(screen_bios, lang.muxretro.information_screen.system_bios, first);
    }

    first = information_row_count;
    build_checklist_rows();
    add_frame(screen_check, lang.muxretro.information_screen.checklist, first);

    if (list_frame_init(
            &theme, ui_pnl_content, information_frames, information_frame_count, information_panels, information_labels,
            information_glyphs, information_values, information_row_count
        )) {
        list_frame_apply();
        screen_state = information_frame_screens[list_frame_current()];
        gen_step_movement(0, +1, 2, 0, 0);
    }

    first_open = 0;
}

static void close_information(void) {
    rumble_bridge_test_cancel();
    list_frame_reset();
    active = 0;

    pause_menu_rebuild();
    pause_menu_focus_information_item();
    pause_menu_show_nav_hints();
    pause_menu_sync_input_mask();
}

static void refresh_information_nav(void) {
    nav_show_a(0, NULL);
    nav_show_lr(0);

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0}, {ui_lbl_nav_b, lang.generic.back, 0}, {NULL, NULL, 0}});
    lv_label_set_text(ui_lbl_nav_lr, lang.generic.change);
    nav_show_lr(list_frame_focused());
    pause_menu_fix_nav_order();
}

void information_menu_open(void) {
    const int show_loading = !checklist_scanned;

    active = 1;
    prev_nav_mask = current_nav_mask();

    if (show_loading) loading_message_show(lang.muxretro.information_loading);
    build_information_sections();
    if (show_loading) loading_message_hide();

    refresh_information_nav();
}

int information_menu_is_active(void) {
    return active;
}

static void refresh_pending_hashes(void) {
    for (int kind = 0; kind < content_hash_count; kind++) {
        const int row = hash_row_index[kind];
        if (row < 0 || row >= information_row_count || hash_row_shown_ready[kind]) continue;
        if (!content_hash_is_ready(kind)) continue;

        hash_row_shown_ready[kind] = 1;
        if (information_labels[row]) lv_label_set_text(information_labels[row], content_hash_get(kind));
    }
}

static void change_section(const int direction) {
    if (!list_frame_move(direction)) return;

    screen_state = information_frame_screens[list_frame_current()];
    play_sound(snd_option);
    gen_step_movement(0, +1, 2, 0, 0);
    refresh_information_nav();
}

void information_menu_tick(void) {
    rumble_bridge_test_tick();
    refresh_pending_hashes();

    const uint64_t mask = current_nav_mask();
    const uint64_t edge = mask & ~prev_nav_mask;
    prev_nav_mask = mask;

    if (nav_input_halted()) return;

    const uint32_t now = SDL_GetTicks();
    int do_up = nav_repeat_step(&rpt_up, edge & BIT(0), mask & BIT(0), current_item_index > 0, now);
    int do_down =
        nav_repeat_step(&rpt_down, edge & BIT(1), mask & BIT(1), current_item_index < ui_count_static - 1, now);

    if (ui_count_static < 2) {
        do_up = 0;
        do_down = 0;
    }

    if (do_up) {
        nav_set_last_dir(nav_dir_up);
        nav_unsuppress_shake();
        gen_step_movement(1, -1, 2, 0, 1);
        refresh_information_nav();
    } else if (do_down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 2, 0, 1);
        refresh_information_nav();
    } else if ((edge & BIT(2)) && list_frame_focused()) {
        change_section(-1);
    } else if ((edge & BIT(3)) && list_frame_focused()) {
        change_section(+1);
    } else if (edge & NAV_PAGE_UP_BIT) {
        change_section(-1);
    } else if (edge & NAV_PAGE_DOWN_BIT) {
        change_section(+1);
    } else if (edge & BIT(5)) {
        play_sound(snd_back);
        close_information();
    } else if (edge & BIT(6)) {
        if (screen_state == screen_core && list_frame_current_row() == rumble_row_index) rumble_bridge_test_start();
    }
}
