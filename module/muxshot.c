#include "muxshare.h"

lv_obj_t *ui_imgScreenshot;
static int is_fullscreen = 0;

static void show_help(void) {
    show_info_box(TS(lv_label_get_text(lv_group_get_focused(ui_group))), lang.MUXSHOT.HELP, 0);
}

static void image_refresh(void) {
    // Invalidate the cache for this image path
    lv_img_cache_invalidate_src(lv_img_get_src(ui_imgScreenshot));

    char screenshot_file[PATH_MAX];
    snprintf(screenshot_file, sizeof(screenshot_file), "%s/%s.png",
             STORAGE_SHOTS, lv_label_get_text(lv_group_get_focused(ui_group)));

    if (file_exist(screenshot_file)) {
        char screenshot_image[PATH_MAX];
        snprintf(screenshot_image, sizeof(screenshot_image), "M:%s", screenshot_file);
        lv_img_set_src(ui_imgScreenshot, screenshot_image);
    } else {
        lv_img_set_src(ui_imgScreenshot, &ui_image_Nothing);
    }
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

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

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

        apply_size_to_content(&theme, ui_pnlContent, ui_lblScreenshotItem, ui_lblScreenshotItemGlyph,
                              items[i].display_name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblScreenshotItem);
    }

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
}

static void list_nav_move(int steps, int direction) {
    if (!ui_count) return;
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    image_refresh();
    set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || !ui_count || hold_call) return;

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

    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);

    close_input();
    mux_input_stop();
}

static void handle_remove(void) {
    if (msgbox_active || is_fullscreen || !ui_count || hold_call) return;

    char screenshot_file[PATH_MAX];
    snprintf(screenshot_file, sizeof(screenshot_file), "%s/%s.png",
             STORAGE_SHOTS, lv_label_get_text(lv_group_get_focused(ui_group)));

    if (file_exist(screenshot_file)) {
        remove(screenshot_file);
        load_mux("screenshot");

        close_input();
        mux_input_stop();
    }
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || is_fullscreen || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements(void) {
    adjust_panels();
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
    lv_img_set_src(ui_imgScreenshot, &ui_image_Nothing);
    lv_obj_set_style_img_opa(ui_imgScreenshot, 25, MU_OBJ_MAIN_DEFAULT);

    overlay_display();
}

static void ui_refresh_task() {
    if (ui_count > 0 && nav_moved) {
        image_refresh();

        adjust_panels();

        lv_obj_invalidate(ui_pnlBox);
        nav_moved = 0;
    }
}

int muxshot_main(void) {
    is_fullscreen = 0;

    const char *m = "muxshot";
    set_process_name(m);
    init_module(m);

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXSHOT.TITLE);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    create_screenshot_items();
    init_elements();

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

    init_timer(ui_refresh_task, NULL);

    list_nav_next(0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_remove,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    free_items(&items, &item_count);

    return 0;
}
