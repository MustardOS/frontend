#include "muxshare.h"
#include "ui/ui_muxthemefilter.h"

#define THEMEFILTER(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(THEMEFILTER_ELEMENTS) + 1 };
#undef THEMEFILTER

static char lookup_original_value[MAX_BUFFER_SIZE];

#define THEMEFILTER(NAME, UDATA) static int NAME##_original;
THEMEFILTER_ELEMENTS
#undef THEMEFILTER

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define THEMEFILTER(NAME, UDATA) {UDATA, lang.muxthemefilter.help.NAME},
        THEMEFILTER_ELEMENTS
#undef THEMEFILTER
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define THEMEFILTER(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_themefilter);
    THEMEFILTER_ELEMENTS
#undef THEMEFILTER
}

static void restore_theme_filter_options(void) {
#define THEMEFILTER(NAME, UDATA) lv_dropdown_set_selected(ui_dro_##NAME##_themefilter, config.theme.filter.NAME);
    THEMEFILTER_ELEMENTS
#undef THEMEFILTER
}

static void save_theme_filter_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(themefilter, all_themes, "theme/filter/allthemes", INT, 0);
    CHECK_AND_SAVE_STD(themefilter, grid, "theme/filter/grid", INT, 0);
    CHECK_AND_SAVE_STD(themefilter, hdmi, "theme/filter/hdmi", INT, 0);
    CHECK_AND_SAVE_STD(themefilter, language, "theme/filter/language", INT, 0);

    if (strcmp(lookup_original_value, lv_label_get_text(ui_val_lookup_themefilter)) != 0) {
        is_modified++;
        write_text_to_file(
            CONF_CONFIG_PATH "theme/filter/lookup", "w", CHAR, lv_label_get_text(ui_val_lookup_themefilter)
        );
    }

    if (is_modified > 0) {
        toast_message(lang.generic.saving, tst_wait_f);
        refresh_config = 1;
        theme_down_index = 0;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *theme_compat_opt[] = {
        lang.muxthemefilter.compat.device,
        lang.muxthemefilter.compat.all,
    };

    INIT_OPTION_ITEM(-1, themefilter, all_themes, lang.muxthemefilter.all_themes, "theme", theme_compat_opt, 2);
    INIT_OPTION_ITEM(-1, themefilter, grid, lang.muxthemefilter.grid, "grid", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, themefilter, hdmi, lang.muxthemefilter.hdmi, "hdmi", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, themefilter, language, lang.muxthemefilter.language, "language", disabled_enabled, 2);

    INIT_VALUE_ITEM(-1, themefilter, lookup, lang.muxthemefilter.lookup, "lookup", config.theme.filter.lookup);

    snprintf(lookup_original_value, sizeof(lookup_original_value), "%s", config.theme.filter.lookup);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
}

static void check_focus(void) {
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    if (e_focused == ui_lbl_lookup_themefilter) {
        lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 0, 0, 1);
    check_focus();
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    if (lv_group_get_focused(ui_group) != ui_lbl_lookup_themefilter)
        move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_keyboard_ok_press(void) {
    key_show = 0;
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    if (e_focused == ui_lbl_lookup_themefilter) {
        lv_label_set_text(ui_val_lookup_themefilter, lv_textarea_get_text(ui_txt_entry_themefilter));
    }

    reset_osk(key_entry);

    lv_textarea_set_text(ui_txt_entry_themefilter, "");
    lv_group_set_focus_cb(ui_group, NULL);
    osk_hide(ui_pnl_entry_themefilter);
}

static void handle_keyboard_press(void) {
    first_open ? (first_open = 0) : play_sound(snd_keypress);

    const char *is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    if (is_key && strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_ok_press();
    } else {
        lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
    }
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    if (lv_group_get_focused(ui_group) != ui_lbl_lookup_themefilter)
        move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_confirm(void) {
    play_sound(snd_confirm);

    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    if (e_focused == ui_lbl_lookup_themefilter) {
        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

        key_show = 1;

        osk_show(ui_pnl_entry_themefilter);

        lv_textarea_set_text(ui_txt_entry_themefilter, lv_label_get_text(lv_group_get_focused(ui_group_value)));
    } else {
        handle_option_next();
    }
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    key_show ? handle_keyboard_press() : handle_confirm();
}

static void handle_x(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) close_osk(key_entry, ui_group, ui_txt_entry_themefilter, ui_pnl_entry_themefilter);
}

static void handle_y(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) key_space(ui_txt_entry_themefilter);
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (key_show) {
        key_backspace(ui_txt_entry_themefilter);
        return;
    }

    play_sound(snd_back);

    save_theme_filter_options();

    load_mux("themedwn");

    mux_input_stop();
}

static void handle_b_hold(void) {
    if (key_show) key_backspace(ui_txt_entry_themefilter);
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || key_show || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void handle_up(void) {
    key_show ? key_up() : handle_list_nav_up();
}

static void handle_up_hold(void) {
    key_show ? key_up() : handle_list_nav_up_hold();
}

static void handle_down(void) {
    key_show ? key_down() : handle_list_nav_down();
}

static void handle_down_hold(void) {
    key_show ? key_down() : handle_list_nav_down_hold();
}

static void handle_left(void) {
    key_show ? key_left() : handle_option_prev();
}

static void handle_right(void) {
    key_show ? key_right() : handle_option_next();
}

static void handle_left_hold(void) {
    key_show ? key_left() : handle_option_prev();
}

static void handle_right_hold(void) {
    key_show ? key_right() : handle_option_next();
}

static void handle_l1(void) {
    if (key_show) {
        key_swap_back();
        return;
    }

    handle_list_nav_page_up();
}

static void handle_r1(void) {
    if (key_show) {
        key_swap();
        return;
    }

    handle_list_nav_page_down();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.save, 0},
                                  {NULL, NULL, 0}});

    check_focus();

#define THEMEFILTER(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_themefilter, UDATA);
    THEMEFILTER_ELEMENTS
#undef THEMEFILTER

    overlay_display();
}

int muxthemefilter_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxthemefilter.title);
    init_muxthemefilter(ui_screen, ui_pnl_content, &theme);

    init_elements();
    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());
    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    restore_theme_filter_options();
    init_dropdown_settings();

    init_osk(ui_pnl_entry_themefilter, ui_txt_entry_themefilter, 0, 0, OSK_MAX);

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_dpad_up] = handle_up,
                [mux_input_dpad_down] = handle_down,
                [mux_input_dpad_left] = handle_left,
                [mux_input_dpad_right] = handle_right,
                [mux_input_l1] = handle_l1,
                [mux_input_r1] = handle_r1,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_b] = handle_b_hold,
            [mux_input_dpad_up] = handle_up_hold,
            [mux_input_dpad_down] = handle_down_hold,
            [mux_input_dpad_left] = handle_left_hold,
            [mux_input_dpad_right] = handle_right_hold,
            [mux_input_l1] = handle_l1,
            [mux_input_r1] = handle_r1,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
