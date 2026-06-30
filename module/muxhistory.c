#include "muxshare.h"
#include "ui/ui_muxhistory.h"

static lv_obj_t *ui_img_splash;
static lv_obj_t *ui_viewport_objects[7];

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

static int exit_status = 0;
static int file_count = 0;
static int file_cap = 0;
static int starter_image = 0;
static int splash_valid = 0;

static char current_meta_text[MAX_BUFFER_SIZE];
static char current_content_label[MAX_BUFFER_SIZE];

static char *load_content_description(void) {
    char *item_dir = get_content_path(items[current_item_index].extra_data);

    char item_file_name_buf[MAX_BUFFER_SIZE];
    snprintf(item_file_name_buf, sizeof(item_file_name_buf), "%s", get_last_dir(items[current_item_index].extra_data));

    char core_desc[MAX_BUFFER_SIZE];
    get_catalogue_name(item_dir, item_file_name_buf, core_desc, sizeof(core_desc));

    if (strlen(core_desc) <= 1 && items[current_item_index].content_type == ITEM) return lang.generic.no_info;

    char item_no_ext[MAX_BUFFER_SIZE];
    snprintf(item_no_ext, sizeof(item_no_ext), "%s", item_file_name_buf);

    char *dot = strrchr(item_no_ext, '.');
    if (dot) *dot = '\0';

    char content_desc[MAX_BUFFER_SIZE];
    snprintf(content_desc, sizeof(content_desc), INFO_CAT_PATH "/%s/text/%s.txt", core_desc, item_no_ext);

    if (file_exist(content_desc)) return read_all_char_from(content_desc);

    snprintf(current_meta_text, sizeof(current_meta_text), " ");
    return lang.generic.no_info;
}

static void image_refresh(const char *image_type) {
    if (!ui_count_static) return;
    if (strcasecmp(image_type, "box") == 0 && config.visual.box_art == 8) return;

    char *item_dir = get_content_path(items[current_item_index].extra_data);

    char item_file_name_buf[MAX_BUFFER_SIZE];
    snprintf(item_file_name_buf, sizeof(item_file_name_buf), "%s", get_last_dir(items[current_item_index].extra_data));

    char h_core_artwork[MAX_BUFFER_SIZE];
    get_catalogue_name(item_dir, item_file_name_buf, h_core_artwork, sizeof(h_core_artwork));

    char h_file_name[MAX_BUFFER_SIZE];
    snprintf(h_file_name, sizeof(h_file_name), "%s", item_file_name_buf);

    char *dot = strrchr(h_file_name, '.');
    if (dot) *dot = '\0';

    render_image_refresh(
        image_type, h_core_artwork, h_file_name, ui_img_splash, ui_viewport_objects, &starter_image, &splash_valid
    );
}

static void video_refresh(void) {
    if (!ui_count_static) return;

    char *item_dir = get_content_path(items[current_item_index].extra_data);

    char item_file_name_buf[MAX_BUFFER_SIZE];
    snprintf(item_file_name_buf, sizeof(item_file_name_buf), "%s", get_last_dir(items[current_item_index].extra_data));

    char h_core_artwork[MAX_BUFFER_SIZE];
    get_catalogue_name(item_dir, item_file_name_buf, h_core_artwork, sizeof(h_core_artwork));

    char h_file_name[MAX_BUFFER_SIZE];
    snprintf(h_file_name, sizeof(h_file_name), "%s", item_file_name_buf);
    char *dot = strrchr(h_file_name, '.');
    if (dot) *dot = '\0';

    char mux_dim[MAX_BUFFER_SIZE];
    snprintf(mux_dim, sizeof(mux_dim), "%dx%d", device.mux.width, device.mux.height);

    char vpath[MAX_BUFFER_SIZE];
    if (!load_video_catalogue(h_core_artwork, h_file_name, h_file_name, mux_dim, vpath, sizeof(vpath))) return;

    const int delay_ms = config.visual.video_preview == 3 ? 10000 : config.visual.video_preview == 2 ? 5000 : 3000;
    video_preview_arm(vpath, delay_ms, ui_pnl_box, ui_img_box);
}

static void image_refresh_transition(void) {
    image_refresh("box");
    transition_box_art_apply_in(config.visual.box_art_transition);
    if (config.visual.video_preview > 0) video_refresh();
}

static void add_file_names(const char *base_dir, char ***file_names) {
    DIR *dir = opendir(base_dir);

    if (!dir) {
        LOG_ERROR(mux_module, "%s", lang.system.fail_dir_open);
        return;
    }

    struct dirent *entry;
    file_count = 0;
    file_cap = 0;

    while ((entry = readdir(dir))) {
        const char *name = entry->d_name;

        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) continue;
        if (!config.visual.hidden && name[0] == '.') continue;

        int type = entry->d_type;

        if (type == DT_UNKNOWN) {
            char full_path[PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, name);

            struct stat st;
            if (stat(full_path, &st) == 0) {
                if (S_ISREG(st.st_mode)) type = DT_REG;
            }
        }

        if (type != DT_REG) continue;
        if (strcmp(name, ".nogrid") == 0) continue;

        if (file_count >= file_cap) {
            const int new_cap = file_cap ? file_cap * 2 : 16;
            char **tmp = realloc(*file_names, (size_t) new_cap * sizeof(char *));

            if (!tmp) continue;

            *file_names = tmp;
            file_cap = new_cap;
        }
        (*file_names)[file_count++] = strdup(name);
    }

    closedir(dir);
}

static char *get_glyph_name(const size_t index) {
    char name_no_ext[MAX_BUFFER_SIZE];
    snprintf(name_no_ext, sizeof(name_no_ext), "%s", items[index].name);

    char *dot = strrchr(name_no_ext, '.');
    if (dot) *dot = '\0';

    char history_file[PATH_MAX];
    snprintf(history_file, sizeof(history_file), INFO_HIS_PATH "/%s.cfg", name_no_ext);

    const char *system_name = read_line_char_from(history_file, cache_core_dir);

    const char *collection_path =
        is_ksk(kiosk.collect.access) && dir_exist(INFO_CKS_PATH) ? INFO_CKS_PATH : INFO_COL_PATH;

    if (search_for_config(collection_path, name_no_ext, system_name)) return "collection";

    return "history";
}

static void gen_item(const int file_count, char **file_names) {
    char init_meta_dir[MAX_BUFFER_SIZE];
    for (int i = 0; i < file_count; i++) {
        char fn_name[MAX_BUFFER_SIZE];

        char history_file[MAX_BUFFER_SIZE];
        snprintf(history_file, sizeof(history_file), "%s/%s", INFO_HIS_PATH, file_names[i]);

        union_rewrite_file_paths(history_file);
        char *file_path_raw = read_line_char_from(history_file, cache_core_path);

        char resolved_path[PATH_MAX];
        if (union_resolve_to_real(file_path_raw, resolved_path, sizeof(resolved_path))) {
            free(file_path_raw);
            file_path_raw = strdup(resolved_path);
        }

        const char *file_path = file_path_raw;
        char *sub_path = read_line_char_from(history_file, cache_core_dir);

        snprintf(init_meta_dir, sizeof(init_meta_dir), INFO_CON_PATH "/%s/", sub_path);
        create_directories(init_meta_dir, 0);

        resolve_friendly_name(file_path, fn_name);
        add_item(&items, &item_count, file_names[i], fn_name, file_path, ITEM);

        ui_count_static++;
        free(file_names[i]);
    }

    sort_items_time(items, item_count);

    char *glyph_icons[item_count];
    for (size_t i = 0; i < item_count; i++) {
        glyph_icons[i] = items[i].content_type == ITEM ? get_glyph_name(i) : "unknown";
    }

    if (grid_mode_enabled) return;

    for (size_t i = 0; i < item_count; i++) {
        if (items[i].content_type == ITEM) {
            gen_label(mux_module, glyph_icons[i], items[i].display_name);
        }
    }
}

static void init_navigation_group_grid(void) {
    grid_mode_enabled = 1;

    init_grid_info((int) item_count, theme.grid.column_count);
    create_grid_panel(&theme, (int) item_count);

    load_font_section(FONT_PANEL_DIR, ui_pnl_grid);
    load_font_section(FONT_PANEL_DIR, ui_lbl_grid_current_item);

    if (item_count == 0) return;

    if (is_carousel_grid_mode()) {
        create_carousel_grid();
        return;
    }

    const int visible_count = theme.grid.column_count * theme.grid.row_count;
    if (visible_count <= 0) return;

    for (int i = 0; i < (int) item_count && i < visible_count; i++) {
        gen_grid_item(i);
    }
}

static void create_history_items(void) {
    turbo_time(1, 1);

    lv_label_set_text(ui_lbl_title, lang.muxhistory.title);

    char **file_names = NULL;
    add_file_names(INFO_HIS_PATH, &file_names);

    grid_mode_enabled = !disable_grid_file_exists(INFO_HIS_PATH) && theme.grid.enabled && file_count > 0
                        && config.visual.grid_mode_content;

    if (file_count > 0) {
        gen_item(file_count, file_names);
        lv_obj_update_layout(ui_pnl_content);
    }

    if (grid_mode_enabled) init_navigation_group_grid();

    free(file_names);

    turbo_time(0, 1);
}

static void list_nav_move(const int steps, const int direction) {
    if (!ui_count_static) return;
    first_open ? (first_open = 0) : play_sound(snd_navigate);

    for (int step = 0; step < steps; ++step) {
        if (!grid_mode_enabled) apply_text_long_dot(&theme, lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = current_item_index == 0 ? ui_count_static - 1 : current_item_index - 1;
        } else {
            current_item_index = current_item_index == ui_count_static - 1 ? 0 : current_item_index + 1;
        }

        if (!is_carousel_grid_mode()) {
            nav_move(ui_group, direction);
            nav_move(ui_group_glyph, direction);
            nav_move(ui_group_panel, direction);
        }

        if (grid_mode_enabled) update_grid(direction);
    }

    if (!grid_mode_enabled) {
        update_scroll_position(
            theme.mux.item.count, theme.mux.item.panel, ui_count_static, current_item_index, ui_pnl_content
        );
        set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);
    }
    lv_label_set_text(ui_lbl_grid_current_item, items[current_item_index].display_name);

    video_preview_cancel();

    if (config.visual.box_art < 4) {
        if (config.visual.box_art_transition != TSN_DISABLED) {
            transition_box_art_nav_activity();
        } else {
            image_refresh("box");
            if (config.visual.video_preview > 0) video_refresh();
        }
    }

    nav_moved = 1;
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void show_splash() {
    if (config.visual.launchsplash) {
        image_refresh("splash");
        if (splash_valid) {
            lv_obj_center(ui_img_splash);
            lv_obj_move_foreground(ui_img_splash);
            if (overlay_image) lv_obj_move_foreground(overlay_image);

            for (unsigned int i = 0; i <= 255; i += 15) {
                lv_obj_set_style_img_opa(ui_img_splash, i, MU_OBJ_MAIN_DEFAULT);
                lv_task_handler();
                usleep(4096);
            }

            sleep(1);
        }
    }
}

static void process_load(const int from_start) {
    if (!ui_count_static || hold_call) return;

    if (msgbox_active) {
        play_sound(snd_info_close);
        if (lv_obj_has_flag(ui_pnl_help_preview, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnl_help_message, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnl_help_preview, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ui_pnl_help_preview, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnl_help_message, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    char resolved_path[PATH_MAX];
    const char *launch_path = items[current_item_index].extra_data;

    if (!union_resolve_to_real(launch_path, resolved_path, sizeof(resolved_path)) || !file_exist(resolved_path)) {
        play_sound(snd_error);
        toast_message(lang.generic.no_load, tst_wait_l);

        LOG_ERROR(mux_module, "Could not launch content: %s", resolved_path[0] ? resolved_path : launch_path);
        return;
    }

    if (strcmp(launch_path, resolved_path) != 0) {
        char history_file[MAX_BUFFER_SIZE];
        snprintf(history_file, sizeof(history_file), INFO_HIS_PATH "/%s", items[current_item_index].name);

        if (!ends_with(history_file, ".cfg"))
            strncat(history_file, ".cfg", sizeof(history_file) - strlen(history_file) - 1);

        if (file_exist(history_file)) {
            rewrite_launch_file(history_file, resolved_path);

            char *content_name = strip_ext(items[current_item_index].name);
            migrate_history_entry(history_file, resolved_path, content_name);
            free(content_name);
        }

        snprintf(items[current_item_index].extra_data, MAX_BUFFER_SIZE, "%s", resolved_path);
    }

    play_sound(snd_confirm);

    if (load_content(0, resolved_path)) {
        if (strcasecmp(passcode.code.launch, "000000") != 0) {
            int result = 0;

            while (result != 1) {
                result = muxpass_main(pct_launch);

                if (result == 1) {
                    show_splash();
                    fade_out_screen();
                    exit_status = 1;
                } else {
                    if (file_exist(MUOS_ROM_LOAD)) remove(MUOS_ROM_LOAD);
                    if (file_exist(MUOS_CON_LOAD)) remove(MUOS_CON_LOAD);
                    if (file_exist(MUOS_GOV_LOAD)) remove(MUOS_GOV_LOAD);
                    if (file_exist(MUOS_RAC_LOAD)) remove(MUOS_RAC_LOAD);
                    if (file_exist(MUOS_FLT_LOAD)) remove(MUOS_FLT_LOAD);
                    if (file_exist(MUOS_SHD_LOAD)) remove(MUOS_SHD_LOAD);
                    goto load_end;
                }
            }
        } else {
            show_splash();
            fade_out_screen();
            exit_status = 1;
        }
    } else {
        write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);
        write_text_to_file(OPTION_SKIP, "w", CHAR, "");
        write_text_to_file(MUOS_ASS_FROM, "w", CHAR, "history");
        load_mux("assign");
    }

    if (from_start) write_text_to_file(MANUAL_RA_LOAD, "w", INT, 1);

load_end:
    load_mux("history");

    mux_input_stop();
}

static void do_remove(void) {
    char name_no_ext[MAX_BUFFER_SIZE];
    snprintf(name_no_ext, sizeof(name_no_ext), "%s", items[current_item_index].name);

    char *dot = strrchr(name_no_ext, '.');
    if (dot) *dot = '\0';

    char history_file[MAX_BUFFER_SIZE];
    snprintf(history_file, sizeof(history_file), INFO_HIS_PATH "/%s.cfg", name_no_ext);

    if (file_exist(history_file)) {
        write_text_to_file(MUOS_IDX_LOAD, "w", INT, get_index_on_delete(current_item_index, ui_count_static - 1));
        play_sound(snd_muos);
        remove(history_file);
        load_mux("history");
        mux_input_stop();
    } else {
        play_sound(snd_error);
        toast_message(lang.generic.remove_fail, tst_wait_m);
    }
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

static void handle_dpad_left(void) {
    if (remove_mode) {
        if (swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_left();
}

static void handle_dpad_right(void) {
    if (remove_mode) {
        if (swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_right();
}

static void handle_dpad_up_hold(void) {
    if (remove_mode) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (remove_mode) return;

    handle_list_nav_down_hold();
}

static void handle_dpad_left_hold(void) {
    if (remove_mode) return;

    handle_list_nav_left_hold();
}

static void handle_dpad_right_hold(void) {
    if (remove_mode) return;

    handle_list_nav_right_hold();
}

static void handle_a(void) {
    if (hold_call || video_preview_active()) return;

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

    video_preview_cancel();
    process_load(launch_flag(config.visual.launch_swap, 0));
}

static void handle_a_hold(void) {
    if (msgbox_active || hold_call || remove_mode || video_preview_active()) return;
    process_load(launch_flag(config.visual.launch_swap, 1));
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

    if (video_preview_active()) {
        video_preview_cancel();
        play_sound(snd_back);
        return;
    }

    video_preview_cancel();
    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "history");

    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || !ui_count_static || is_ksk(kiosk.content.history) || remove_mode || video_preview_active())
        return;

    if (config.settings.advanced.trust_remove || skip_confirm) {
        do_remove();
        return;
    }

    play_sound(snd_confirm);
    show_remove_dialog();
}

static void handle_y(void) {
    if (msgbox_active || !ui_count_static || is_ksk(kiosk.collect.add_con) || hold_call || video_preview_active())
        return;

    write_text_to_file(ADD_MODE_FROM, "w", CHAR, "history");
    if (!load_content(1, items[current_item_index].extra_data)) {
        remove(ADD_MODE_FROM);
        play_sound(snd_error);
        toast_message(lang.muxplore.error.no_core, tst_wait_s);
    }
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call || video_preview_active()) return;

    play_sound(snd_info_open);
    image_refresh("preview");

    show_info_box(items[current_item_index].display_name, load_content_description(), 1);
}

static void handle_random_select(void) {
    if (msgbox_active || ui_count_static < 2 || hold_call || !config.visual.shuffle || video_preview_active()) return;

    int dir, target;
    shuffle_index(current_item_index, &dir, &target);

    list_nav_move(target, dir);
}

static void handle_nav_key_released(void) {
    if (config.visual.box_art_transition != TSN_DISABLED) {
        transition_box_art_key_released();
    }
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {ui_pnl_footer, ui_pnl_header, ui_lbl_counter_history, ui_pnl_help,
                                          ui_pnl_progress_brightness, ui_pnl_progress_volume, ui_pnl_message, NULL});
    if (config.visual.box_art == 3) lv_obj_move_foreground(ui_pnl_box);
}

static void init_elements(void) {
    lv_obj_set_align(ui_img_box, config.visual.box_art_align);
    lv_obj_set_align(ui_viewport_objects[0], config.visual.box_art_align);

    adjust_box_art();
    adjust_panels();
    header_and_footer_setup();
    lv_label_set_text(ui_lbl_preview_header, lang.generic.switch_image);
    lv_obj_clear_flag(ui_lbl_preview_header_glyph, LV_OBJ_FLAG_HIDDEN);

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 1},
                                  {ui_lbl_nav_a, lang.generic.open, 1},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 1},
                                  {ui_lbl_nav_x, lang.generic.remove, 1},
                                  {ui_lbl_nav_y_glyph, "", 1},
                                  {ui_lbl_nav_y, lang.generic.collect, 1},
                                  {ui_lbl_nav_menu_glyph, "", 1},
                                  {ui_lbl_nav_menu, lang.generic.info, 1},
                                  {NULL, NULL, 0}});

    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    if (nav_moved) {
        starter_image = adjust_wallpaper_element(ui_group, starter_image, wall_general);
        adjust_panels();

        const char *content_label = lv_obj_get_user_data(lv_group_get_focused(ui_group));
        snprintf(current_content_label, sizeof(current_content_label), "%s", content_label);

        if (!lv_obj_has_flag(ui_pnl_message, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnl_message, LV_OBJ_FLAG_HIDDEN);
        }

        update_file_counter(ui_lbl_counter_history, file_count);
        if (overlay_image) lv_obj_move_foreground(overlay_image);

        nav_moved = 0;
    }
}

int muxhistory_main(const int his_index) {
    skip_confirm = 0;
    exit_status = 0;
    file_count = 0;
    starter_image = 0;
    splash_valid = 0;

    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, "");
    init_muxhistory(ui_screen, &theme);

    ui_viewport_objects[0] = lv_obj_create(ui_pnl_box);
    ui_viewport_objects[1] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[2] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[3] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[4] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[5] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[6] = lv_img_create(ui_viewport_objects[0]);

    ui_img_splash = lv_img_create(ui_screen);
    lv_obj_set_style_img_opa(ui_img_splash, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    init_fonts();

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    reset_ui_groups();

    create_history_items();
    init_elements();

    transition_box_art_init(image_refresh_transition);

    int nav_vis = 0;
    if (ui_count_static > 0) {
        nav_vis = 1;
        if (his_index > -1 && his_index <= ui_count_static && current_item_index < ui_count_static) {
            list_nav_move(his_index, +1);
        } else {
            if (!grid_mode_enabled)
                set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);
            if (config.visual.box_art < 4) {
                image_refresh("box");
                if (config.visual.video_preview > 0) video_refresh();
            }
        }
        nav_moved = 1;
    } else {
        lv_label_set_text(ui_lbl_screen_message, lang.muxhistory.none);
    }

    const struct nav_flag nav_e[] = {{ui_lbl_nav_a, nav_vis},    {ui_lbl_nav_a_glyph, nav_vis},
                                     {ui_lbl_nav_x, nav_vis},    {ui_lbl_nav_x_glyph, nav_vis},
                                     {ui_lbl_nav_y, nav_vis},    {ui_lbl_nav_y_glyph, nav_vis},
                                     {ui_lbl_nav_menu, nav_vis}, {ui_lbl_nav_menu_glyph, nav_vis}};

    set_nav_flags(nav_e, A_SIZE(nav_e));
    adjust_panels();

    if (is_ksk(kiosk.content.history)) {
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    if (is_ksk(kiosk.collect.add_con)) {
        lv_obj_add_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    update_file_counter(ui_lbl_counter_history, file_count);

    if (file_exist(ADD_MODE_DONE)) {
        char *done_content = read_all_char_from(ADD_MODE_DONE);
        if (done_content && strncasecmp(done_content, "DONE", 4) == 0) {
            char *col_file = read_line_char_from(ADD_MODE_DONE, 2);
            if (col_file && *col_file) check_collection(col_file);

            free(col_file);
            refresh_screen(ui_screen, 1);
        }

        free(done_content);
        remove(ADD_MODE_DONE);
    }

    dialogue_init_remove(&remove_dlg, &theme, ui_screen, NULL, lang.generic.select, lang.generic.back);
    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1
                     || (grid_mode_enabled && theme.grid.navigation_type >= 1 && theme.grid.navigation_type <= 5),
        .press_handler =
            {
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_dpad_left] = handle_dpad_left,
                [mux_input_dpad_right] = handle_dpad_right,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
                [mux_input_r2] = handle_random_select,
            },
        .release_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_menu] = handle_help,
                [mux_input_dpad_up] = handle_nav_key_released,
                [mux_input_dpad_down] = handle_nav_key_released,
                [mux_input_dpad_left] = handle_nav_key_released,
                [mux_input_dpad_right] = handle_nav_key_released,
                [mux_input_l1] = handle_nav_key_released,
                [mux_input_r1] = handle_nav_key_released,
            },
        .hold_handler = {
            [mux_input_a] = handle_a_hold,
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_dpad_left] = handle_dpad_left_hold,
            [mux_input_dpad_right] = handle_dpad_right_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
            [mux_input_r2] = handle_random_select,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    transition_box_art_destroy();
    video_preview_destroy();
    free_items(&items, &item_count);

    return exit_status;
}
