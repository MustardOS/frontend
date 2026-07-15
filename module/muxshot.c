#include "muxshare.h"

lv_obj_t *ui_img_screenshot;
static int is_fullscreen = 0;

static int remove_mode = 0;
static int skip_confirm = 0;
static mux_dialogue remove_dlg;

static void show_remove_dialog(void) {
    remove_mode = 1;
    remove_dlg.selected = 0;
    dialogue_show(&remove_dlg);
    dialogue_refresh(&remove_dlg, &theme);
}

static void hide_remove_dialog(void) {
    remove_mode = 0;
    dialogue_hide(&remove_dlg);
}

static void show_help(void) {
    show_info_box(TRS(lv_label_get_text(lv_group_get_focused(ui_group))), lang.muxshot.help, 0);
}

static void image_refresh(void) {
    // Invalidate the cache for this image path
    lv_img_cache_invalidate_src(lv_img_get_src(ui_img_screenshot));

    char shot_name[PATH_MAX];
    const int n_name =
        snprintf(shot_name, sizeof(shot_name), "%s.png", lv_label_get_text(lv_group_get_focused(ui_group)));
    if (n_name < 0 || (size_t) n_name >= sizeof(shot_name)) {
        lv_img_set_src(ui_img_screenshot, &ui_img_blank);
        return;
    }

    char screenshot_file[PATH_MAX];
    if (build_safe_path(screenshot_file, sizeof(screenshot_file), STORAGE_SHOTS, shot_name) != 0) {
        lv_img_set_src(ui_img_screenshot, &ui_img_blank);
        return;
    }

    char screenshot_image[PATH_MAX];
    const int n_img = snprintf(screenshot_image, sizeof(screenshot_image), "M:%s", screenshot_file);
    if (n_img < 0 || (size_t) n_img >= sizeof(screenshot_image)) {
        lv_img_set_src(ui_img_screenshot, &ui_img_blank);
        return;
    }

    lv_img_set_src(ui_img_screenshot, screenshot_image);
}

static void create_screenshot_items(void) {
    struct dirent *tf;
    regex_t regex;
    const char *pattern = "^muOS_[0-9]{8}_[0-9]{4}_[0-9]+\\.png$";

    if (regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB) != 0) return;

    DIR *td = opendir(STORAGE_SHOTS);
    if (!td) {
        regfree(&regex);
        return;
    }

    while ((tf = readdir(td))) {
        if (regexec(&regex, tf->d_name, 0, NULL, 0) == 0) {
            char *last_dot = strrchr(tf->d_name, '.');
            if (last_dot) *last_dot = '\0';

            add_item(&items, &item_count, tf->d_name, tf->d_name, "", ITEM);
        }
    }

    closedir(td);
    regfree(&regex);

    sort_items(items, item_count);

    reset_ui_groups();

    ui_count_static += (int) item_count;

    const size_t limit = theme.mux.item.count;
    for (size_t i = 0; i < item_count && i < limit; i++) {
        lv_obj_t *ui_pnl_screenshot = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_screenshot);

        lv_obj_t *ui_lbl_screenshot_item = lv_label_create(ui_pnl_screenshot);
        apply_theme_list_item(&theme, ui_lbl_screenshot_item, items[i].display_name);

        lv_obj_t *ui_lbl_screenshot_item_glyph = lv_img_create(ui_pnl_screenshot);
        apply_theme_list_glyph(&theme, ui_lbl_screenshot_item_glyph, mux_module, "screenshot");

        lv_group_add_obj(ui_group, ui_lbl_screenshot_item);
        lv_group_add_obj(ui_group_glyph, ui_lbl_screenshot_item_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_screenshot);

        apply_size_to_content(
            &theme, ui_pnl_content, ui_lbl_screenshot_item, ui_lbl_screenshot_item_glyph, items[i].display_name
        );
        apply_text_long_dot(&theme, ui_lbl_screenshot_item);
    }

    if (ui_count_static > 0) lv_obj_update_layout(ui_pnl_content);
}

static void update_list_item(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, const int index) {
    lv_label_set_text(ui_lbl_item, items[index].display_name);
    apply_theme_list_glyph(&theme, ui_lbl_item_glyph, mux_module, "screenshot");

    apply_size_to_content(&theme, ui_pnl_content, ui_lbl_item, ui_lbl_item_glyph, items[index].display_name);
    apply_text_long_dot(&theme, ui_lbl_item);
}

static void list_nav_prev(const int steps) {
    list_win_nav_move(steps, -1, update_list_item);
}

static void list_nav_next(const int steps) {
    list_win_nav_move(steps, +1, update_list_item);
}

static void do_remove(void) {
    char shot_name[PATH_MAX];
    const int n_name =
        snprintf(shot_name, sizeof(shot_name), "%s.png", lv_label_get_text(lv_group_get_focused(ui_group)));

    char screenshot_file[PATH_MAX];
    if (n_name < 0 || (size_t) n_name >= sizeof(shot_name)
        || build_safe_path(screenshot_file, sizeof(screenshot_file), STORAGE_SHOTS, shot_name) != 0) {
        play_sound(snd_error);
        toast_message(lang.generic.remove_fail, tst_wait_m);
        return;
    }

    write_text_to_file(MUOS_IDX_LOAD, "w", INT, get_index_on_delete(current_item_index, ui_count_static - 1));
    remove(screenshot_file);

    play_sound(snd_muos);
    load_mux("screenshot");

    mux_input_stop();
}

static void handle_dpad_up(void) {
    if (remove_mode) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (remove_mode) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (remove_mode) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (remove_mode) return;

    handle_list_nav_down_hold();
}

static void handle_a(void) {
    if (msgbox_active || !ui_count_static || hold_call) return;

    if (remove_mode) {
        const mux_remove_opt opt = (mux_remove_opt) remove_dlg.selected;
        hide_remove_dialog();
        if (opt == mux_remove_yep) {
            do_remove();
        } else if (opt == mux_remove_skip) {
            skip_confirm = 1;
            do_remove();
        }
        return;
    }

    play_sound(snd_confirm);

    if (is_fullscreen) {
        is_fullscreen = 0;
        lv_obj_set_style_img_opa(ui_img_screenshot, 64, MU_OBJ_MAIN_DEFAULT);
        lv_obj_move_to_index(ui_img_screenshot, (int16_t) (lv_obj_get_index(ui_pnl_wall) + 1));
        lv_obj_clear_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnl_box, LV_OBJ_FLAG_HIDDEN);
    } else {
        is_fullscreen = 1;
        lv_obj_set_style_img_opa(ui_img_screenshot, 255, MU_OBJ_MAIN_DEFAULT);
        lv_obj_move_foreground(ui_img_screenshot);
        lv_obj_add_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnl_box, LV_OBJ_FLAG_HIDDEN);
    }
}

static void handle_b(void) {
    if (hold_call) return;

    if (remove_mode) {
        hide_remove_dialog();
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "screenshot");

    skip_confirm = 0;
    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || is_fullscreen || !ui_count_static || remove_mode) return;

    if (config.settings.advanced.trust_remove || skip_confirm) {
        do_remove();
        return;
    }

    play_sound(snd_confirm);
    show_remove_dialog();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || is_fullscreen || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 1},
                                  {ui_lbl_nav_a, lang.generic.select, 1},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 1},
                                  {ui_lbl_nav_x, lang.generic.remove, 1},
                                  {NULL, NULL, 0}});

    ui_img_screenshot = lv_img_create(ui_screen);

    lv_img_set_src(ui_img_screenshot, &ui_img_blank);
    lv_obj_set_width(ui_img_screenshot, device.mux.width);
    lv_obj_set_height(ui_img_screenshot, device.mux.height);
    lv_obj_set_align(ui_img_screenshot, LV_ALIGN_CENTER);

    lv_obj_set_style_img_opa(ui_img_screenshot, 64, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_img_screenshot, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_img_screenshot, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_shadow_width(ui_img_screenshot, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(ui_img_screenshot, 0, MU_OBJ_MAIN_DEFAULT);

    mu_img_no_shadow(ui_img_screenshot);
    lv_obj_move_to_index(ui_img_screenshot, (int16_t) (lv_obj_get_index(ui_pnl_wall) + 1));

    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    if (ui_count_static > 0 && nav_moved) {
        image_refresh();
        adjust_gen_panel();

        lv_obj_invalidate(ui_pnl_box);
        nav_moved = 0;
    }
}

int muxshot_main(void) {
    is_fullscreen = 0;

    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxshot.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    create_screenshot_items();
    init_elements();

    int sys_index = 0;
    if (file_exist(MUOS_IDX_LOAD)) {
        sys_index = read_line_int_from(MUOS_IDX_LOAD, 1);
        remove(MUOS_IDX_LOAD);
    }

    int nav_hidden = 0;
    if (ui_count_static > 0) {
        nav_hidden = 1;
    } else {
        lv_label_set_text(ui_lbl_screen_message, lang.muxshot.none);
    }

    const struct nav_flag nav_e[] = {
        {ui_lbl_nav_a, nav_hidden},
        {ui_lbl_nav_a_glyph, nav_hidden},
        {ui_lbl_nav_x, nav_hidden},
        {ui_lbl_nav_x_glyph, nav_hidden}
    };
    set_nav_flags(nav_e, A_SIZE(nav_e));

    if (ui_count_static > 0 && sys_index <= ui_count_static && current_item_index < ui_count_static) {
        current_item_index = sys_index;
        list_win_focus_initial(update_list_item);
    }

    dialogue_init_remove(&remove_dlg, &theme, ui_screen, NULL, lang.generic.select, lang.generic.cancel);
    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    free_items(&items, &item_count);

    return 0;
}
