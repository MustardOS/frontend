#include <stdio.h>
#include "../common/audio.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/input.h"
#include "../common/miniz/miniz.h"
#include "../common/ui/common.h"
#include "../module/muxshare.h"
#include "muxretro.h"
#include "core.h"
#include "settings.h"

static int active = 0;
static uint64_t prev_nav_mask = 0;

static uint32_t hold_delay_up = 0;
static uint32_t hold_tick_up = 0;
static uint32_t hold_delay_down = 0;
static uint32_t hold_tick_down = 0;

static uint64_t current_nav_mask(void) {
    const int up = mux_input_pressed(mux_input_dpad_up);
    const int down = mux_input_pressed(mux_input_dpad_down);
    const int back = mux_input_pressed(mux_input_b);

    return (up ? BIT(0) : 0) | (down ? BIT(1) : 0) | (back ? BIT(2) : 0);
}

static void format_content_hash(char *out, const size_t out_size) {
    FILE *f = fopen(core_content_path, "rb");
    if (!f) {
        snprintf(out, out_size, "%s", lang.generic.unknown);
        return;
    }

    mz_ulong crc = MZ_CRC32_INIT;
    unsigned char buf[65536];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        crc = mz_crc32(crc, buf, n);
    }

    fclose(f);

    snprintf(out, out_size, "%08lX", (unsigned long) crc);
}

static void build_info_row(const char *label, const char *value) {
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label_obj = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value_obj = lv_label_create(panel);

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label_obj, label, 1);
    apply_theme_list_glyph(&theme, icon, "muxretro", "info");
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

    struct retro_system_info info = {0};
    if (current_core.retro_get_system_info) current_core.retro_get_system_info(&info);

    struct retro_system_av_info av_info = {0};
    if (current_core.retro_get_system_av_info) current_core.retro_get_system_av_info(&av_info);

    const char *content_name = strrchr(core_content_path, '/');
    content_name = content_name ? content_name + 1 : core_content_path;

    char hash[16];
    format_content_hash(hash, sizeof(hash));

    char api_version[16];
    snprintf(api_version, sizeof(api_version), "%s", lang.generic.unknown);
    if (current_core.retro_api_version) {
        snprintf(api_version, sizeof(api_version), "%u", current_core.retro_api_version());
    }

    build_info_row(
        lang.muxretro.information_screen.core_name, info.library_name ? info.library_name : lang.generic.unknown
    );
    build_info_row(
        lang.muxretro.information_screen.core_version,
        info.library_version ? info.library_version : lang.generic.unknown
    );
    build_info_row(lang.muxretro.information_screen.libretro_api, api_version);
    build_info_row(lang.generic.content, content_name);
    build_info_row(lang.muxretro.information_screen.content_hash, hash);
    build_info_row(
        lang.muxretro.information_screen.loaded_via,
        core_content_load_method[0] ? core_content_load_method : lang.generic.unknown
    );

    int frame_w = 0, frame_h = 0;
    video_bridge_get_frame_size(&frame_w, &frame_h);
    char resolution[32];
    snprintf(resolution, sizeof(resolution), "%s", lang.generic.unknown);
    if (frame_w > 0 && frame_h > 0) snprintf(resolution, sizeof(resolution), "%dx%d", frame_w, frame_h);
    build_info_row(lang.muxretro.information_screen.resolution, resolution);

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
    build_info_row(lang.muxretro.information_screen.display_output, display_output);

    char fps[16];
    snprintf(fps, sizeof(fps), "%s", lang.generic.unknown);
    if (av_info.timing.fps > 0) snprintf(fps, sizeof(fps), "%.2f", av_info.timing.fps);
    build_info_row(lang.muxretro.information_screen.target_fps, fps);

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
    build_info_row(lang.muxretro.information_screen.audio_output, audio_output);

    build_info_row(
        lang.muxretro.information_screen.rumble_support,
        device.board.rumble[0] ? lang.generic.enabled : lang.generic.disabled
    );

    const int disc_count = mux_retro_disk_get_num_images();
    if (disc_count > 1) {
        char discs[8];
        snprintf(discs, sizeof(discs), "%d", disc_count);
        build_info_row(lang.muxretro.information_screen.disc_count, discs);
    }

    ui_count_static = disc_count > 1 ? 12 : 11;
    first_open = 0;
}

static void close_information(void) {
    active = 0;

    pause_menu_rebuild();
    pause_menu_focus_information_item();
    pause_menu_show_nav_hints();
    pause_menu_sync_input_mask();
}

void information_menu_open(void) {
    active = 1;
    prev_nav_mask = current_nav_mask();

    rebuild_rows();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0}, {ui_lbl_nav_b, lang.generic.back, 0}, {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
}

int information_menu_is_active(void) {
    return active;
}

void information_menu_tick(void) {
    const uint64_t mask = current_nav_mask();
    const uint64_t edge = mask & ~prev_nav_mask;
    prev_nav_mask = mask;

    const uint32_t now = SDL_GetTicks();
    int do_up = 0;
    int do_down = 0;

    if (edge & BIT(0)) {
        do_up = 1;
        hold_delay_up = (uint32_t) config.settings.advanced.repeat_delay;
        hold_tick_up = now;
    } else if ((mask & BIT(0)) && now - hold_tick_up >= hold_delay_up) {
        if (current_item_index > 0) do_up = 1;
        hold_delay_up = (uint32_t) config.settings.advanced.accelerate;
        hold_tick_up = now;
    }

    if (edge & BIT(1)) {
        do_down = 1;
        hold_delay_down = (uint32_t) config.settings.advanced.repeat_delay;
        hold_tick_down = now;
    } else if ((mask & BIT(1)) && now - hold_tick_down >= hold_delay_down) {
        if (current_item_index < ui_count_static - 1) do_down = 1;
        hold_delay_down = (uint32_t) config.settings.advanced.accelerate;
        hold_tick_down = now;
    }

    if (ui_count_static < 2) {
        do_up = 0;
        do_down = 0;
    }

    // See ui_pause.c's own gen_step_movement() call site for why this pairing
    // is needed on every muxretro screen - without it, this screen's row
    // shake (muxvisual.c's Selection Animation/Style) never plays.
    if (do_up) {
        nav_set_last_dir(nav_dir_up);
        nav_unsuppress_shake();
        gen_step_movement(1, -1, 1, 0, 1);
    } else if (do_down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 1, 0, 1);
    } else if (edge & BIT(2)) {
        play_sound(snd_back);
        close_information();
    }
}
