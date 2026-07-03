#include "muxshare.h"
#include "ui/ui_muxcollect.h"

static lv_obj_t *ui_img_splash;
static lv_obj_t *ui_viewport_objects[7];

static char prev_dir[MAX_BUFFER_SIZE];
static char new_dir[MAX_BUFFER_SIZE];

static int exit_status = 0;
static int add_mode = 0;
static int sys_index = -1;
static int file_count = 0;
static int dir_count = 0;
static int file_cap = 0;
static int dir_cap = 0;
static int starter_image = 0;
static int splash_valid = 0;

static char current_meta_text[MAX_BUFFER_SIZE];
static char current_content_label[MAX_BUFFER_SIZE];

char *access_mode = NULL;

static char *load_content_description(void) {
    char *item_dir = get_content_path(items[current_item_index].extra_data);

    char item_file_name_buf[MAX_BUFFER_SIZE];
    snprintf(item_file_name_buf, sizeof(item_file_name_buf), "%s", get_last_dir(items[current_item_index].extra_data));

    char core_desc[MAX_BUFFER_SIZE];
    get_catalogue_name(item_dir, item_file_name_buf, core_desc, sizeof(core_desc));

    if (strlen(core_desc) <= 1 && items[current_item_index].content_type == ITEM) return lang.generic.no_info;

    const char *h_core_artwork;
    const char *h_file_name_ptr;
    char item_no_ext[MAX_BUFFER_SIZE];

    if (items[current_item_index].content_type == FOLDER) {
        h_core_artwork = "Collection";
        h_file_name_ptr = items[current_item_index].name;
    } else {
        snprintf(item_no_ext, sizeof(item_no_ext), "%s", item_file_name_buf);
        char *dot = strrchr(item_no_ext, '.');
        if (dot) *dot = '\0';

        h_core_artwork = core_desc;
        h_file_name_ptr = item_no_ext;
    }

    char content_desc[MAX_BUFFER_SIZE];
    snprintf(content_desc, sizeof(content_desc), INFO_CAT_PATH "/%s/text/%s.txt", h_core_artwork, h_file_name_ptr);

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

    char core_desc[MAX_BUFFER_SIZE];
    get_catalogue_name(item_dir, item_file_name_buf, core_desc, sizeof(core_desc));

    char *h_core_artwork;
    char *h_file_name_ptr;

    char item_no_ext[MAX_BUFFER_SIZE];

    if (items[current_item_index].content_type == FOLDER) {
        h_core_artwork = "Collection";
        h_file_name_ptr = items[current_item_index].name;
    } else {
        snprintf(item_no_ext, sizeof(item_no_ext), "%s", item_file_name_buf);
        char *dot = strrchr(item_no_ext, '.');
        if (dot) *dot = '\0';

        h_core_artwork = core_desc;
        h_file_name_ptr = item_no_ext;
    }

    render_image_refresh(
        image_type, h_core_artwork, h_file_name_ptr, ui_img_splash, ui_viewport_objects, &starter_image, &splash_valid
    );
}

static void video_refresh(void) {
    if (!ui_count_static || items[current_item_index].content_type == FOLDER) return;

    char *item_dir = get_content_path(items[current_item_index].extra_data);

    char item_file_name_buf[MAX_BUFFER_SIZE];
    snprintf(item_file_name_buf, sizeof(item_file_name_buf), "%s", get_last_dir(items[current_item_index].extra_data));

    char core_desc[MAX_BUFFER_SIZE];
    get_catalogue_name(item_dir, item_file_name_buf, core_desc, sizeof(core_desc));

    char item_no_ext[MAX_BUFFER_SIZE];
    snprintf(item_no_ext, sizeof(item_no_ext), "%s", item_file_name_buf);
    char *dot = strrchr(item_no_ext, '.');
    if (dot) *dot = '\0';

    char mux_dim[MAX_BUFFER_SIZE];
    snprintf(mux_dim, sizeof(mux_dim), "%dx%d", device.mux.width, device.mux.height);

    char vpath[MAX_BUFFER_SIZE];
    if (!load_video_catalogue(core_desc, item_no_ext, item_no_ext, mux_dim, vpath, sizeof(vpath))) return;

    const int delay_ms = config.visual.video_preview == 3 ? 10000 : config.visual.video_preview == 2 ? 5000 : 3000;
    video_preview_arm(vpath, delay_ms, ui_pnl_box, ui_img_box);
}

static void image_refresh_transition(void) {
    image_refresh("box");
    transition_box_art_apply_in(config.visual.box_art_transition);
    if (config.visual.video_preview > 0) video_refresh();
}

static void add_directory_and_file_names(const char *base_dir, char ***dir_names, char ***file_names) {
    DIR *dir = opendir(base_dir);

    if (!dir) {
        LOG_ERROR(mux_module, "%s", lang.system.fail_dir_open);
        return;
    }

    struct dirent *entry;
    file_count = 0;
    dir_count = 0;
    file_cap = 0;
    dir_cap = 0;

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
                if (S_ISDIR(st.st_mode)) {
                    type = DT_DIR;
                } else if (S_ISREG(st.st_mode)) {
                    type = DT_REG;
                }
            }
        }

        if (type == DT_DIR) {
            if (!at_base(sys_dir, access_mode)) continue;
            if (strcmp(name, "kiosk") == 0) continue;

            if (dir_count >= dir_cap) {
                const int new_cap = dir_cap ? dir_cap * 2 : 16;
                char **tmp = realloc(*dir_names, (size_t) new_cap * sizeof(char *));

                if (!tmp) continue;

                *dir_names = tmp;
                dir_cap = new_cap;
            }

            (*dir_names)[dir_count++] = strdup(name);

        } else if (type == DT_REG) {
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
    }

    closedir(dir);
}

static void gen_item(const int file_count, char **file_names) {
    gen_item_from_files(sys_dir, file_count, file_names);

    sort_items(items, item_count);
}

static void create_collection_items(void) {
    char **dir_names = NULL;
    char **file_names = NULL;

    add_directory_and_file_names(sys_dir, &dir_names, &file_names);

    int fn_valid = 0;
    struct json fn_json = {0};
    char *fn_json_buf = NULL;

    turbo_time(1, 1);

    if (config.visual.friendly_folder) {
        const char *folder_name_file = resolve_info_path("name/folder.json");

        if (folder_name_file) {
            char *file_content = read_all_char_from(folder_name_file);

            if (file_content && json_valid(file_content)) {
                fn_valid = 1;

                fn_json_buf = strdup(file_content);
                fn_json = json_parse(fn_json_buf);

                LOG_SUCCESS(mux_module, "Using Friendly Folder: %s", folder_name_file);
            } else {
                LOG_WARN(mux_module, "Invalid Friendly Folder: %s", folder_name_file);
            }

            free(file_content);
        } else {
            LOG_WARN(mux_module, "Friendly Folder does not exist: %s", folder_name_file);
        }
    }

    const char *col_path = is_ksk(kiosk.collect.access) && dir_exist(INFO_CKS_PATH) ? INFO_CKS_PATH : INFO_COL_PATH;
    update_title(sys_dir, fn_valid, fn_json, lang.muxcollect.title, col_path);

    if (dir_count > 0 || file_count > 0) {
        if (at_base(sys_dir, access_mode)) {
            for (int i = 0; i < dir_count; i++) {
                char *friendly_folder_name = get_friendly_folder_name(dir_names[i], fn_valid, fn_json);

                char collection_dir[MAX_BUFFER_SIZE];
                snprintf(collection_dir, sizeof(collection_dir), "%s/%s", sys_dir, dir_names[i]);

                content_item *new_item =
                    add_item(&items, &item_count, dir_names[i], friendly_folder_name, collection_dir, FOLDER);
                adjust_visual_label(new_item->display_name, config.visual.name, config.visual.dash);

                if (config.visual.folder_item_count) {
                    char display_name[MAX_BUFFER_SIZE];
                    snprintf(
                        display_name, sizeof(display_name), "%s (%d)", new_item->display_name,
                        get_directory_item_count(sys_dir, new_item->name, 0)
                    );
                    new_item->display_name = strdup(display_name);
                }

                free(dir_names[i]);
                free(friendly_folder_name);
            }
        }

        sort_items(items, item_count);

        grid_mode_enabled =
            !disable_grid_file_exists(sys_dir) && theme.grid.enabled
            && ((file_count > 0 && config.visual.grid_mode_content) || (dir_count > 0 && file_count == 0));

        gen_item(file_count, file_names);

        if (grid_mode_enabled) {
            init_grid_dynamic(prev_dir, &sys_index);
        } else {
            const size_t limit = theme.mux.item.count;
            for (size_t i = 0; i < item_count && i < limit; i++) {
                gen_label(mux_module, items[i].content_type == FOLDER ? "folder" : "collection", items[i].display_name);
            }

            for (size_t i = 0; i < item_count; i++) {
                if (items[i].content_type == FOLDER && strcasecmp(items[i].extra_data, prev_dir) == 0)
                    sys_index = (int) i;
            }
        }

        if (ui_count_static > 0) lv_obj_update_layout(ui_pnl_content);

        free(file_names);
        free(dir_names);
    }

    free(fn_json_buf);
    fn_json_buf = NULL;

    turbo_time(0, 1);
}

static void update_footer_glyph(void) {
    if (!add_mode) return;
    lv_label_set_text(
        ui_lbl_nav_a,
        item_count > 0 && items[current_item_index].content_type == FOLDER ? lang.generic.open : lang.generic.add
    );
}

static void update_list_item(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, const int index) {
    lv_label_set_text(ui_lbl_item, items[index].display_name);

    char glyph_image_embed[MAX_BUFFER_SIZE];
    if (config.visual.list_glyph && theme.list_default.glyph_alpha > 0 && theme.list_focus.glyph_alpha > 0) {
        get_glyph_path(
            mux_module, items[index].content_type == FOLDER ? "folder" : "collection", glyph_image_embed,
            MAX_BUFFER_SIZE
        );
        set_list_glyph_image(ui_lbl_item_glyph, glyph_image_embed);
    }

    apply_size_to_content(&theme, ui_pnl_content, ui_lbl_item, ui_lbl_item_glyph, items[index].display_name);
    apply_text_long_dot(&theme, ui_lbl_item);
}

static void update_list_items(const int start_index) {
    const int max = (int) item_count - start_index;
    if (max <= 0) return;

    int count = theme.mux.item.count;
    if (count > max) count = max;

    for (int index = 0; index < count; ++index) {
        const lv_obj_t *panel_item = lv_obj_get_child(ui_pnl_content, index);
        update_list_item(lv_obj_get_child(panel_item, 0), lv_obj_get_child(panel_item, 1), start_index + index);
    }
}

static void focus_group(const int index) {
    if (index < 0 || index >= theme.mux.item.count) return;
    lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, index);

    if (!panel) return;

    lv_group_focus_obj(panel);
    lv_group_focus_obj(lv_obj_get_child(panel, 0));
    lv_group_focus_obj(lv_obj_get_child(panel, 1));
}

static int focus_list_index(void) {
    const int before = (theme.mux.item.count - theme.mux.item.count % 2) / 2;
    const int after = (theme.mux.item.count - 1) / 2;

    if (current_item_index < before) return current_item_index;
    if (current_item_index >= (int) item_count - after)
        return theme.mux.item.count - ((int) item_count - current_item_index);

    return before;
}

static void move_index(const int direction) {
    if (direction < 0) {
        current_item_index = current_item_index == 0 ? ui_count_static - 1 : current_item_index - 1;
    } else {
        current_item_index = current_item_index == ui_count_static - 1 ? 0 : current_item_index + 1;
    }
}

static void focus_initial(void) {
    const int count = theme.mux.item.count;

    if ((int) item_count <= count) {
        focus_group(current_item_index);
    } else {
        const int before = (count - count % 2) / 2;
        const int after = (count - 1) / 2;

        int start_index;
        if (current_item_index < before) {
            start_index = 0;
        } else if (current_item_index >= (int) item_count - after) {
            start_index = (int) item_count - count;
        } else {
            start_index = current_item_index - before;
        }

        update_list_items(start_index);

        int new_item_index = current_item_index - start_index;
        if (new_item_index < 0) new_item_index = 0;
        if (new_item_index >= count) new_item_index = count - 1;

        focus_group(new_item_index);
    }

    set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);
    lv_label_set_text(ui_lbl_grid_current_item, items[current_item_index].display_name);

    if (config.visual.box_art < 4) {
        image_refresh("box");
        if (config.visual.video_preview > 0) video_refresh();
    }

    update_footer_glyph();
}

static void list_nav_move(const int steps, const int direction) {
    if (!ui_count_static) return;
    first_open ? (first_open = 0) : play_sound(snd_navigate);

    const int visible_count = theme.mux.item.count;
    const int static_list = !grid_mode_enabled && (int) item_count <= visible_count;
    const int multi_list = !grid_mode_enabled && (int) item_count > visible_count;

    if (!grid_mode_enabled) apply_text_long_dot(&theme, lv_group_get_focused(ui_group));

    if (static_list) {
        for (int step = 0; step < steps; ++step) {
            move_index(direction);
        }
        focus_group(current_item_index);
    } else {
        for (int step = 0; step < steps; ++step) {
            move_index(direction);

            if (!is_carousel_grid_mode()) {
                nav_move(ui_group, direction);
                nav_move(ui_group_glyph, direction);
                nav_move(ui_group_panel, direction);
            }

            if (multi_list) {
                update_windowed_list(
                    ui_pnl_content, direction, current_item_index, (int) item_count, visible_count, update_list_item,
                    update_list_items
                );
            } else if (grid_mode_enabled) {
                update_grid(direction);
            }

            if (multi_list) focus_group(focus_list_index());
        }
    }

    if (!grid_mode_enabled) set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);
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

    update_footer_glyph();
    nav_moved = 1;
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

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

static void do_remove(void) {
    if (items[current_item_index].content_type == FOLDER) {
        if (get_directory_item_count(sys_dir, items[current_item_index].name, 0) > 0) {
            play_sound(snd_error);
            toast_message(lang.muxcollect.error.remove_dir, tst_wait_s);
            return;
        }

        char collection_dir[MAX_BUFFER_SIZE];
        snprintf(collection_dir, sizeof(collection_dir), "%s/%s", sys_dir, items[current_item_index].name);

        if (!dir_exist(collection_dir)) {
            play_sound(snd_error);
            toast_message(lang.muxcollect.error.remove_dir, tst_wait_s);
            return;
        }

        write_text_to_file(MUOS_IDX_LOAD, "w", INT, get_index_on_delete(current_item_index, ui_count_static - 1));
        play_sound(snd_muos);
        remove(collection_dir);
        mark_collection_dirty();
    } else {
        char coll_name_no_ext[MAX_BUFFER_SIZE];
        snprintf(coll_name_no_ext, sizeof(coll_name_no_ext), "%s", items[current_item_index].name);

        char *coll_dot = strrchr(coll_name_no_ext, '.');
        if (coll_dot) *coll_dot = '\0';

        char collection_file[MAX_BUFFER_SIZE];
        snprintf(collection_file, sizeof(collection_file), "%s/%s.cfg", sys_dir, coll_name_no_ext);

        if (!file_exist(collection_file)) {
            play_sound(snd_error);
            toast_message(lang.muxcollect.error.remove_file, tst_wait_s);
            return;
        }

        write_text_to_file(MUOS_IDX_LOAD, "w", INT, get_index_on_delete(current_item_index, ui_count_static - 1));
        play_sound(snd_muos);
        remove(collection_file);
        mark_collection_dirty();
    }

    load_mux("collection");
    mux_input_stop();
}

static void handle_keyboard_ok_press(void) {
    key_show = 0;

    snprintf(new_dir, sizeof(new_dir), "%s/%s", sys_dir, lv_textarea_get_text(ui_txt_entry_collect));
    create_directories(new_dir, 0);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, lv_textarea_get_text(ui_txt_entry_collect));
    load_mux("collection");

    skip_confirm = 0;
    mux_input_stop();
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

static void add_collection_item(void) {
    char *base_file_name = read_line_char_from(ADD_MODE_WORK, 1);
    const char *cache_file = read_line_char_from(ADD_MODE_WORK, 2);
    char *system_sub = read_line_char_from(ADD_MODE_WORK, 3);
    char full_file_path[MAX_BUFFER_SIZE];
    snprintf(
        full_file_path, sizeof(full_file_path), "%s%s/%s", read_line_char_from(cache_file, content_mount),
        read_line_char_from(cache_file, content_dir), base_file_name
    );

    char collection_content[MAX_BUFFER_SIZE];
    snprintf(
        collection_content, sizeof(collection_content), "%s\n%s\n%s", full_file_path, system_sub,
        strip_ext(base_file_name)
    );

    char collection_file[MAX_BUFFER_SIZE];
    snprintf(
        collection_file, sizeof(collection_file), "%s/%s-%08X.cfg", sys_dir, strip_ext(base_file_name),
        fnv_hash_str(full_file_path)
    );

    write_text_to_file(collection_file, "w", CHAR, collection_content);
    mark_collection_dirty();

    if (file_exist(ADD_MODE_WORK)) remove(ADD_MODE_WORK);

    char done_content[MAX_BUFFER_SIZE];
    snprintf(done_content, sizeof(done_content), "DONE\n%s", collection_file);
    write_text_to_file(ADD_MODE_DONE, "w", CHAR, done_content);

    if (file_exist(COLLECTION_DIR)) remove(COLLECTION_DIR);
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
    if (key_show) {
        handle_keyboard_press();
        return;
    }

    if (hold_call || (!add_mode && !ui_count_static)) return;

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

    int load_message = 0;

    if (add_mode && !ui_count_static) {
        add_collection_item();
        goto load_end;
    }

    if (items[current_item_index].content_type == FOLDER) {
        play_sound(snd_confirm);
        load_message = 1;

        char n_dir[MAX_BUFFER_SIZE];
        snprintf(n_dir, sizeof(n_dir), "%s/%s", sys_dir, items[current_item_index].name);

        write_text_to_file(COLLECTION_DIR, "w", CHAR, n_dir);
    } else {
        if (add_mode) {
            play_sound(snd_confirm);
            add_collection_item();
            goto load_end;
        }

        write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

        const char *launch_path = items[current_item_index].extra_data;
        char resolved_path[PATH_MAX];

        if (!union_resolve_to_real(launch_path, resolved_path, sizeof(resolved_path)) || !file_exist(resolved_path)) {
            play_sound(snd_error);
            toast_message(lang.generic.no_load, tst_wait_l);

            LOG_ERROR(mux_module, "Could not launch content: %s", resolved_path[0] ? resolved_path : launch_path);
            return;
        }

        if (strcmp(launch_path, resolved_path) != 0) {
            char collection_file[MAX_BUFFER_SIZE];
            snprintf(collection_file, sizeof(collection_file), "%s/%s", sys_dir, items[current_item_index].name);

            if (file_exist(collection_file)) rewrite_launch_file(collection_file, resolved_path);
            snprintf(items[current_item_index].extra_data, MAX_BUFFER_SIZE, "%s", resolved_path);
        }

        play_sound(snd_confirm);

        if (load_content(0, resolved_path)) {
            if (strcasecmp(passcode.code.launch, "000000") != 0) {
                int result = 0;

                while (result != 1) {
                    result = muxpass_main(pct_launch);

                    switch (result) {
                        case 1:
                            show_splash();
                            fade_out_screen();
                            exit_status = 1;
                            break;
                        case 2:
                        default:
                            if (file_exist(MUOS_ROM_LOAD)) remove(MUOS_ROM_LOAD);
                            if (file_exist(MUOS_CON_LOAD)) remove(MUOS_CON_LOAD);
                            if (file_exist(MUOS_GOV_LOAD)) remove(MUOS_GOV_LOAD);
                            if (file_exist(MUOS_RAC_LOAD)) remove(MUOS_RAC_LOAD);
                            if (file_exist(MUOS_FLT_LOAD)) remove(MUOS_FLT_LOAD);
                            if (file_exist(MUOS_SHD_LOAD)) remove(MUOS_SHD_LOAD);

                            write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);
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
            write_text_to_file(MUOS_ASS_FROM, "w", CHAR, "collection");
            load_mux("assign");
        }
    }

    if (from_start) write_text_to_file(MANUAL_RA_LOAD, "w", INT, 1);
    if (load_message) toast_message(lang.generic.loading, tst_wait_f);

load_end:
    if (file_exist(ADD_MODE_DONE)) {
        load_mux(read_all_char_from(ADD_MODE_FROM));
    } else {
        load_mux("collection");
    }

    skip_confirm = 0;
    mux_input_stop();
}

static void handle_a(void) {
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
    if (hold_call) return;
    video_preview_cancel();
    process_load(launch_flag(config.visual.launch_swap, 0));
}

static void handle_a_hold(void) {
    if (msgbox_active || hold_call) return;
    video_preview_cancel();
    process_load(launch_flag(config.visual.launch_swap, 1));
}

static void handle_b(void) {
    if (key_show) {
        key_backspace(ui_txt_entry_collect);
        return;
    }

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

    if (at_base(sys_dir, access_mode)) {
        if (file_exist(ADD_MODE_WORK)) remove(ADD_MODE_WORK);
        if (add_mode) write_text_to_file(ADD_MODE_DONE, "w", CHAR, "CANCEL");
        if (file_exist(COLLECTION_DIR)) remove(COLLECTION_DIR);
    } else {
        const char *base_dir = strrchr(sys_dir, '/');
        if (base_dir) write_text_to_file(COLLECTION_DIR, "w", CHAR, strndup(sys_dir, base_dir - sys_dir));
    }

    if (file_exist(COLLECTION_DIR)) {
        load_mux("collection");
    } else {
        if (file_exist(ADD_MODE_DONE)) {
            load_mux(read_all_char_from(ADD_MODE_FROM));
            remove(ADD_MODE_FROM);
        } else {
            load_mux("launcher");
            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "collection");
        }
    }

    skip_confirm = 0;
    mux_input_stop();
}

static void handle_b_hold(void) {
    if (key_show) key_backspace(ui_txt_entry_collect);
}

static void handle_x(void) {
    if (key_show) {
        close_osk(key_entry, ui_group, ui_txt_entry_collect, ui_pnl_entry_collect);
        return;
    }

    if (msgbox_active || !ui_count_static || add_mode || is_ksk(kiosk.collect.remove) || remove_mode
        || video_preview_active())
        return;

    if (config.settings.advanced.trust_remove || skip_confirm) {
        do_remove();
        return;
    }

    play_sound(snd_confirm);
    show_remove_dialog();
}

static void handle_y(void) {
    if (key_show) {
        key_space(ui_txt_entry_collect);
        return;
    }

    if (msgbox_active || hold_call || video_preview_active()) return;

    if (!is_ksk(kiosk.collect.new_dir) && at_base(sys_dir, access_mode)) {
        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

        key_show = 1;
        osk_show(ui_pnl_entry_collect);
        lv_textarea_set_text(ui_txt_entry_collect, "");
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

    if (grid_mode_enabled) {
        current_item_index = target;
        update_grid_items(1);
        list_nav_next(0);
    } else {
        list_nav_move(target, dir);
    }
}

static void handle_up(void) {
    if (key_show) {
        key_up();
        return;
    }

    if (remove_mode) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_up_hold(void) {
    if (key_show) {
        key_up();
        return;
    }

    if (remove_mode) return;

    handle_list_nav_up_hold();
}

static void handle_down(void) {
    if (key_show) {
        key_down();
        return;
    }

    if (remove_mode) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_down();
}

static void handle_down_hold(void) {
    if (key_show) {
        key_down();
        return;
    }

    if (remove_mode) return;

    handle_list_nav_down_hold();
}

static void handle_left(void) {
    if (key_show) {
        key_left();
        return;
    }

    if (remove_mode) {
        if (swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_left();
}

static void handle_right(void) {
    if (key_show) {
        key_right();
        return;
    }

    if (remove_mode) {
        if (swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_right();
}

static void handle_left_hold(void) {
    if (key_show) {
        key_left();
        return;
    }

    if (remove_mode) return;

    handle_list_nav_left_hold();
}

static void handle_right_hold(void) {
    if (key_show) {
        key_right();
        return;
    }

    if (remove_mode) return;

    handle_list_nav_right_hold();
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

static void handle_nav_key_released(void) {
    if (config.visual.box_art_transition != TSN_DISABLED) {
        transition_box_art_key_released();
    }
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {ui_pnl_footer, ui_pnl_header, ui_lbl_counter_collect, ui_pnl_help,
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

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.open, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.remove, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, lang.generic.new, 0},
                                  {ui_lbl_nav_menu_glyph, "", 0},
                                  {ui_lbl_nav_menu, lang.generic.info, 0},
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

        update_file_counter(ui_lbl_counter_collect, file_count);
        if (overlay_image) lv_obj_move_foreground(overlay_image);

        nav_moved = 0;
    }
}

static void on_key_event(const struct input_event ev) {
    if (ev.code == KEY_ENTER && ev.value == 1) {
        handle_keyboard_ok_press();
    }

    if (ev.code == KEY_ESC && ev.value == 1) {
        handle_b();
    } else {
        process_key_event(&ev, ui_txt_entry_collect);
    }
}

int muxcollect_main(const int add, const char *dir, const int last_index) {
    exit_status = 0;
    add_mode = add;
    sys_index = last_index;
    file_count = 0;
    dir_count = 0;
    starter_image = 0;
    splash_valid = 0;

    const char *collection_path =
        is_ksk(kiosk.collect.access) && dir_exist(INFO_CKS_PATH) ? INFO_CKS_PATH : INFO_COL_PATH;

    snprintf(sys_dir, sizeof(sys_dir), "%s", strcmp(dir, "") == 0 ? collection_path : dir);
    access_mode = strcasecmp(collection_path, INFO_CKS_PATH) ? "collection" : "kiosk";

    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, "");
    init_muxcollect(ui_screen, &theme);

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

    snprintf(prev_dir, sizeof(prev_dir), "%s", file_exist(MUOS_PDI_LOAD) ? read_all_char_from(MUOS_PDI_LOAD) : "");

    create_collection_items();
    init_elements();

    ui_count_static = (int) item_count;

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, sys_dir);

    transition_box_art_init(image_refresh_transition);

    if (ui_count_static > 0) {
        if (sys_index > -1 && sys_index <= ui_count_static && current_item_index < ui_count_static) {
            if (grid_mode_enabled) {
                list_nav_move(sys_index, +1);
            } else {
                current_item_index = sys_index;
                focus_initial();
            }
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
        lv_label_set_text(ui_lbl_screen_message, lang.muxcollect.none);
        clear_box_image();
    }

    struct nav_flag nav_e[] = {{ui_lbl_nav_a, 1},       {ui_lbl_nav_a_glyph, 1},   {ui_lbl_nav_x, 1},
                               {ui_lbl_nav_x_glyph, 1}, {ui_lbl_nav_y, 1},         {ui_lbl_nav_y_glyph, 1},
                               {ui_lbl_nav_menu, 1},    {ui_lbl_nav_menu_glyph, 1}};

    /* this should make it somewhat easier to reference hidden navs... */
    enum { nav_a, nav_a_glyph, nav_x, nav_x_glyph, nav_y, nav_y_glyph, nav_menu, nav_menu_glyph };

    const struct {
        int add_mode;
        int at_collect;
        int ui_count_static;
        const int *hidden;
        size_t count;
    } nav_rules[] = {
        {1, 1, 0, (int[]) {nav_x, nav_x_glyph, nav_menu, nav_menu_glyph}, 4},
        {1, 1, 1, (int[]) {nav_x, nav_x_glyph}, 2},
        {1, 0, 0, (int[]) {nav_x, nav_x_glyph, nav_y, nav_y_glyph, nav_menu, nav_menu_glyph}, 6},
        {1, 0, 1, (int[]) {nav_x, nav_x_glyph, nav_y, nav_y_glyph}, 4},
        {0, 1, 0, (int[]) {nav_a, nav_a_glyph, nav_x, nav_x_glyph, nav_menu, nav_menu_glyph}, 6},
        {0, 0, 0, (int[]) {nav_a, nav_a_glyph, nav_x, nav_x_glyph, nav_y, nav_y_glyph, nav_menu, nav_menu_glyph}, 8},
        {0, 0, 1, (int[]) {nav_y, nav_y_glyph}, 2},
    };

    const int *hidden = NULL;
    size_t hidden_count = 0;

    for (size_t i = 0; i < A_SIZE(nav_rules); ++i) {
        if (nav_rules[i].add_mode == add_mode && nav_rules[i].at_collect == at_base(sys_dir, access_mode)
            && nav_rules[i].ui_count_static == !!ui_count_static) {
            hidden = nav_rules[i].hidden;
            hidden_count = nav_rules[i].count;
            break;
        }
    }

    if (hidden != NULL) {
        for (int i = 0; i < hidden_count; ++i) {
            const int k = hidden[i];
            if (k >= 0 && k < (int) A_SIZE(nav_e)) nav_e[k].visible = 0;
        }
    }

    set_nav_flags(nav_e, A_SIZE(nav_e));
    adjust_panels();

    if (is_ksk(kiosk.collect.remove)) {
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    if (is_ksk(kiosk.collect.new_dir)) {
        lv_obj_add_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    dialogue_init_remove(&remove_dlg, &theme, ui_screen, NULL, lang.generic.select, lang.generic.back);
    update_file_counter(ui_lbl_counter_collect, file_count);
    init_osk(ui_pnl_entry_collect, ui_txt_entry_collect, 0, 0, OSK_MAX);

    update_footer_glyph();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1
                     || (grid_mode_enabled && theme.grid.navigation_type >= 1 && theme.grid.navigation_type <= 5),
        .press_handler =
            {
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_dpad_up] = handle_up,
                [mux_input_dpad_down] = handle_down,
                [mux_input_dpad_left] = handle_left,
                [mux_input_dpad_right] = handle_right,
                [mux_input_l1] = handle_l1,
                [mux_input_r1] = handle_r1,
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
            [mux_input_b] = handle_b_hold,
            [mux_input_dpad_up] = handle_up_hold,
            [mux_input_dpad_down] = handle_down_hold,
            [mux_input_dpad_left] = handle_left_hold,
            [mux_input_dpad_right] = handle_right_hold,
            [mux_input_l1] = handle_l1,
            [mux_input_r1] = handle_r1,
            [mux_input_r2] = handle_random_select,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    register_key_event_callback(on_key_event);
    mux_input_task(&input_opts);

    transition_box_art_destroy();
    video_preview_destroy();
    free_items(&items, &item_count);

    return exit_status;
}
