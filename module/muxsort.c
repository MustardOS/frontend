#include "muxshare.h"

static int default_original;
static int collection_original;
static int history_original;

static lv_obj_t **ui_objects_value = NULL;

static void show_help(void) {
    char *title = TS(lv_label_get_text(lv_group_get_focused(ui_group)));

    if (current_item_index == 0) {
        show_info_box(title, lang.MUXSORT.HELP.DEFAULT, 0);
    } else if (current_item_index == 1) {
        show_info_box(title, lang.MUXSORT.HELP.COLLECTION, 0);
    } else if (current_item_index == 2) {
        show_info_box(title, lang.MUXSORT.HELP.HISTORY, 0);
    } else {
        char help_message[MAX_BUFFER_SIZE];
        snprintf(help_message, sizeof(help_message), lang.MUXSORT.HELP.TAG, title);
        show_info_box(title, help_message, 0);
    }
}

static void init_dropdown_settings(void) {
    default_original = lv_dropdown_get_selected(ui_objects_value[0]);
    collection_original = lv_dropdown_get_selected(ui_objects_value[1]);
    history_original = lv_dropdown_get_selected(ui_objects_value[2]);
}

static void restore_clamped_value(lv_obj_t *value, int selected) {
    if (!value) return;

    int max = (int) lv_dropdown_get_option_cnt(value) - 1;
    if (max < 0) max = 0;

    lv_dropdown_set_selected(value, selected > max ? max : selected);
}

static void restore_sort_options(void) {
    restore_clamped_value(ui_objects_value[0], config.SORT.DEFAULT);
    restore_clamped_value(ui_objects_value[1], config.SORT.COLLECTION);
    restore_clamped_value(ui_objects_value[2], config.SORT.HISTORY);

    for (size_t i = 0; i < tag_item_count; i++) {
        restore_clamped_value(ui_objects_value[3 + i], tag_items[i].sort_bucket);
    }
}

static int save_sort_option(lv_obj_t *ui_droItem, int originalValue, char *file) {
    int current = lv_dropdown_get_selected(ui_droItem);
    if (current != originalValue) {
        char config_path[MAX_BUFFER_SIZE];
        snprintf(config_path, sizeof(config_path), SORTING_CONFIG_PATH "%s", file);
        write_text_to_file(config_path, "w", INT, current);
        return 1;
    }
    return 0;
}

static void save_sort_options(void) {
    int is_modified = 0;

    is_modified += save_sort_option(ui_objects_value[0], default_original, "default");
    is_modified += save_sort_option(ui_objects_value[1], collection_original, "collection");
    is_modified += save_sort_option(ui_objects_value[2], history_original, "history");

    for (size_t i = 0; i < tag_item_count; i++) {
        is_modified += save_sort_option(ui_objects_value[3 + i], tag_items[i].sort_bucket, tag_items[i].glyph);
    }

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, FOREVER);
        refresh_config = 1;
    }
}

static void add_tag_item(int index, char *label, char *glyph, char *options) {
    lv_obj_t *ui_pnlItem = lv_obj_create(ui_pnlContent);
    lv_obj_t *ui_lblItem = lv_label_create(ui_pnlItem);
    lv_obj_t *ui_lblItemGlyph = lv_img_create(ui_pnlItem);
    lv_obj_t *ui_droItem = lv_dropdown_create(ui_pnlItem);

    apply_theme_list_panel(ui_pnlItem);
    apply_theme_list_item(&theme, ui_lblItem, label);
    apply_theme_list_glyph(&theme, ui_lblItemGlyph, mux_module, glyph);
    apply_theme_list_drop_down(&theme, ui_droItem, options);

    lv_group_add_obj(ui_group, ui_lblItem);
    lv_group_add_obj(ui_group_value, ui_droItem);
    lv_group_add_obj(ui_group_glyph, ui_lblItemGlyph);
    lv_group_add_obj(ui_group_panel, ui_pnlItem);

    ui_objects_value[index] = ui_droItem;
}

static void init_navigation_group(void) {
    reset_ui_groups();

    ui_count = (int) (3 + tag_item_count);
    ui_objects_value = malloc(ui_count * sizeof(lv_obj_t *));

    char *increment_values = generate_number_string(0, ui_count, 1, NULL, NULL, NULL, 0);

    add_tag_item(0, lang.MUXSORT.DEFAULT, "default", increment_values);
    add_tag_item(1, lang.MUXSORT.COLLECTION, "collection", increment_values);
    add_tag_item(2, lang.MUXSORT.HISTORY, "history", increment_values);

    for (size_t i = 0; i < tag_item_count; i++) {
        add_tag_item((int) (i + 3), tag_items[i].name, tag_items[i].glyph, increment_values);
    }

    free(increment_values);
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, false, 0);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
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
    save_sort_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "interface");
    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {NULL, NULL,                            0}
    });

    overlay_display();
}

int muxsort_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXSORT.TITLE);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    load_tag_items(&tag_items, &tag_item_count);
    sort_tag_items(tag_items, tag_item_count);
    init_navigation_group();

    restore_sort_options();
    init_dropdown_settings();

    init_timer(ui_gen_refresh_task, NULL);
    list_nav_next(0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_option_next,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
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
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
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

    free_tag_items(&tag_items, &tag_item_count);

    free(ui_objects_value);
    ui_objects_value = NULL;

    return 0;
}
