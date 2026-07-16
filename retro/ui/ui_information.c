#include <stdio.h>
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
#include "../settings/settings.h"

static int active = 0;
static uint64_t prev_nav_mask = 0;
static int hash_row_shown_ready = 0;
static int hash_row_index = 0;
static int bios_count = 0;

typedef enum { screen_main, screen_bios } screen_state_t;
static screen_state_t screen_state = screen_main;

static nav_repeat_t rpt_up = {0};
static nav_repeat_t rpt_down = {0};

static uint64_t current_nav_mask(void) {
    const int up = mux_input_pressed(mux_input_dpad_up);
    const int down = mux_input_pressed(mux_input_dpad_down);
    const int confirm = mux_input_pressed(mux_input_a);
    const int back = mux_input_pressed(mux_input_b);

    return (up ? BIT(0) : 0) | (down ? BIT(1) : 0) | (confirm ? BIT(2) : 0) | (back ? BIT(3) : 0) | nav_mask_page();
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

static void rebuild_rows(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;

    bios_count = bios_check_scan(core_file_path);
    const int row_offset = bios_count > 0 ? 1 : 0;
    hash_row_index = row_offset + 3;

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
        build_info_row(lang.muxretro.information_screen.system_bios, bios_summary, "folder");
    }

    struct retro_system_info info = {0};
    if (current_core.retro_get_system_info) current_core.retro_get_system_info(&info);

    struct retro_system_av_info av_info = {0};
    if (current_core.retro_get_system_av_info) current_core.retro_get_system_av_info(&av_info);

    const char *content_name = strrchr(core_content_path, '/');
    content_name = content_name ? content_name + 1 : core_content_path;

    content_hash_request(core_content_path);
    hash_row_shown_ready = content_hash_is_ready();

    char hash[16];
    snprintf(
        hash, sizeof(hash), "%s",
        hash_row_shown_ready ? content_hash_get() : lang.muxretro.information_screen.calculating
    );

    build_info_row(
        lang.muxretro.information_screen.core_name, info.library_name ? info.library_name : lang.generic.unknown, "core"
    );
    build_info_row(
        lang.muxretro.information_screen.core_version,
        info.library_version ? info.library_version : lang.generic.unknown, "version"
    );
    build_info_row(lang.generic.content, content_name, "content");
    build_info_row(lang.muxretro.information_screen.content_hash, hash, "hash");
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

    int frame_w = 0, frame_h = 0;
    video_bridge_get_frame_size(&frame_w, &frame_h);
    char resolution[32];
    snprintf(resolution, sizeof(resolution), "%s", lang.generic.unknown);
    if (frame_w > 0 && frame_h > 0) snprintf(resolution, sizeof(resolution), "%dx%d", frame_w, frame_h);
    build_info_row(lang.muxretro.information_screen.resolution, resolution, "resolution");

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

    char fps[16];
    snprintf(fps, sizeof(fps), "%s", lang.generic.unknown);
    if (av_info.timing.fps > 0) snprintf(fps, sizeof(fps), "%.2f", av_info.timing.fps);
    build_info_row(lang.muxretro.information_screen.target_fps, fps, "fps");

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
        lang.muxretro.information_screen.rumble_support,
        device.board.rumble[0] ? lang.generic.enabled : lang.generic.disabled, "rumble"
    );

    const int disc_count = mux_retro_disk_get_num_images();
    if (disc_count > 1) {
        char discs[8];
        snprintf(discs, sizeof(discs), "%d", disc_count);
        build_info_row(lang.muxretro.information_screen.disc_count, discs, "disc");
    }

    ui_count_static = row_offset + (disc_count > 1 ? 12 : 11);
    first_open = 0;
}

static void build_bios_rows(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;

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

static void close_information(void) {
    active = 0;

    pause_menu_rebuild();
    pause_menu_focus_information_item();
    pause_menu_show_nav_hints();
    pause_menu_sync_input_mask();
}

static void update_a_hint(void) {
    nav_show_a(screen_state == screen_main && bios_count > 0 && current_item_index == 0, lang.generic.select);
}

static void close_bios_screen(void) {
    screen_state = screen_main;
    rebuild_rows();

    update_a_hint();
}

void information_menu_open(void) {
    active = 1;
    screen_state = screen_main;
    prev_nav_mask = current_nav_mask();

    rebuild_rows();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0}, {ui_lbl_nav_b, lang.generic.back, 0}, {NULL, NULL, 0}});
    update_a_hint();
    pause_menu_fix_nav_order();
}

int information_menu_is_active(void) {
    return active;
}

void information_menu_tick(void) {
    if (screen_state == screen_main && !hash_row_shown_ready && content_hash_is_ready()) {
        hash_row_shown_ready = 1;

        const lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, hash_row_index);
        lv_obj_t *value = panel ? lv_obj_get_child(panel, 2) : NULL;
        if (value) lv_label_set_text(value, content_hash_get());
    }

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
        if (screen_state == screen_main && bios_count > 0 && current_item_index == 0) {
            play_sound(snd_confirm);
            screen_state = screen_bios;
            build_bios_rows();
            update_a_hint();
        }
    } else if (edge & BIT(3)) {
        play_sound(snd_back);
        if (screen_state == screen_bios) {
            close_bios_screen();
        } else {
            close_information();
        }
    }
}
