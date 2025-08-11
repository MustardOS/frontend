#include "muxshare.h"

static void show_help(void) {
    show_info_box(lang.MUXTIMEZONE.TITLE, lang.MUXTIMEZONE.HELP, 0);
}

static void create_timezone_items(void) {
    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; timezone_location[i] != NULL; i++) {
        const char *base_key = timezone_location[i];

        ui_count++;

        lv_obj_t *ui_pnlTimezone = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlTimezone);
        lv_obj_set_user_data(ui_pnlTimezone, strdup(base_key));

        lv_obj_t *ui_lblTimezoneItem = lv_label_create(ui_pnlTimezone);
        apply_theme_list_item(&theme, ui_lblTimezoneItem, base_key);

        lv_obj_t *ui_lblTimezoneGlyph = lv_img_create(ui_pnlTimezone);
        apply_theme_list_glyph(&theme, ui_lblTimezoneGlyph, mux_module, "timezone");

        lv_group_add_obj(ui_group, ui_lblTimezoneItem);
        lv_group_add_obj(ui_group_glyph, ui_lblTimezoneGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlTimezone);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblTimezoneItem, ui_lblTimezoneGlyph, base_key);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblTimezoneItem);
    }

    if (ui_count > 0) {
        lv_obj_update_layout(ui_pnlContent);
        set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    }
}

static void list_nav_move(int steps, int direction) {
    if (ui_count <= 0) return;
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
    if (msgbox_active) return;

    play_sound(SND_CONFIRM);
    toast_message(lang.MUXTIMEZONE.SAVE, 0);
    refresh_screen(ui_screen);

    char zone_group[MAX_BUFFER_SIZE];
    snprintf(zone_group, sizeof(zone_group), "/usr/share/zoneinfo/%s",
             lv_label_get_text(lv_group_get_focused(ui_group)));

    unlink(LOCAL_TIME);
    if (symlink(zone_group, LOCAL_TIME) != 0) {
        LOG_ERROR(mux_module, "Failed to timezone symlink")
    }

    // Because weirdos live in different timezones...
    if (config.BOOT.FACTORY_RESET) {
        const char *args_date[] = {"date", "010100002025", NULL};
        run_exec(args_date, A_SIZE(args_date), 0);

        const char *args_hw_clock[] = {"hwclock", "-w", NULL};
        run_exec(args_hw_clock, A_SIZE(args_hw_clock), 0);
    }

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "timezone");
    refresh_config = 1;

    close_input();
    mux_input_stop();
}

static void handle_b(void) {
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

static void handle_menu(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count) return;

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
            {ui_lblNavAGlyph, "",                  0},
            {ui_lblNavA,      lang.GENERIC.SELECT, 0},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {NULL, NULL,                           0}
    });

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxtimezone_main(void) {
    init_module("muxtimezone");

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXTIMEZONE.TITLE);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    create_timezone_items();

    if (!ui_count) lv_label_set_text(ui_lblScreenMessage, lang.MUXTIMEZONE.NONE);

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
