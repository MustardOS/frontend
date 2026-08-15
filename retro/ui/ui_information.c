#include <stdio.h>
#include <sys/stat.h>
#include "../../common/audio.h"
#include "../../common/device.h"
#include "../../common/input.h"
#include "../../common/ui/common.h"
#include "../../module/muxshare.h"
#include "../state/bios_check.h"
#include "../state/content_hash.h"
#include "../core/muxretro.h"
#include "../core/core.h"
#include "../input/nav_repeat.h"
#include "../input/rumble.h"
#include "../settings/settings.h"
#include "../video/hw_render.h"

static int active = 0;
static uint64_t prev_nav_mask = 0;
static int bios_count = 0;
static int bios_row_index = -1;
static int rumble_row_index = -1;
static int root_row_index = 0;
static int hash_row_index[content_hash_count];
static int hash_row_shown_ready[content_hash_count];

typedef enum {
    screen_root,
    screen_core,
    screen_content,
    screen_hash,
    screen_video,
    screen_audio,
    screen_bios
} screen_state_t;

static screen_state_t screen_state = screen_root;

static nav_repeat_t rpt_up = {0};
static nav_repeat_t rpt_down = {0};

static uint64_t current_nav_mask(void) {
    const int confirm = mux_input_pressed(mux_input_a);
    const int back = mux_input_pressed(mux_input_b);
    const int test_rumble = mux_input_pressed(mux_input_y);

    return (nav_dir_bits() & (BIT(0) | BIT(1))) | (confirm ? BIT(2) : 0) | (back ? BIT(3) : 0)
           | (test_rumble ? BIT(4) : 0) | nav_mask_page();
}

static void build_info_row(const char *label, const char *value, const char *glyph) {
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label_obj = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value_obj = lv_label_create(panel);

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label_obj, label, 1);
    apply_theme_list_glyph(&theme, icon, "muxretro", glyph);
    apply_theme_list_value(&theme, value_obj, value);
    apply_size_to_content(&theme, ui_pnl_content, label_obj, icon, label);
    apply_text_long_dot(&theme, label_obj);

    lv_group_add_obj(ui_group, label_obj);
    lv_group_add_obj(ui_group_glyph, icon);
    lv_group_add_obj(ui_group_panel, panel);
    lv_group_add_obj(ui_group_value, value_obj);
}

static void focus_item(const int index) {
    if (index < 0 || index >= ui_count_static) return;
    current_item_index = index;

    lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, index);
    if (!panel) return;

    lv_obj_t *label = lv_obj_get_child(panel, 0);
    lv_obj_t *glyph = lv_obj_get_child(panel, 1);
    lv_obj_t *value = lv_obj_get_child(panel, 2);

    nav_suppress_next_shake();

    if (label) lv_group_focus_obj(label);
    if (glyph) lv_group_focus_obj(glyph);
    if (value) lv_group_focus_obj(value);
    lv_group_focus_obj(panel);

    update_scroll_position(
        theme.mux.item.count, theme.mux.item.panel, ui_count_static, current_item_index, ui_pnl_content
    );
}

static void begin_rows(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;

    bios_row_index = -1;
    rumble_row_index = -1;
    for (int i = 0; i < content_hash_count; i++)
        hash_row_index[i] = -1;
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

static void build_root_rows(void) {
    begin_rows();

    build_info_row(lang.muxretro.information_screen.section_core, "", "core");
    build_info_row(lang.muxretro.information_screen.section_content, "", "content");
    build_info_row(lang.muxretro.information_screen.section_hash, "", "hash");
    build_info_row(lang.muxretro.information_screen.section_video, "", "videosettings");
    build_info_row(lang.muxretro.information_screen.section_audio, "", "audio");

    int rows = 5;

    bios_count = bios_check_scan(core_file_path);
    if (bios_count > 0) {
        int missing = 0;
        for (int i = 0; i < bios_count; i++) {
            const bios_entry_t *e = bios_check_get(i);
            if (e && !e->present) missing++;
        }

        char bios_summary[32];
        if (missing > 0) {
            snprintf(
                bios_summary, sizeof(bios_summary), "%d %s", missing, lang.muxretro.information_screen.bios_missing
            );
        } else {
            snprintf(bios_summary, sizeof(bios_summary), "%s", lang.muxretro.information_screen.bios_valid);
        }

        bios_row_index = rows;
        build_info_row(lang.muxretro.information_screen.system_bios, bios_summary, "folder");
        rows++;
    }

    ui_count_static = rows;
    first_open = 0;
}

static void build_core_rows(void) {
    begin_rows();

    struct retro_system_info info = {0};
    if (current_core.retro_get_system_info) current_core.retro_get_system_info(&info);

    build_info_row(
        lang.muxretro.information_screen.core_name, info.library_name ? info.library_name : lang.generic.unknown, "core"
    );
    build_info_row(
        lang.muxretro.information_screen.core_version,
        info.library_version ? info.library_version : lang.generic.unknown, "version"
    );

    char api[16];
    snprintf(api, sizeof(api), "%s", lang.generic.unknown);
    if (current_core.retro_api_version) snprintf(api, sizeof(api), "%u", current_core.retro_api_version());
    build_info_row(lang.muxretro.information_screen.core_api, api, "version");

    build_info_row(
        lang.muxretro.information_screen.extensions,
        current_core.valid_extensions && current_core.valid_extensions[0] ? current_core.valid_extensions
                                                                          : lang.generic.unknown,
        "content"
    );

    const size_t state_size =
        state_saves_supported() && current_core.retro_serialize_size ? current_core.retro_serialize_size() : 0;
    char state_text[32];
    if (state_size > 0) {
        format_bytes((long long) state_size, state_text, sizeof(state_text));
    } else {
        snprintf(state_text, sizeof(state_text), "%s", lang.muxretro.information_screen.not_supported);
    }
    build_info_row(lang.muxretro.information_screen.save_states, state_text, "state");

    const size_t sram_size =
        current_core.retro_get_memory_size ? current_core.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM) : 0;
    char sram_text[32];
    if (sram_size > 0) {
        format_bytes((long long) sram_size, sram_text, sizeof(sram_text));
    } else {
        snprintf(sram_text, sizeof(sram_text), "%s", lang.muxretro.information_screen.patches_none);
    }
    build_info_row(lang.muxretro.information_screen.save_memory, sram_text, "sram");

    build_info_row(
        lang.muxretro.information_screen.rumble_support,
        device.board.rumble[0] ? lang.generic.enabled : lang.generic.disabled, "rumble"
    );
    rumble_row_index = 6;

    ui_count_static = 7;
    first_open = 0;
}

static void build_content_rows(void) {
    begin_rows();

    const char *content_name = strrchr(core_content_path, '/');
    content_name = content_name ? content_name + 1 : core_content_path;

    build_info_row(lang.generic.content, content_name, "content");

    char location[PATH_MAX];
    snprintf(location, sizeof(location), "%s", core_content_path);
    char *slash = strrchr(location, '/');
    if (slash) *slash = '\0';

    const char *location_name = strrchr(location, '/');
    location_name = location_name ? location_name + 1 : location;
    build_info_row(lang.muxretro.information_screen.content_location, location_name, "folder");

    struct stat st;
    char size_text[32];
    if (stat(core_content_path, &st) == 0) {
        format_bytes(st.st_size, size_text, sizeof(size_text));
    } else {
        snprintf(size_text, sizeof(size_text), "%s", lang.generic.unknown);
    }
    build_info_row(lang.muxretro.information_screen.content_size, size_text, "storagesettings");

    build_info_row(
        lang.muxretro.information_screen.loaded_via,
        core_content_load_method[0] ? core_content_load_method : lang.generic.unknown, "loaded"
    );

    char patches_text[16];
    if (core_active_patch_count > 0) {
        snprintf(patches_text, sizeof(patches_text), "%d", core_active_patch_count);
    } else {
        snprintf(patches_text, sizeof(patches_text), "%s", lang.muxretro.information_screen.patches_none);
    }
    build_info_row(lang.muxretro.information_screen.active_patches, patches_text, "patch");

    int rows = 5;

    const int disc_count = mux_retro_disk_get_num_images();
    if (disc_count > 1) {
        char discs[8];
        snprintf(discs, sizeof(discs), "%d", disc_count);
        build_info_row(lang.muxretro.information_screen.disc_count, discs, "disc");
        rows++;
    }

    ui_count_static = rows;
    first_open = 0;
}

static void build_hash_row(const enum content_hash_kind kind, const char *label, const int row) {
    hash_row_shown_ready[kind] = content_hash_is_ready(kind);
    hash_row_index[kind] = row;

    build_info_row(
        label, hash_row_shown_ready[kind] ? content_hash_get(kind) : lang.muxretro.information_screen.calculating,
        "hash"
    );
}

static void build_hash_rows(void) {
    begin_rows();

    content_hash_request(core_content_path, core_resolved_content_path);

    int rows = 0;
    if (content_came_from_archive()) {
        build_hash_row(content_hash_archive, lang.muxretro.information_screen.archive_hash, rows);
        rows++;
    }

    build_hash_row(content_hash_content, lang.muxretro.information_screen.content_hash, rows);
    rows++;

    build_hash_row(content_hash_cheevo, lang.muxretro.information_screen.cheevo_hash, rows);
    rows++;

    ui_count_static = rows;
    first_open = 0;
}

static void build_video_rows(void) {
    begin_rows();

    struct retro_system_av_info av_info = {0};
    if (current_core.retro_get_system_av_info) current_core.retro_get_system_av_info(&av_info);

    int frame_w = 0, frame_h = 0;
    video_bridge_get_frame_size(&frame_w, &frame_h);
    char resolution[32];
    snprintf(resolution, sizeof(resolution), "%s", lang.generic.unknown);
    if (frame_w > 0 && frame_h > 0) snprintf(resolution, sizeof(resolution), "%dx%d", frame_w, frame_h);
    build_info_row(lang.muxretro.information_screen.resolution, resolution, "resolution");

    char max_resolution[32];
    snprintf(max_resolution, sizeof(max_resolution), "%s", lang.generic.unknown);
    if (av_info.geometry.max_width > 0 && av_info.geometry.max_height > 0) {
        snprintf(
            max_resolution, sizeof(max_resolution), "%ux%u", av_info.geometry.max_width, av_info.geometry.max_height
        );
    }
    build_info_row(lang.muxretro.information_screen.max_resolution, max_resolution, "resolution");

    char aspect[32];
    snprintf(aspect, sizeof(aspect), "%s", lang.generic.unknown);
    if (av_info.geometry.aspect_ratio > 0.0f) {
        snprintf(aspect, sizeof(aspect), "%.3f", (double) av_info.geometry.aspect_ratio);
    } else if (frame_w > 0 && frame_h > 0) {
        snprintf(aspect, sizeof(aspect), "%.3f", (double) frame_w / (double) frame_h);
    }
    build_info_row(lang.muxretro.settings_screen.aspect_ratio, aspect, "aspectratio");

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
    build_info_row(lang.muxretro.information_screen.pixel_format, pixel_format, "screeninfo");

    int dest_w = 0, dest_h = 0;
    video_bridge_get_dest_size(&dest_w, &dest_h);
    char display_output[64];
    snprintf(display_output, sizeof(display_output), "%s", lang.generic.unknown);
    if (dest_w > 0 && dest_h > 0) {
        snprintf(
            display_output, sizeof(display_output), "%dx%d (%s)", dest_w, dest_h,
            session_settings_scale_name(session_settings.scaling_mode)
        );
    }
    build_info_row(lang.muxretro.information_screen.display_output, display_output, "display");

    // No hardware context means the core is drawing frames on the CPU itself
    const char *renderer = hw_render_bridge_description();
    build_info_row(
        lang.muxretro.information_screen.renderer,
        renderer ? renderer : lang.muxretro.information_screen.renderer_software, "videosettings"
    );

    char fps[16];
    snprintf(fps, sizeof(fps), "%s", lang.generic.unknown);
    if (av_info.timing.fps > 0) snprintf(fps, sizeof(fps), "%.2f", av_info.timing.fps);
    build_info_row(lang.muxretro.information_screen.target_fps, fps, "fps");

    ui_count_static = 7;
    first_open = 0;
}

static void build_audio_rows(void) {
    begin_rows();

    int audio_freq = 0, audio_channels = 0;
    audio_bridge_get_info(&audio_freq, &audio_channels);
    char audio_output[32];
    snprintf(audio_output, sizeof(audio_output), "%s", lang.generic.unknown);
    if (audio_freq > 0) {
        snprintf(
            audio_output, sizeof(audio_output), "%d Hz %s", audio_freq,
            audio_channels >= 2 ? lang.muxretro.information_screen.stereo : lang.muxretro.information_screen.mono
        );
    }
    build_info_row(lang.muxretro.information_screen.audio_output, audio_output, "audio");

    build_info_row(
        lang.muxretro.settings_screen.audio_latency,
        session_settings_audio_latency_name(session_settings.audio_latency_profile), "audiolatency"
    );

    char buffer_text[16];
    snprintf(buffer_text, sizeof(buffer_text), "%u ms", audio_bridge_high_water_ms());
    build_info_row(lang.muxretro.information_screen.audio_buffer, buffer_text, "audioperiod");

    build_info_row(
        lang.muxretro.settings_screen.audio_filter, session_settings_audio_filter_name(session_settings.audio_filter),
        "audiofilter"
    );

    ui_count_static = 4;
    first_open = 0;
}

static void build_bios_rows(void) {
    begin_rows();

    for (int i = 0; i < bios_count; i++) {
        const bios_entry_t *e = bios_check_get(i);
        if (!e) continue;

        char value[64];
        snprintf(
            value, sizeof(value), "%s (%s)",
            e->present ? lang.muxretro.information_screen.bios_valid : lang.muxretro.information_screen.bios_missing,
            e->optional ? lang.muxretro.information_screen.bios_optional
                        : lang.muxretro.information_screen.bios_required
        );
        build_info_row(e->desc, value, e->present ? "valid" : "missing");
    }

    ui_count_static = bios_count;
    first_open = 0;
}

static void build_screen(void) {
    switch (screen_state) {
        case screen_core:
            build_core_rows();
            break;
        case screen_content:
            build_content_rows();
            break;
        case screen_hash:
            build_hash_rows();
            break;
        case screen_video:
            build_video_rows();
            break;
        case screen_audio:
            build_audio_rows();
            break;
        case screen_bios:
            build_bios_rows();
            break;
        default:
            build_root_rows();
            break;
    }
}

static void close_information(void) {
    rumble_bridge_test_cancel();
    active = 0;

    pause_menu_rebuild();
    pause_menu_focus_information_item();
    pause_menu_show_nav_hints();
    pause_menu_sync_input_mask();
}

static int row_is_enterable(void) {
    return screen_state == screen_root;
}

static void update_a_hint(void) {
    nav_show_a(row_is_enterable(), lang.generic.select);
}

void information_menu_open(void) {
    active = 1;
    screen_state = screen_root;
    prev_nav_mask = current_nav_mask();

    content_hash_request(core_content_path, core_resolved_content_path);
    build_screen();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0}, {ui_lbl_nav_b, lang.generic.back, 0}, {NULL, NULL, 0}});
    update_a_hint();
    pause_menu_fix_nav_order();
}

int information_menu_is_active(void) {
    return active;
}

static void refresh_pending_hashes(void) {
    if (screen_state != screen_hash) return;

    for (int kind = 0; kind < content_hash_count; kind++) {
        if (hash_row_index[kind] < 0 || hash_row_shown_ready[kind]) continue;
        if (!content_hash_is_ready(kind)) continue;

        hash_row_shown_ready[kind] = 1;

        const lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, hash_row_index[kind]);
        lv_obj_t *value = panel ? lv_obj_get_child(panel, 2) : NULL;
        if (value) lv_label_set_text(value, content_hash_get(kind));
    }
}

static void enter_row(void) {
    if (screen_state != screen_root) return;

    root_row_index = current_item_index;
    if (current_item_index == bios_row_index) {
        screen_state = screen_bios;
    } else {
        switch (current_item_index) {
            case 0:
                screen_state = screen_core;
                break;
            case 1:
                screen_state = screen_content;
                break;
            case 2:
                screen_state = screen_hash;
                break;
            case 3:
                screen_state = screen_video;
                break;
            default:
                screen_state = screen_audio;
                break;
        }
    }

    play_sound(snd_confirm);
    build_screen();
    update_a_hint();
}

static void leave_row(void) {
    play_sound(snd_back);

    if (screen_state == screen_root) {
        close_information();
        return;
    }

    screen_state = screen_root;

    build_screen();
    focus_item(root_row_index);
    update_a_hint();
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
        update_a_hint();
    } else if (do_down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 2, 0, 1);
        update_a_hint();
    } else if (nav_page_tick(edge, mask, 2)) {
        update_a_hint();
    } else if (edge & BIT(2)) {
        enter_row();
    } else if (edge & BIT(3)) {
        leave_row();
    } else if (edge & BIT(4)) {
        if (screen_state == screen_core && current_item_index == rumble_row_index) rumble_bridge_test_start();
    }
}
