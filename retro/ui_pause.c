#include "../common/audio.h"
#include "../common/battery.h"
#include "../common/config.h"
#include "../common/datetime.h"
#include "../common/display.h"
#include "../common/init.h"
#include "../common/input.h"
#include "../common/log.h"
#include "../common/theme.h"
#include "../common/ui/common.h"
#include "../common/ui/glyph.h"
#include "../module/muxshare.h"
#include "gamestate.h"
#include "hotkeys.h"
#include "muxretro.h"
#include "core.h"
#include "nav_repeat.h"
#include "settings.h"

#define TOAST_DURATION_MS 2048
#define HEADER_FADE_MS    256

static uint32_t toast_expire_tick = 0;
static int toast_active = 0;

static uint32_t header_fade_start_tick = 0;
static int header_fading = 0;
static int header_fade_played = 0;

static int active = 0;

static uint64_t prev_nav_mask = 0;
static lv_obj_t *dim_overlay = NULL;
static lv_obj_t *ui_lbl_fps = NULL;
static lv_obj_t *ui_img_fps_glyph = NULL;
static lv_obj_t *ui_lbl_speed_mode = NULL;
static lv_obj_t *ui_img_speed_glyph = NULL;

static int has_disc_control = 0;
static int row_resume;
static int row_game_state;
static int row_options;
static int row_disc_control;
static int row_cheats;
static int row_settings;
static int row_information;
static int row_restart;
static int row_quit;
static int row_count;

static void compute_row_indices(void) {
    has_disc_control = mux_retro_disk_get_num_images() > 1;

    int i = 0;
    row_resume = i++;
    row_game_state = i++;
    row_options = i++;
    row_disc_control = has_disc_control ? i++ : -1;
    row_cheats = i++;
    row_settings = i++;
    row_information = i++;
    row_restart = i++;
    row_quit = i++;
    row_count = i;
}

static nav_repeat_t rpt_up = {0};
static nav_repeat_t rpt_down = {0};

static uint64_t current_nav_mask(void) {
    const int up = mux_input_pressed(mux_input_dpad_up);
    const int down = mux_input_pressed(mux_input_dpad_down);
    const int confirm = mux_input_pressed(mux_input_a);
    const int back = mux_input_pressed(mux_input_b);

    return (up ? BIT(0) : 0) | (down ? BIT(1) : 0) | (confirm ? BIT(2) : 0) | (back ? BIT(3) : 0);
}

void pause_menu_sync_input_mask(void) {
    prev_nav_mask = current_nav_mask();
}

static void create_dim_overlay(void) {
    dim_overlay = lv_obj_create(ui_screen);
    lv_obj_remove_style_all(dim_overlay);
    lv_obj_set_size(dim_overlay, device.mux.width, device.mux.height);
    lv_obj_center(dim_overlay);
    lv_obj_clear_flag(dim_overlay, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(dim_overlay, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(dim_overlay, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(dim_overlay, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dim_overlay, 200, MU_OBJ_MAIN_DEFAULT);

    lv_obj_move_background(dim_overlay);
}

static void set_corner_glyph(lv_obj_t *img, const char *glyph_name) {
    char embed[MAX_BUFFER_SIZE];
    if (get_glyph_path("muxretro", glyph_name, embed, sizeof(embed))) {
        set_list_glyph_image(img, embed);

        lv_obj_set_style_img_opa(img, (lv_opa_t) theme.list_default.glyph_alpha, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_img_recolor(img, lv_color_hex(theme.list_default.glyph_recolour), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_img_recolor_opa(img, (lv_opa_t) theme.list_default.glyph_recolour_alpha, MU_OBJ_MAIN_DEFAULT);

        lv_obj_clear_flag(img, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *create_corner_indicator(const lv_align_t align, const lv_coord_t x_ofs, lv_obj_t **out_glyph) {
    lv_obj_t *panel = lv_obj_create(ui_screen);
    lv_obj_set_size(panel, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(panel, 140, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(panel, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(panel, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(panel, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(panel, align, x_ofs, -4);

    lv_obj_move_foreground(panel);

    *out_glyph = lv_img_create(panel);

    return panel;
}

static void create_fps_label(void) {
    lv_obj_t *ui_pnl_fps = create_corner_indicator(LV_ALIGN_BOTTOM_LEFT, 4, &ui_img_fps_glyph);
    set_corner_glyph(ui_img_fps_glyph, "fps");

    ui_lbl_fps = lv_label_create(ui_pnl_fps);
    lv_obj_set_style_text_color(ui_lbl_fps, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(ui_lbl_fps, "");

    if (!session_settings.show_fps) lv_obj_add_flag(lv_obj_get_parent(ui_lbl_fps), LV_OBJ_FLAG_HIDDEN);
}

void pause_menu_set_fps_visible(const int visible) {
    if (!ui_lbl_fps) return;
    lv_obj_t *panel = lv_obj_get_parent(ui_lbl_fps);
    if (visible) {
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
    }
}

void pause_menu_set_fps_text(const char *text) {
    if (!ui_lbl_fps) return;
    lv_label_set_text(ui_lbl_fps, text);
}

static void create_speed_mode_label(void) {
    lv_obj_t *ui_pnl_speed_mode = create_corner_indicator(LV_ALIGN_BOTTOM_RIGHT, -4, &ui_img_speed_glyph);

    ui_lbl_speed_mode = lv_label_create(ui_pnl_speed_mode);
    lv_obj_set_style_text_color(ui_lbl_speed_mode, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(ui_lbl_speed_mode, "");

    lv_obj_add_flag(ui_pnl_speed_mode, LV_OBJ_FLAG_HIDDEN);
}

void pause_menu_set_speed_indicator(const char *text, const char *glyph) {
    if (!ui_lbl_speed_mode) return;
    lv_obj_t *panel = lv_obj_get_parent(ui_lbl_speed_mode);

    if (!text || !text[0]) {
        lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (glyph) set_corner_glyph(ui_img_speed_glyph, glyph);

    lv_label_set_text(ui_lbl_speed_mode, text);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

void pause_menu_show_toast_timed(const char *msg, const uint32_t duration_ms) {
    if (!ui_pnl_message || !ui_lbl_message) return;

    lv_label_set_text(ui_lbl_message, msg);
    lv_obj_clear_flag(ui_pnl_message, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(ui_pnl_message, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_move_foreground(ui_pnl_message);

    lv_obj_mark_layout_as_dirty(ui_pnl_message);
    lv_obj_update_layout(ui_pnl_message);

    toast_expire_tick = SDL_GetTicks() + duration_ms;
    toast_active = 1;
}

void pause_menu_show_toast(const char *msg) {
    pause_menu_show_toast_timed(msg, TOAST_DURATION_MS);
}

void pause_menu_toast_tick(void) {
    if (!toast_active) return;
    if (SDL_GetTicks() < toast_expire_tick) return;

    lv_obj_add_flag(ui_pnl_message, LV_OBJ_FLAG_HIDDEN);
    toast_active = 0;
}

static void apply_gameplay_header_overlay(void) {
    if (session_settings.header_visibility == header_visibility_none) {
        lv_obj_add_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
        header_fading = 0;
        return;
    }

    lv_obj_clear_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(ui_pnl_header, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_add_flag(ui_lbl_title, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_sta_bluetooth, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_sta_network, LV_OBJ_FLAG_HIDDEN);

    const int show_clock = session_settings.header_visibility == header_visibility_clock
                           || session_settings.header_visibility == header_visibility_both;
    const int show_battery = session_settings.header_visibility == header_visibility_battery
                             || session_settings.header_visibility == header_visibility_both;

    if (show_clock) {
        lv_obj_clear_flag(ui_lbl_datetime, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ui_lbl_datetime, LV_OBJ_FLAG_HIDDEN);
    }

    if (show_battery) {
        if (config.visual.battery == 1) {
            lv_obj_add_flag(ui_sta_capacity, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_lbl_battery_percent, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(ui_sta_capacity, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_lbl_battery_percent, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_add_flag(ui_sta_capacity, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lbl_battery_percent, LV_OBJ_FLAG_HIDDEN);
    }

    datetime_task(NULL);
    battery_capacity_task(NULL);

    if (header_fade_played) {
        lv_obj_set_style_opa(ui_pnl_header, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
        return;
    }
    header_fade_played = 1;

    header_fade_start_tick = SDL_GetTicks();
    header_fading = 1;
    lv_obj_set_style_opa(ui_pnl_header, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
}

void pause_menu_header_fade_tick(void) {
    if (!header_fading) return;

    const uint32_t elapsed = SDL_GetTicks() - header_fade_start_tick;
    if (elapsed >= HEADER_FADE_MS) {
        lv_obj_set_style_opa(ui_pnl_header, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
        header_fading = 0;
        return;
    }

    lv_obj_set_style_opa(ui_pnl_header, (lv_opa_t) (255 * elapsed / HEADER_FADE_MS), MU_OBJ_MAIN_DEFAULT);
}

static void restore_header_chrome(void) {
    header_fading = 0;
    lv_obj_set_style_opa(ui_pnl_header, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_header, theme.header.background_alpha, MU_OBJ_MAIN_DEFAULT);

    lv_obj_clear_flag(ui_lbl_title, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_sta_bluetooth, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_sta_network, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_lbl_datetime, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_sta_capacity, LV_OBJ_FLAG_HIDDEN);

    process_visual_element(vis_headertitle, ui_lbl_title);
    process_visual_element(vis_bluetooth, ui_sta_bluetooth);
    process_visual_element(vis_network, ui_sta_network);
    process_visual_element(vis_clock, ui_lbl_datetime);
    process_visual_element(vis_battery, ui_sta_capacity);

    lv_label_set_text(ui_lbl_datetime, get_datetime());
    update_battery_capacity(ui_sta_capacity, &theme);
    update_battery_percent_label(ui_lbl_battery_percent, &theme);
}

void pause_menu_update_header(void) {
    if (session_settings.header_visibility == header_visibility_none) return;
    datetime_task(NULL);
    battery_capacity_task(NULL);
}

void pause_menu_apply_header_visibility(void) {
    if (active) return;
    apply_gameplay_header_overlay();
}

void pause_menu_rebuild(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;

    compute_row_indices();

    gen_label("muxretro", "resume", lang.muxretro.resume);
    gen_label("muxretro", "state", lang.muxretro.game_state);
    gen_label("muxretro", "core", lang.muxretro.core_options);
    if (has_disc_control) gen_label("muxretro", "disc", lang.muxretro.disc_control);
    gen_label("muxretro", "cheat", lang.muxretro.cheats);
    gen_label("muxretro", "settings", lang.muxretro.settings);
    gen_label("muxretro", "info", lang.muxretro.information);
    gen_label("muxretro", "restart", lang.muxretro.restart);
    gen_label("muxretro", "exit", lang.muxretro.quit);

    ui_count_static = row_count;
    first_open = 0;
}

static void set_chrome_visible(const int visible) {
    if (visible) {
        lv_obj_clear_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(dim_overlay, LV_OBJ_FLAG_HIDDEN);

        if (ui_lbl_fps) lv_obj_add_flag(lv_obj_get_parent(ui_lbl_fps), LV_OBJ_FLAG_HIDDEN);
        pause_menu_set_speed_indicator(NULL, NULL);
        restore_header_chrome();
    } else {
        lv_obj_add_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(dim_overlay, LV_OBJ_FLAG_HIDDEN);

        pause_menu_set_fps_visible(session_settings.show_fps);
        apply_gameplay_header_overlay();
    }
}

static void focus_item(const int index) {
    if (index >= ui_count_static) return;
    current_item_index = index;

    lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, index);
    if (!panel) return;

    lv_obj_t *label = lv_obj_get_child(panel, 0);
    lv_obj_t *glyph = lv_obj_get_child(panel, 1);

    nav_suppress_next_shake();

    if (label) lv_group_focus_obj(label);
    if (glyph) lv_group_focus_obj(glyph);
    lv_group_focus_obj(panel);

    update_scroll_position(
        theme.mux.item.count, theme.mux.item.panel, ui_count_static, current_item_index, ui_pnl_content
    );
}

void pause_menu_focus_options_item(void) {
    focus_item(row_options);
}

void pause_menu_focus_gamestate_item(void) {
    focus_item(row_game_state);
}

void pause_menu_focus_diskcontrol_item(void) {
    focus_item(row_disc_control);
}

void pause_menu_focus_cheats_item(void) {
    focus_item(row_cheats);
}

void pause_menu_focus_settings_item(void) {
    focus_item(row_settings);
}

void pause_menu_focus_information_item(void) {
    focus_item(row_information);
}

void pause_menu_fix_nav_order(void) {
    lv_obj_move_foreground(ui_lbl_nav_lr_glyph);
    lv_obj_move_foreground(ui_lbl_nav_lr);
    lv_obj_move_foreground(ui_lbl_nav_a_glyph);
    lv_obj_move_foreground(ui_lbl_nav_a);
    lv_obj_move_foreground(ui_lbl_nav_b_glyph);
    lv_obj_move_foreground(ui_lbl_nav_b);
    lv_obj_move_foreground(ui_lbl_nav_x_glyph);
    lv_obj_move_foreground(ui_lbl_nav_x);
    lv_obj_move_foreground(ui_lbl_nav_y_glyph);
    lv_obj_move_foreground(ui_lbl_nav_y);
}

void pause_menu_show_nav_hints(void) {
    nav_show_lr(0);

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.muxretro.resume, 0},
                                  {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
}

void pause_menu_init(void) {
    init_ui_common_screen(&theme, &device, &lang, lang.muxretro.title);
    set_gradient_visible(0);

    init_fonts();
    if (init_audio_backend()) init_fe_snd(&fe_snd, config.settings.general.sound, 0);

    create_dim_overlay();
    create_fps_label();
    create_speed_mode_label();
    gamestate_menu_init();
    settings_menu_init();
    hotkeys_menu_init();
    cheats_menu_init();
    display_menu_init();
    options_menu_init();

    pause_menu_rebuild();

    header_and_footer_setup();
    pause_menu_show_nav_hints();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    set_chrome_visible(0);
    fade_in_instant();
}

void pause_menu_shutdown(void) {
    if (ui_screen_container) lv_obj_del(ui_screen_container);
}

int pause_menu_is_active(void) {
    return active;
}

void pause_menu_toggle(void) {
    active = !active;
    LOG_DEBUG(mux_module, "pause_menu_toggle: active=%d", active);
    pause_menu_sync_input_mask();

    if (active) {
        lv_refr_now(NULL);
        display_composite_frame();
        gamestate_capture_pending();
        hotkeys_reset();
    } else {
        input_bridge_suppress_held();
    }

    set_chrome_visible(active);

    if (active) {
        pause_menu_rebuild();
        lv_label_set_text(ui_lbl_datetime, get_datetime());
    }
}

int pause_menu_tick(void) {
    if (!active) return 0;

    if (options_menu_is_active()) {
        options_menu_tick();
        return 0;
    }

    if (gamestate_menu_is_active()) {
        gamestate_menu_tick();
        return 0;
    }

    if (diskcontrol_menu_is_active()) {
        diskcontrol_menu_tick();
        return 0;
    }

    if (cheats_menu_is_active()) {
        cheats_menu_tick();
        return 0;
    }

    if (settings_menu_is_active()) {
        settings_menu_tick();
        return 0;
    }

    if (information_menu_is_active()) {
        information_menu_tick();
        return 0;
    }

    const uint64_t mask = current_nav_mask();
    const uint64_t edge = mask & ~prev_nav_mask;
    prev_nav_mask = mask;

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
        gen_step_movement(1, -1, 1, 0, 1);
    } else if (do_down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 1, 0, 1);
    } else if (edge & BIT(3)) {
        play_sound(snd_back);
        pause_menu_toggle();
    } else if (edge & BIT(2)) {
        if (current_item_index == row_resume) {
            pause_menu_toggle();
        } else if (current_item_index == row_game_state) {
            play_sound(snd_confirm);
            gamestate_menu_open();
        } else if (current_item_index == row_options) {
            play_sound(snd_confirm);
            options_menu_open();
        } else if (has_disc_control && current_item_index == row_disc_control) {
            play_sound(snd_confirm);
            diskcontrol_menu_open();
        } else if (current_item_index == row_cheats) {
            play_sound(snd_confirm);
            cheats_menu_open();
        } else if (current_item_index == row_settings) {
            play_sound(snd_confirm);
            settings_menu_open();
        } else if (current_item_index == row_information) {
            play_sound(snd_confirm);
            information_menu_open();
        } else if (current_item_index == row_restart) {
            play_sound(snd_confirm);
            if (current_core.retro_reset) current_core.retro_reset();
            pause_menu_toggle();
        } else if (current_item_index == row_quit) {
            play_sound(snd_confirm);
            if (session_settings_auto_save_on_quit()) gamestate_autosave_save();
            return 1;
        }
    }

    return 0;
}
