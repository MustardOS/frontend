#include "../common/audio.h"
#include "../common/config.h"
#include "../common/datetime.h"
#include "../common/display.h"
#include "../common/init.h"
#include "../common/input.h"
#include "../common/log.h"
#include "../common/ui/common.h"
#include "../module/muxshare.h"
#include "gamestate.h"
#include "muxretro.h"
#include "core.h"
#include "settings.h"

static int active = 0;

static uint64_t prev_nav_mask = 0;
static lv_obj_t *dim_overlay = NULL;
static lv_obj_t *ui_lbl_fps = NULL;

static int has_disc_control = 0;
static int row_resume;
static int row_game_state;
static int row_options;
static int row_disc_control;
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
    row_settings = i++;
    row_information = i++;
    row_restart = i++;
    row_quit = i++;
    row_count = i;
}

static uint32_t hold_delay_up = 0;
static uint32_t hold_tick_up = 0;
static uint32_t hold_delay_down = 0;
static uint32_t hold_tick_down = 0;

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

static void create_fps_label(void) {
    ui_lbl_fps = lv_label_create(ui_screen);
    lv_obj_set_style_text_color(ui_lbl_fps, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lbl_fps, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lbl_fps, 140, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(ui_lbl_fps, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(ui_lbl_fps, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text(ui_lbl_fps, "");
    lv_obj_align(ui_lbl_fps, LV_ALIGN_TOP_LEFT, 4, 4);

    lv_obj_move_foreground(ui_lbl_fps);

    if (!session_settings.show_fps) lv_obj_add_flag(ui_lbl_fps, LV_OBJ_FLAG_HIDDEN);
}

void pause_menu_set_fps_visible(const int visible) {
    if (!ui_lbl_fps) return;
    if (visible) {
        lv_obj_clear_flag(ui_lbl_fps, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ui_lbl_fps, LV_OBJ_FLAG_HIDDEN);
    }
}

void pause_menu_set_fps_text(const char *text) {
    if (!ui_lbl_fps) return;
    lv_label_set_text(ui_lbl_fps, text);
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

        if (ui_lbl_fps) lv_obj_add_flag(ui_lbl_fps, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(dim_overlay, LV_OBJ_FLAG_HIDDEN);

        pause_menu_set_fps_visible(session_settings.show_fps);
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

    setup_nav(
        (struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0}, {ui_lbl_nav_b, lang.muxretro.resume, 0}, {NULL, NULL, 0}}
    );
    pause_menu_fix_nav_order();
}

void pause_menu_init(void) {
    init_ui_common_screen(&theme, &device, &lang, lang.muxretro.title);
    set_gradient_visible(0);

    init_fonts();
    if (init_audio_backend()) init_fe_snd(&fe_snd, config.settings.general.sound, 0);

    create_dim_overlay();
    create_fps_label();
    gamestate_menu_init();
    settings_menu_init();
    options_menu_init();

    pause_menu_rebuild();

    header_and_footer_setup();
    pause_menu_show_nav_hints();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    set_chrome_visible(0);
    fade_in_screen();
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
            return 1;
        }
    }

    return 0;
}
