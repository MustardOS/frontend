#include "muxshare.h"

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_system[PATH_MAX];

static void show_help() {
    show_info_box(lang.MUXTAG.TITLE, lang.MUXTAG.HELP, 0);
}

static void write_tag_file(char *path, char *tag, char *log) {
    if (!strcasecmp(tag, "none")) return;

    FILE *file = fopen(path, "w");
    if (!file) {
        perror(lang.SYSTEM.FAIL_FILE_OPEN);
        return;
    }

    LOG_INFO(mux_module, "%s: %s", log, tag)

    fprintf(file, "%s", tag);
    fclose(file);
}

static void assign_tag_single(char *core_dir, char *tag, char *rom) {
    char tag_path[MAX_BUFFER_SIZE];
    snprintf(tag_path, sizeof(tag_path), "%s/%s.tag", core_dir, strip_ext(rom));

    if (file_exist(tag_path)) remove(tag_path);
    write_tag_file(tag_path, tag, "Single Tag Content");
}

static void assign_tag_directory(char *core_dir, char *tag, int purge) {
    if (purge) delete_files_of_type(core_dir, ".tag", NULL, 0);

    char tag_path[MAX_BUFFER_SIZE];
    snprintf(tag_path, sizeof(tag_path), "%s/%s/core.tag",
             INFO_COR_PATH, get_last_subdir(rom_dir, '/', 4));
    remove_double_slashes(tag_path);

    write_tag_file(tag_path, tag, "Directory Tag Content");
}

static void assign_tag_parent(char *core_dir, char *tag) {
    delete_files_of_type(core_dir, ".tag", NULL, 1);

    if (!strcasecmp(tag, "none")) return;

    assign_tag_directory(core_dir, tag, 0);

    char **subdirs = get_subdirectories(rom_dir);
    if (!subdirs) return;

    for (int i = 0; subdirs[i]; i++) {
        char tag_path[MAX_BUFFER_SIZE];
        snprintf(tag_path, sizeof(tag_path), "%s%s/core.tag", core_dir, subdirs[i]);

        create_directories(strip_dir(tag_path));
        write_tag_file(tag_path, tag, "Recursive Tag Content");
    }

    free_subdirectories(subdirs);
}

static void create_tag_assignment(char *tag, char *rom, enum gen_type method) {
    char core_dir[MAX_BUFFER_SIZE];
    snprintf(core_dir, sizeof(core_dir), "%s/%s/",
             INFO_COR_PATH, get_last_subdir(rom_dir, '/', 4));
    remove_double_slashes(core_dir);

    create_directories(core_dir);

    switch (method) {
        case SINGLE:
            assign_tag_single(core_dir, tag, rom);
            break;
        case PARENT:
            assign_tag_parent(core_dir, tag);
            break;
        case DIRECTORY:
            assign_tag_directory(core_dir, tag, 1);
            break;
        case DIRECTORY_NO_WIPE:
            assign_tag_directory(core_dir, tag, 0);
            break;
    }
}

static void generate_available_tags() {
    int tag_count;
    char **tags = str_parse_file(INFO_NAM_PATH "/tag.txt", &tag_count, LINES);
    if (!tags) return;

    for (int i = 0; i < tag_count; ++i) add_item(&items, &item_count, tags[i], tags[i], "", ITEM);
    sort_items(items, item_count);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; i < item_count; i++) {
        ui_count++;

        char *cap_name = str_capital(items[i].display_name);
        char *raw_name = str_tolower(str_remchar(str_trim(strdup(items[i].display_name)), ' '));

        lv_obj_t *ui_pnlTag = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlTag);

        lv_obj_set_user_data(ui_pnlTag, raw_name);

        lv_obj_t *ui_lblTagItem = lv_label_create(ui_pnlTag);
        apply_theme_list_item(&theme, ui_lblTagItem, cap_name);

        lv_obj_t *ui_lblTagItemGlyph = lv_img_create(ui_pnlTag);
        apply_theme_list_glyph(&theme, ui_lblTagItemGlyph, mux_module, str_remchar(raw_name, ' '));

        lv_group_add_obj(ui_group, ui_lblTagItem);
        lv_group_add_obj(ui_group_glyph, ui_lblTagItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlTag);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblTagItem, ui_lblTagItemGlyph, cap_name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblTagItem);
    }

    if (ui_count > 0) {
        lv_obj_update_layout(ui_pnlContent);
        free_items(&items, &item_count);
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

static void handle_a() {
    if (msgbox_active) return;

    LOG_INFO(mux_module, "Single Tag Assignment Triggered")
    play_sound(SND_CONFIRM);

    char *selected = str_tolower(str_trim(lv_label_get_text(lv_group_get_focused(ui_group))));
    create_tag_assignment(selected, rom_name, SINGLE);

    close_input();
    mux_input_stop();
}

static void handle_b() {
    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);
    remove(MUOS_SAG_LOAD);

    close_input();
    mux_input_stop();
}

static void handle_x() {
    if (msgbox_active) return;

    LOG_INFO(mux_module, "Directory Tag Assignment Triggered")
    play_sound(SND_CONFIRM);

    char *selected = str_tolower(str_trim(lv_label_get_text(lv_group_get_focused(ui_group))));
    create_tag_assignment(selected, rom_name, DIRECTORY);

    close_input();
    mux_input_stop();
}

static void handle_y() {
    if (msgbox_active) return;

    LOG_INFO(mux_module, "Parent Tag Assignment Triggered")
    play_sound(SND_CONFIRM);

    char *selected = str_tolower(str_trim(lv_label_get_text(lv_group_get_focused(ui_group))));
    create_tag_assignment(selected, rom_name, PARENT);

    close_input();
    mux_input_stop();
}

static void handle_help() {
    if (msgbox_active || progress_onscreen != -1 || !ui_count) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void adjust_panels() {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements() {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                      1},
            {ui_lblNavA,      lang.GENERIC.INDIVIDUAL, 1},
            {ui_lblNavBGlyph, "",                      0},
            {ui_lblNavB,      lang.GENERIC.BACK,       0},
            {ui_lblNavXGlyph, "",                      1},
            {ui_lblNavX,      lang.GENERIC.DIRECTORY,  1},
            {ui_lblNavYGlyph, "",                      1},
            {ui_lblNavY,      lang.GENERIC.RECURSIVE,  1},
            {NULL, NULL,                               0}
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

int muxtag_main(int nothing, char *name, char *dir, char *sys) {
    (void) nothing;

    snprintf(rom_name, sizeof(rom_name), "%s", name);
    snprintf(rom_dir, sizeof(rom_name), "%s", dir);
    snprintf(rom_system, sizeof(rom_name), "%s", sys);

    init_module("muxtag");

    LOG_INFO(mux_module, "Assign Tag ROM_NAME: \"%s\"", rom_name)
    LOG_INFO(mux_module, "Assign Tag ROM_DIR: \"%s\"", rom_dir)
    LOG_INFO(mux_module, "Assign Tag ROM_SYS: \"%s\"", rom_system)

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, "");
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);
    init_fonts();

    char title[MAX_BUFFER_SIZE];
    snprintf(title, sizeof(title), "%s - %s", lang.MUXTAG.TITLE, get_last_dir(rom_dir));
    lv_label_set_text(ui_lblTitle, title);

    generate_available_tags();

    if (ui_count > 0) {
        LOG_SUCCESS(mux_module, "%d Tag%s Detected", ui_count, ui_count == 1 ? "" : "s")
        list_nav_next(0);
    } else {
        LOG_ERROR(mux_module, "No Tags Detected!")
        lv_label_set_text(ui_lblScreenMessage, lang.MUXTAG.NONE);
    }

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
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
