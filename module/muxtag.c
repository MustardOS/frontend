#include "muxshare.h"

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_system[PATH_MAX];

static void show_help(void) {
    show_info_box(lang.muxtag.title, lang.muxtag.help, 0);
}

static void create_tag_assignment(const char *tag, const char *rom, const enum gen_type method) {
    create_marker_assignment("tag", "Assign Tag", tag, rom, rom_dir, 0, method);
}

static void generate_available_tags(void) {
    int tag_count;

    const char *tag_path = resolve_info_path("name/tag.txt");
    if (!tag_path) return;

    char **tags = str_parse_file(tag_path, &tag_count, parse_lines);
    if (!tags) return;

    for (int i = 0; i < tag_count; ++i)
        add_item(&items, &item_count, tags[i], tags[i], "", ITEM);
    sort_items(items, item_count);

    reset_ui_groups();

    for (size_t i = 0; i < item_count; i++) {
        ui_count_static++;

        const char *cap_name = str_capital(items[i].display_name);
        char *raw_name = str_tolower(str_remchar(str_trim(strdup(items[i].display_name)), ' '));

        lv_obj_t *ui_pnl_tag = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_tag);

        lv_obj_t *ui_lbl_tag_item = lv_label_create(ui_pnl_tag);
        apply_theme_list_item(&theme, ui_lbl_tag_item, cap_name);

        lv_obj_t *ui_lbl_tag_item_glyph = lv_img_create(ui_pnl_tag);
        apply_theme_list_glyph(&theme, ui_lbl_tag_item_glyph, mux_module, str_remchar(raw_name, ' '));
        free(raw_name);

        lv_group_add_obj(ui_group, ui_lbl_tag_item);
        lv_group_add_obj(ui_group_glyph, ui_lbl_tag_item_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_tag);

        apply_size_to_content(&theme, ui_pnl_content, ui_lbl_tag_item, ui_lbl_tag_item_glyph, cap_name);
        apply_text_long_dot(&theme, ui_lbl_tag_item);
    }

    if (ui_count_static > 0) {
        lv_obj_update_layout(ui_pnl_content);
        free_items(&items, &item_count);
    }
}

static void handle_a(void) {
    if (msgbox_active || !ui_count_static || hold_call) return;

    LOG_INFO(mux_module, "Single Tag Assignment Triggered");
    play_sound(snd_confirm);

    const char *selected = str_tolower(str_trim(lv_label_get_text(lv_group_get_focused(ui_group))));
    create_tag_assignment(selected, rom_name, SINGLE);

    mux_input_stop();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    remove(MUOS_SAG_LOAD);

    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || !ui_count_static || hold_call) return;

    LOG_INFO(mux_module, "Directory Tag Assignment Triggered");
    play_sound(snd_confirm);

    const char *selected = str_tolower(str_trim(lv_label_get_text(lv_group_get_focused(ui_group))));
    create_tag_assignment(selected, rom_name, DIRECTORY);

    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || !ui_count_static || hold_call) return;

    LOG_INFO(mux_module, "Parent Tag Assignment Triggered");
    play_sound(snd_confirm);

    const char *selected = str_tolower(str_trim(lv_label_get_text(lv_group_get_focused(ui_group))));
    create_tag_assignment(selected, rom_name, PARENT);

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 1},
                                  {ui_lbl_nav_a, lang.generic.content, 1},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 1},
                                  {ui_lbl_nav_x, lang.generic.directory, 1},
                                  {ui_lbl_nav_y_glyph, "", 1},
                                  {ui_lbl_nav_y, lang.generic.recursive, 1},
                                  {NULL, NULL, 0}});

    overlay_display();
}

void muxtag_main(int auto_assign, const char *name, const char *dir, const char *sys, int app) {
    (void) auto_assign;
    (void) app;

    snprintf(rom_name, sizeof(rom_name), "%s", get_file_name(name));
    snprintf(rom_dir, sizeof(rom_dir), "%s", dir);
    snprintf(rom_system, sizeof(rom_system), "%s", sys);

    init_module(__func__);

    LOG_INFO(mux_module, "Assign Tag ROM_NAME: \"%s\"", rom_name);
    LOG_INFO(mux_module, "Assign Tag ROM_DIR: \"%s\"", rom_dir);
    LOG_INFO(mux_module, "Assign Tag ROM_SYS: \"%s\"", rom_system);

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxtag.title);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);
    init_fonts();

    generate_available_tags();

    if (ui_count_static > 0) {
        LOG_SUCCESS(mux_module, "%d Tag%s Detected", ui_count_static, ui_count_static == 1 ? "" : "s");
        gen_step_movement(0, +1, 1, 0, 1);
    } else {
        LOG_ERROR(mux_module, "No Tags Detected!");
        lv_label_set_text(ui_lbl_screen_message, lang.muxtag.none);
    }

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_dpad_up] = handle_list_nav_up,
                [mux_input_dpad_down] = handle_list_nav_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_list_nav_up_hold,
            [mux_input_dpad_down] = handle_list_nav_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);
}
