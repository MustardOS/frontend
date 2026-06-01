#include "muxshare.h"

lv_obj_t *ui_imgScreenshot;
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
    show_info_box(TRS(lv_label_get_text(lv_group_get_focused(ui_group))), lang.MUXSHOT.HELP, 0);
}

static void image_refresh(void) {
    // Invalidate the cache for this image path
    lv_img_cache_invalidate_src(lv_img_get_src(ui_imgScreenshot));

    char shot_name[PATH_MAX];
    int n_name = snprintf(shot_name, sizeof(shot_name), "%s.png",
                          lv_label_get_text(lv_group_get_focused(ui_group)));
    if (n_name < 0 || (size_t) n_name >= sizeof(shot_name)) {
        lv_img_set_src(ui_imgScreenshot, &ui_img_blank);
        return;
    }

    char screenshot_file[PATH_MAX];
    if (build_safe_path(screenshot_file, sizeof(screenshot_file), STORAGE_SHOTS, shot_name) != 0) {
        lv_img_set_src(ui_imgScreenshot, &ui_img_blank);
        return;
    }

    char screenshot_image[PATH_MAX];
    int n_img = snprintf(screenshot_image, sizeof(screenshot_image), "M:%s", screenshot_file);
    if (n_img < 0 || (size_t) n_img >= sizeof(screenshot_image)) {
        lv_img_set_src(ui_imgScreenshot, &ui_img_blank);
        return;
    }

    lv_img_set_src(ui_imgScreenshot, screenshot_image);
}

static void create_screenshot_items(void) {
    DIR *td;
    struct dirent *tf;
    regex_t regex;
    const char *pattern = "^muOS_[0-9]{8}_[0-9]{4}_[0-9]+\\.png$";

    if (regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB) != 0) return;

    td = opendir(STORAGE_SHOTS);
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

    for (size_t i = 0; i < item_count; i++) {
        ui_count++;

        lv_obj_t *ui_pnlScreenshot = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlScreenshot);

        lv_obj_t *ui_lblScreenshotItem = lv_label_create(ui_pnlScreenshot);
        apply_theme_list_item(&theme, ui_lblScreenshotItem, items[i].display_name);

        lv_obj_t *ui_lblScreenshotItemGlyph = lv_img_create(ui_pnlScreenshot);
        apply_theme_list_glyph(&theme, ui_lblScreenshotItemGlyph, mux_module, "screenshot");

        lv_group_add_obj(ui_group, ui_lblScreenshotItem);
        lv_group_add_obj(ui_group_glyph, ui_lblScreenshotItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlScreenshot);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblScreenshotItem, ui_lblScreenshotItemGlyph, items[i].display_name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblScreenshotItem);
    }

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
}


static void do_remove(void) {
    char shot_name[PATH_MAX];
    int n_name = snprintf(shot_name, sizeof(shot_name), "%s.png", lv_label_get_text(lv_group_get_focused(ui_group)));

    char screenshot_file[PATH_MAX];
    if (n_name < 0 || (size_t) n_name >= sizeof(shot_name) ||
        build_safe_path(screenshot_file, sizeof(screenshot_file), STORAGE_SHOTS, shot_name) != 0) {
        play_sound(SND_ERROR);
        toast_message(lang.GENERIC.REMOVE_FAIL, MEDIUM);
        return;
    }

    write_text_to_file(MUOS_IDX_LOAD, "w", INT, get_index_on_delete(current_item_index, ui_count - 1));
    remove(screenshot_file);

    play_sound(SND_MUOS);
    load_mux("screenshot");

    mux_input_stop();
}

static void handle_dpad_up(void) {
    if (remove_mode) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (remove_mode) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
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
    if (msgbox_active || !ui_count || hold_call) return;

    if (remove_mode) {
        mux_remove_opt opt = (mux_remove_opt) remove_dlg.selected;
        hide_remove_dialog();
        if (opt == MUX_REMOVE_YEP) {
            do_remove();
        } else if (opt == MUX_REMOVE_SKIP) {
            skip_confirm = 1;
            do_remove();
        }
        return;
    }

    play_sound(SND_CONFIRM);

    if (is_fullscreen) {
        is_fullscreen = 0;
        lv_obj_set_style_img_opa(ui_imgScreenshot, 25, MU_OBJ_MAIN_DEFAULT);
        lv_obj_clear_flag(ui_pnlHeader, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnlFooter, LV_OBJ_FLAG_HIDDEN);
    } else {
        is_fullscreen = 1;
        lv_obj_set_style_img_opa(ui_imgScreenshot, 255, MU_OBJ_MAIN_DEFAULT);
        lv_obj_add_flag(ui_pnlHeader, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlFooter, LV_OBJ_FLAG_HIDDEN);
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

    play_sound(SND_BACK);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "screenshot");

    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || is_fullscreen || !ui_count || remove_mode) return;

    if (config.SETTINGS.ADVANCED.TRUSTREMOVE || skip_confirm) {
        do_remove();
        return;
    }

    play_sound(SND_CONFIRM);
    show_remove_dialog();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || is_fullscreen || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  1},
            {ui_lblNavA,      lang.GENERIC.SELECT, 1},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph, "",                  1},
            {ui_lblNavX,      lang.GENERIC.REMOVE, 1},
            {NULL, NULL,                           0}
    });

    ui_imgScreenshot = lv_img_create(ui_screen);
    lv_img_set_pivot(ui_imgScreenshot, 0, 0);
    lv_img_set_src(ui_imgScreenshot, &ui_img_blank);
    lv_obj_set_style_img_opa(ui_imgScreenshot, 25, MU_OBJ_MAIN_DEFAULT);

    overlay_display();
}

static void ui_refresh_task() {
    if (ui_count > 0 && nav_moved) {
        image_refresh();
        adjust_gen_panel();

        lv_obj_invalidate(ui_pnlBox);
        nav_moved = 0;
    }
}

int muxshot_main(void) {
    skip_confirm = 0;
    is_fullscreen = 0;

    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXSHOT.TITLE);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    create_screenshot_items();
    init_elements();

    int sys_index = 0;
    if (file_exist(MUOS_IDX_LOAD)) {
        sys_index = read_line_int_from(MUOS_IDX_LOAD, 1);
        remove(MUOS_IDX_LOAD);
    }

    int nav_hidden = 0;
    if (ui_count > 0) {
        nav_hidden = 1;
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXSHOT.NONE);
    }

    struct nav_flag nav_e[] = {
            {ui_lblNavA,      nav_hidden},
            {ui_lblNavAGlyph, nav_hidden},
            {ui_lblNavX,      nav_hidden},
            {ui_lblNavXGlyph, nav_hidden}
    };
    set_nav_flags(nav_e, A_SIZE(nav_e));

    if (ui_count > 0 && sys_index <= ui_count && current_item_index < ui_count) gen_step_movement(sys_index, +1, 1, 0);

    dialogue_init_remove(&remove_dlg, &theme, ui_screen, lang.GENERIC.SELECT, lang.GENERIC.BACK);
    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_DPAD_UP] = handle_dpad_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_dpad_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_dpad_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_dpad_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    free_items(&items, &item_count);

    return 0;
}
