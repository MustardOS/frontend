#include "muxshare.h"
#include "../common/skip_list.h"

static lv_obj_t *ui_imgSplash;
static lv_obj_t *ui_viewport_objects[7];

static char prev_dir[PATH_MAX];

static int exit_status = 0;
static int sys_index = -1;
static int file_count = 0;
static int dir_count = 0;
static int starter_image = 0;
static int splash_valid = 0;

static char current_meta_text[MAX_BUFFER_SIZE];
static char current_content_label[MAX_BUFFER_SIZE];

static void assign_item_buckets(void) {
    for (size_t i = 0; i < item_count; i++) {
        content_item *it = &items[i];
        it->sort_bucket = BUCKET_NORMAL;
        it->group_tag[0] = '\0';

        if (config.VISUAL.PINNEDCOLLECT && is_in_list(collection_items, collection_item_count, sys_dir, it->name)) {
            it->sort_bucket = BUCKET_COLLECT;
            continue;
        }

        if (config.VISUAL.GROUPTAGS > 0 && it->use_module && strcasecmp(it->use_module, "muxtag") == 0) {
            it->sort_bucket = BUCKET_TAGGED;

            if (config.VISUAL.GROUPTAGS == 1) {
                char c = toupper((unsigned char) it->display_name[0]);
                it->group_tag[0] = isalnum(c) ? c : '#';
                it->group_tag[1] = '\0';
                continue;
            } else if (config.VISUAL.GROUPTAGS == 2) {
                snprintf(it->group_tag, sizeof(it->group_tag), "%s", it->glyph_icon);
                continue;
            }
        }

        if (config.VISUAL.DROPHISTORY && is_in_list(history_items, history_item_count, sys_dir, it->name)) {
            it->sort_bucket = BUCKET_HISTORY;
            continue;
        }
    }
}

static char *load_content_description(void) {
    char content_desc[MAX_BUFFER_SIZE];

    char *content_label = items[current_item_index].name;
    char *desc_name = strip_ext(items[current_item_index].name);

    char core_desc[MAX_BUFFER_SIZE];
    get_catalogue_name(sys_dir, content_label, core_desc, sizeof(core_desc));

    if (strlen(core_desc) <= 1 && items[current_item_index].content_type == ITEM) return lang.GENERIC.NO_INFO;

    if (items[current_item_index].content_type == FOLDER) {
        snprintf(content_desc, sizeof(content_desc), "%s/Folder/text/%s.txt",
                 INFO_CAT_PATH, content_label);
        if (!file_exist(content_desc)) {
            char *catalogue_name = get_catalogue_name_from_rom_path(sys_dir, items[current_item_index].name);
            snprintf(content_desc, sizeof(content_desc), "%s/Folder/text/%s.txt",
                     INFO_CAT_PATH, catalogue_name);
            LOG_INFO(mux_module, "Falling back to catalogue name for content description '%s'", catalogue_name);
        }
    } else {
        snprintf(content_desc, sizeof(content_desc), "%s/%s/text/%s.txt",
                 INFO_CAT_PATH, core_desc, desc_name);
    }

    if (file_exist(content_desc)) {
        return read_all_char_from(content_desc);
    }

    snprintf(current_meta_text, sizeof(current_meta_text), " ");
    return lang.GENERIC.NO_INFO;
}

static void image_refresh(char *image_type) {
    if (strcasecmp(image_type, "box") == 0 && config.VISUAL.BOX_ART == 8) return;

    char image[MAX_BUFFER_SIZE];
    char image_path[MAX_BUFFER_SIZE];
    char core_artwork[MAX_BUFFER_SIZE];

    char *content_label = items[current_item_index].name;

    if (strcasecmp(get_last_subdir(sys_dir, '/', 4), strip_dir(UNION_ROM_PATH)) == 0) {
        snprintf(image, sizeof(image), "%s/Folder/%s/%s.png",
                 INFO_CAT_PATH, image_type, content_label);
    } else {
        char *file_name = strip_ext(items[current_item_index].name);

        get_catalogue_name(sys_dir, content_label, core_artwork, sizeof(core_artwork));

        if (strlen(core_artwork) <= 1 && items[current_item_index].content_type == ITEM) {
            snprintf(image, sizeof(image), "%s/%simage/none_%s.png",
                     theme_base, mux_dimension, image_type);
            if (!file_exist(image)) {
                snprintf(image, sizeof(image), "%s/image/none_%s.png",
                         theme_base, image_type);
            }
        } else {
            if (strcasecmp(image_type, "box") != 0 || !grid_mode_enabled || !config.VISUAL.BOX_ART_HIDE) {
                if (items[current_item_index].content_type == FOLDER) {
                    char *catalogue_name = get_catalogue_name_from_rom_path(sys_dir, items[current_item_index].name);
                    load_image_catalogue("Folder", file_name, catalogue_name, "default",
                                         mux_dimension, image_type, image, sizeof(image));
                } else {
                    load_image_catalogue(core_artwork, file_name, "", "default", mux_dimension,
                                         image_type, image, sizeof(image));
                }
            }
            if (strcasecmp(image_type, "splash") == 0 && !file_exist(image)) {
                load_splash_image_fallback(mux_dimension, image, sizeof(image));
            }
        }
    }

    LOG_INFO(mux_module, "Loading '%s' Artwork: %s", image_type, image);

    if (strcasecmp(image_type, "preview") == 0) {
        if (strcasecmp(preview_image_previous_path, image) != 0) {
            if (file_exist(image)) {
                struct ImageSettings image_settings = {
                        image, LV_ALIGN_CENTER,
                        validate_int16((int16_t) (device.MUX.WIDTH * .9) - 60, "width"),
                        validate_int16((int16_t) (device.MUX.HEIGHT * .9) - 120, "height"),
                        0, 0, 0, 0
                };
                update_image(ui_imgHelpPreviewImage, image_settings);
                snprintf(preview_image_previous_path, sizeof(preview_image_previous_path), "%s", image);
            } else {
                lv_img_set_src(ui_imgHelpPreviewImage, &ui_image_Nothing);
                snprintf(preview_image_previous_path, sizeof(preview_image_previous_path), " ");
            }
        }
    } else if (strcasecmp(image_type, "splash") == 0) {
        if (strcasecmp(splash_image_previous_path, image) != 0) {
            if (file_exist(image)) {
                splash_valid = 1;
                snprintf(image_path, sizeof(image_path), "M:%s", image);
                lv_img_set_src(ui_imgSplash, image_path);
                snprintf(splash_image_previous_path, sizeof(splash_image_previous_path), "%s", image);
            } else {
                splash_valid = 0;
                lv_img_set_src(ui_imgSplash, &ui_image_Nothing);
                snprintf(splash_image_previous_path, sizeof(splash_image_previous_path), " ");
            }
        }
    } else {
        if (strcasecmp(box_image_previous_path, image) != 0) {
            char *catalogue_folder = items[current_item_index].content_type == FOLDER ? "Folder" : core_artwork;
            char *content_name =
                    items[current_item_index].content_type == FOLDER ? items[current_item_index].name : strip_ext(
                            items[current_item_index].name);
            char artwork_config_path[MAX_BUFFER_SIZE];
            snprintf(artwork_config_path, sizeof(artwork_config_path), "%s/%s.ini",
                     INFO_CAT_PATH, catalogue_folder);
            if (!file_exist(artwork_config_path)) {
                snprintf(artwork_config_path, sizeof(artwork_config_path), "%s/default.ini",
                         INFO_CAT_PATH);
            }

            if (file_exist(artwork_config_path)) {
                viewport_refresh(ui_viewport_objects, artwork_config_path, catalogue_folder, content_name);
                snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", image);
            } else {
                if (file_exist(image)) {
                    starter_image = 1;
                    snprintf(image_path, sizeof(image_path), "M:%s", image);
                    lv_img_set_src(ui_imgBox, image_path);
                    snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", image);
                } else {
                    lv_img_set_src(ui_imgBox, &ui_image_Nothing);
                    snprintf(box_image_previous_path, sizeof(box_image_previous_path), " ");
                }
            }
        }
    }
}

static void add_directory_and_file_names(const char *base_dir, char ***dir_names, char ***file_names) {
    file_count = 0;
    dir_count = 0;
    struct dirent *entry;
    DIR *dir = opendir(base_dir);

    if (!dir) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_DIR_OPEN);
        return;
    }

    while ((entry = readdir(dir))) {
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, entry->d_name);
        if (entry->d_type == DT_DIR) {
            if (!should_skip(entry->d_name, 1)) {
                if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                    if (config.VISUAL.DISPLAYEMPTYFOLDER || get_directory_item_count(base_dir, entry->d_name, 1) != 0) {
                        char *subdir_path = (char *) malloc(strlen(entry->d_name) + 2);
                        snprintf(subdir_path, strlen(entry->d_name) + 2, "%s", entry->d_name);

                        *dir_names = (char **) realloc(*dir_names, (dir_count + 1) * sizeof(char *));
                        (*dir_names)[dir_count] = subdir_path;
                        (dir_count)++;
                    }
                }
            }
        } else if (entry->d_type == DT_REG) {
            if (!should_skip(entry->d_name, 0)) {
                char *file_path = (char *) malloc(strlen(entry->d_name) + 2);
                snprintf(file_path, strlen(entry->d_name) + 2, "%s", entry->d_name);

                *file_names = (char **) realloc(*file_names, (file_count + 1) * sizeof(char *));
                (*file_names)[file_count] = file_path;
                (file_count)++;
            }
        }
    }

    closedir(dir);
}

static void remove_match_items(const char *filter_name, int mode, char ***filter_list, int *filter_count,
                               void (*pop_func)(void), content_item **items, size_t *item_count, const char *sys_dir) {
    free_item_list(filter_list, filter_count);
    pop_func();

    if (mode != 2 || !*filter_list || *filter_count == 0) return;

    for (int c = 0; c < *filter_count; c++) {
        for (size_t i = 0; i < *item_count; i++) {
            char item_path[PATH_MAX];
            snprintf(item_path, sizeof(item_path), "%s/%s", sys_dir, (*items)[i].name);

            if (strcasecmp(item_path, (*filter_list)[c]) == 0) {
                LOG_DEBUG(mux_module, "Skipping %s Item: %s", filter_name, item_path);
                remove_item(items, item_count, i);
                i--;
            }
        }
    }
}

static void gen_item(char **file_names, int file_count) {
    char init_meta_dir[MAX_BUFFER_SIZE];
    char *sub_path = sys_dir;

    if (strncasecmp(sys_dir, UNION_ROM_PATH, strlen(UNION_ROM_PATH)) == 0) {
        sub_path = sys_dir + strlen(UNION_ROM_PATH);
        while (*sub_path == '/') sub_path++;
    }

    snprintf(init_meta_dir, sizeof(init_meta_dir), INFO_COR_PATH "/%s/",
             sub_path);
    create_directories(init_meta_dir, 0);

    const char *last_dir = str_tolower(get_last_dir(sub_path));
    if (strlen(last_dir) < 1) last_dir = str_tolower(sub_path);

    char custom_lookup[MAX_BUFFER_SIZE];
    snprintf(custom_lookup, sizeof(custom_lookup), INFO_NAM_PATH "/%s.json", last_dir);
    if (!file_exist(custom_lookup)) snprintf(custom_lookup, sizeof(custom_lookup), INFO_NAM_PATH "/global.json");

    int fn_valid = 0;
    struct json fn_json;

    if (file_exist(custom_lookup)) {
        char *lookup_content = read_all_char_from(custom_lookup);

        if (lookup_content && json_valid(lookup_content)) {
            fn_valid = 1;
            fn_json = json_parse(read_all_char_from(custom_lookup));
            LOG_SUCCESS(mux_module, "Using Friendly Name: %s", custom_lookup);
        } else {
            LOG_WARN(mux_module, "Invalid Friendly Name: %s", custom_lookup);
        }

        free(lookup_content);
    } else {
        LOG_WARN(mux_module, "Friendly Name does not exist: %s", custom_lookup);
    }

    SkipList skiplist;
    init_skiplist(&skiplist);
    for (int i = 0; i < file_count; i++) {
        if (ends_with(file_names[i], ".cue")) {
            process_cue_file(sys_dir, file_names[i], &skiplist);
        } else if (ends_with(file_names[i], ".gdi")) {
            process_gdi_file(sys_dir, file_names[i], &skiplist);
        } else if (ends_with(file_names[i], ".m3u")) {
            process_m3u_file(sys_dir, file_names[i], &skiplist);
        }
    }

    for (int i = 0; i < file_count; i++) {
        char full_path[MAX_BUFFER_SIZE];
        snprintf(full_path, sizeof(full_path), "%s/%s", sys_dir, file_names[i]);
        if (!in_skiplist(&skiplist, full_path)) {
            int has_custom_name = 0;
            char fn_name[MAX_BUFFER_SIZE];
            char *stripped_name = strip_ext(file_names[i]);

            if (fn_valid) {
                struct json custom_lookup_json = json_object_get(fn_json, str_tolower(stripped_name));
                if (json_exists(custom_lookup_json)) {
                    json_string_copy(custom_lookup_json, fn_name, sizeof(fn_name));
                    has_custom_name = 1;
                }
            }

            int lookup_line = CONTENT_LOOKUP;
            char name_lookup[MAX_BUFFER_SIZE];
            snprintf(name_lookup, sizeof(name_lookup), "%s/%s.cfg", str_rem_last_char(init_meta_dir, 1), stripped_name);

            if (!file_exist(name_lookup)) {
                snprintf(name_lookup, sizeof(name_lookup), "%score.cfg", init_meta_dir);
                lookup_line = GLOBAL_LOOKUP;
            }

            if (!has_custom_name) {
                const char *lookup_result = read_line_int_from(name_lookup, lookup_line) ? lookup(stripped_name) : NULL;
                snprintf(fn_name, sizeof(fn_name), "%s", lookup_result ? lookup_result : stripped_name);
            }

            content_item *new_item = add_item(&items, &item_count, file_names[i], fn_name, "", ITEM);
            adjust_visual_label(new_item->display_name, config.VISUAL.NAME, config.VISUAL.DASH);

            free(file_names[i]);
        }
    }

    free_skiplist(&skiplist);

    sort_items(items, item_count);

    char *e_name_line = file_exist(EXPLORE_NAME) ? read_line_char_from(EXPLORE_NAME, 1) : NULL;
    if (e_name_line) {
        for (size_t i = 0; i < item_count; i++) {
            if (strcasecmp(items[i].name, e_name_line) == 0) {
                sys_index = (int) i;
                remove(EXPLORE_NAME);
                break;
            }
        }
    }

    remove_match_items("History", config.VISUAL.CONTENTHISTORY, &history_items, &history_item_count,
                       populate_history_items, &items, &item_count, sys_dir);

    remove_match_items("Collected", config.VISUAL.CONTENTCOLLECT, &collection_items, &collection_item_count,
                       populate_collection_items, &items, &item_count, sys_dir);

    const char *last_subdir = get_last_subdir(sys_dir, '/', 4);
    char content_tag[MAX_BUFFER_SIZE];
    char content_file[MAX_BUFFER_SIZE];

    for (size_t i = 0; i < item_count; ++i) {
        if (items[i].content_type != ITEM) continue;

        const char *basename = strip_ext(items[i].name);

        snprintf(content_tag, sizeof(content_tag), INFO_COR_PATH "/%s/%s.tag",
                 last_subdir, basename);

        if (file_exist(content_tag)) {
            items[i].glyph_icon = strdup(str_remchar(read_line_char_from(content_tag, 1), ' '));
            items[i].use_module = strdup("muxtag");
        } else {
            snprintf(content_file, sizeof(content_file), "%s/%s", sys_dir, items[i].name);

            items[i].glyph_icon = strdup(get_content_explorer_glyph_name(content_file));
            items[i].use_module = strdup(mux_module);
        }
    }

    assign_item_buckets();
    qsort(items, item_count, sizeof(content_item), bucket_item_compare);
}

static void init_navigation_group_grid(void) {
    grid_mode_enabled = 1;

    init_grid_info((int) item_count, theme.GRID.COLUMN_COUNT);
    create_grid_panel(&theme, (int) item_count);

    load_font_section(FONT_PANEL_DIR, ui_pnlGrid);
    load_font_section(FONT_PANEL_DIR, ui_lblGridCurrentItem);

    if (is_carousel_grid_mode()) {
        create_carousel_grid();
        int prev_dir_index = get_folder_item_index_by_name(items, item_count, prev_dir);
        if (prev_dir_index > -1) sys_index = prev_dir_index;
    } else {
        for (int i = 0; i < item_count; i++) {
            if (strcasecmp(items[i].name, prev_dir) == 0) sys_index = (int) i;
            if (i < theme.GRID.COLUMN_COUNT * theme.GRID.ROW_COUNT) gen_grid_item(i);
        }
    }
}

static void create_content_items(void) {
    char item_curr_dir[PATH_MAX];
    snprintf(item_curr_dir, sizeof(item_curr_dir), "%s", sys_dir);

    char **dir_names = NULL;
    char **file_names = NULL;
    add_directory_and_file_names(item_curr_dir, &dir_names, &file_names);

    int fn_valid = 0;
    struct json fn_json = {0};

    turbo_time(1, 1);

    if (config.VISUAL.FRIENDLYFOLDER) {
        char folder_name_file[MAX_BUFFER_SIZE];
        snprintf(folder_name_file, sizeof(folder_name_file), INFO_NAM_PATH "/folder.json");

        if (file_exist(folder_name_file)) {
            char *file_content = read_all_char_from(folder_name_file);

            if (file_content && json_valid(file_content)) {
                fn_valid = 1;
                fn_json = json_parse(strdup(file_content));
                LOG_SUCCESS(mux_module, "Using Friendly Folder: %s", folder_name_file);
            } else {
                LOG_WARN(mux_module, "Invalid Friendly Folder: %s", folder_name_file);
            }

            free(file_content);
        } else {
            LOG_WARN(mux_module, "Friendly Folder does not exist: %s", folder_name_file);
        }
    }

    update_title(item_curr_dir, fn_valid, fn_json, lang.MUXPLORE.TITLE, UNION_ROM_PATH);

    for (int i = 0; i < dir_count; i++) {
        char *friendly_folder_name = get_friendly_folder_name(dir_names[i], fn_valid, fn_json);

        char rom_dir[MAX_BUFFER_SIZE];
        snprintf(rom_dir, sizeof(rom_dir), "%s/%s", sys_dir, dir_names[i]);
        automatic_assign_core(rom_dir);

        content_item *new_item = add_item(&items, &item_count, dir_names[i], friendly_folder_name, "", FOLDER);

        new_item->glyph_icon = "folder";
        adjust_visual_label(new_item->display_name, config.VISUAL.NAME, config.VISUAL.DASH);

        if (config.VISUAL.FOLDERITEMCOUNT) {
            char display_name[MAX_BUFFER_SIZE];
            snprintf(display_name, sizeof(display_name), "%s (%d)",
                     new_item->display_name, get_directory_item_count(item_curr_dir, new_item->name, 1));
            new_item->display_name = strdup(display_name);
        }

        free(dir_names[i]);
        free(friendly_folder_name);
    }

    gen_item(file_names, file_count);

    assign_item_buckets();
    qsort(items, item_count, sizeof(content_item), bucket_item_compare);

    for (size_t i = 0; i < item_count; i++) {
        if (strcasecmp(items[i].name, prev_dir) == 0) {
            sys_index = (int) i;
            break;
        }
    }

    grid_mode_enabled = !disable_grid_file_exists(item_curr_dir) && theme.GRID.ENABLED && (
            (file_count > 0 && config.VISUAL.GRID_MODE_CONTENT) ||
            (dir_count > 0 && file_count == 0)
    );

    if (grid_mode_enabled) {
        init_navigation_group_grid();
    } else {
        size_t limit = theme.MUX.ITEM.COUNT;
        for (size_t i = 0; i < item_count && i < limit; i++) {
            gen_label(items[i].use_module, items[i].glyph_icon, items[i].display_name);
        }
    }

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);

    free(file_names);
    free(dir_names);

    turbo_time(0, 1);
}

static void update_list_item(lv_obj_t *ui_lblItem, lv_obj_t *ui_lblItemGlyph, int index) {
    lv_label_set_text(ui_lblItem, items[index].display_name);

    char glyph_image_embed[MAX_BUFFER_SIZE];
    if (theme.LIST_DEFAULT.GLYPH_ALPHA > 0 && theme.LIST_FOCUS.GLYPH_ALPHA > 0) {
        get_glyph_path(items[index].use_module, items[index].glyph_icon, glyph_image_embed, MAX_BUFFER_SIZE);
        lv_img_set_src(ui_lblItemGlyph, glyph_image_embed);
    }

    apply_size_to_content(&theme, ui_pnlContent, ui_lblItem, ui_lblItemGlyph, items[index].display_name);
    apply_text_long_dot(&theme, ui_pnlContent, ui_lblItem);
}

static void update_list_items(int start_index) {
    int max = (int) item_count - start_index;
    if (max <= 0) return;

    int count = theme.MUX.ITEM.COUNT;
    if (count > max) count = max;

    for (int index = 0; index < count; ++index) {
        lv_obj_t *panel_item = lv_obj_get_child(ui_pnlContent, index);
        update_list_item(lv_obj_get_child(panel_item, 0), lv_obj_get_child(panel_item, 1), start_index + index);
    }
}

static inline void focus_group(int index) {
    if (index < 0 || index >= theme.MUX.ITEM.COUNT) return;
    lv_obj_t *panel = lv_obj_get_child(ui_pnlContent, index);

    if (!panel) return;

    lv_group_focus_obj(panel);
    lv_group_focus_obj(lv_obj_get_child(panel, 0));
    lv_group_focus_obj(lv_obj_get_child(panel, 1));
}

static void focus_initial(void) {
    if (grid_mode_enabled) {
        image_refresh("box");
        nav_moved = 1;
        return;
    }

    int count = theme.MUX.ITEM.COUNT;

    if (item_count <= (size_t) count) {
        focus_group(current_item_index);
        image_refresh("box");
        nav_moved = 1;
        return;
    }

    int before = (count - count % 2) / 2;
    int after = (count - 1) / 2;

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

    image_refresh("box");
    nav_moved = 1;
}

static int focus_index(void) {
    int before = (theme.MUX.ITEM.COUNT - theme.MUX.ITEM.COUNT % 2) / 2;
    int after = (theme.MUX.ITEM.COUNT - 1) / 2;

    if (current_item_index < before)
        return current_item_index;

    if (current_item_index >= item_count - after)
        return (int) (theme.MUX.ITEM.COUNT - (item_count - current_item_index));

    return before;
}

static inline void move_index(int direction) {
    if (direction < 0) {
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
    } else {
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
    }
}

static void list_nav_move(int steps, int direction) {
    if (!ui_count) return;

    if (!first_open) play_sound(SND_NAVIGATE);
    first_open = 0;

    const int visible_count = theme.MUX.ITEM.COUNT;
    const int static_list = !grid_mode_enabled && item_count <= visible_count;
    const int multi_list = !grid_mode_enabled && item_count > visible_count;

    const int items_before = (visible_count - visible_count % 2) / 2;
    const int items_after = (visible_count - 1) / 2;

    if (!grid_mode_enabled) apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));

    if (static_list) {
        move_index(direction);
        focus_group(current_item_index);

        set_label_long_mode(&theme, lv_group_get_focused(ui_group));
        lv_label_set_text(ui_lblGridCurrentItem, items[current_item_index].display_name);

        image_refresh("box");
        nav_moved = 1;
        return;
    }

    for (int step = 0; step < steps; ++step) {
        move_index(direction);

        if (!is_carousel_grid_mode()) {
            nav_move(ui_group, direction);
            nav_move(ui_group_glyph, direction);
            nav_move(ui_group_panel, direction);
        }

        if (multi_list) {
            if (direction < 0) {
                if (current_item_index == item_count - 1) {
                    update_list_items((int) item_count - visible_count);
                } else if (current_item_index >= items_before && current_item_index < item_count - items_after - 1) {
                    lv_obj_t *last = lv_obj_get_child(ui_pnlContent, visible_count - 1);
                    lv_obj_move_to_index(last, 0);

                    update_list_item(lv_obj_get_child(last, 0), lv_obj_get_child(last, 1), current_item_index - items_before);
                }
            } else {
                if (current_item_index == 0) {
                    update_list_items(0);
                } else if (current_item_index > items_before && current_item_index < item_count - items_after) {
                    lv_obj_t *first = lv_obj_get_child(ui_pnlContent, 0);
                    lv_obj_move_to_index(first, visible_count - 1);

                    update_list_item(lv_obj_get_child(first, 0), lv_obj_get_child(first, 1), current_item_index + items_after);
                }
            }
        } else if (grid_mode_enabled) {
            update_grid(direction);
        }

        if (!grid_mode_enabled) focus_group(focus_index());
    }

    if (!grid_mode_enabled) set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    lv_label_set_text(ui_lblGridCurrentItem, items[current_item_index].display_name);

    image_refresh("box");
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void show_splash() {
    if (config.VISUAL.LAUNCHSPLASH) {
        image_refresh("splash");
        if (splash_valid) {
            lv_obj_center(ui_imgSplash);
            lv_obj_move_foreground(ui_imgSplash);
            lv_obj_move_foreground(overlay_image);

            for (unsigned int i = 0; i <= 255; i += 15) {
                lv_obj_set_style_img_opa(ui_imgSplash, i, MU_OBJ_MAIN_DEFAULT);
                lv_task_handler();
                usleep(128);
            }

            sleep(1);
        }
    }
}

static void process_load(int from_start) {
    if (!ui_count || hold_call) return;

    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        if (lv_obj_has_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    play_sound(SND_CONFIRM);
    int load_message = 0;

    if (items[current_item_index].content_type == FOLDER) {
        load_message = 1;

        char n_dir[MAX_BUFFER_SIZE];
        snprintf(n_dir, sizeof(n_dir), "%s/%s",
                 sys_dir, items[current_item_index].name);

        write_text_to_file(EXPLORE_DIR, "w", CHAR, n_dir);
    } else {
        write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

        if (load_content(0, sys_dir, items[current_item_index].name)) {
            if (config.SETTINGS.ADVANCED.PASSCODE) {
                int result = 0;

                while (result != 1) {
                    result = muxpass_main(PCT_LAUNCH);

                    switch (result) {
                        case 1:
                            show_splash();
                            config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
                            exit_status = 1;
                            break;
                        case 2:
                        default:
                            if (file_exist(MUOS_ROM_LOAD)) remove(MUOS_ROM_LOAD);
                            if (file_exist(MUOS_CON_LOAD)) remove(MUOS_CON_LOAD);
                            if (file_exist(MUOS_GOV_LOAD)) remove(MUOS_GOV_LOAD);
                            if (file_exist(MUOS_RAC_LOAD)) remove(MUOS_RAC_LOAD);
                            if (file_exist(MUOS_FLT_LOAD)) remove(MUOS_FLT_LOAD);

                            write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

                            goto load_end;
                    }
                }
            } else {
                show_splash();
                config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
                exit_status = 1;
            }
        } else {
            write_text_to_file(MUOS_ASS_FROM, "w", CHAR, "explore");
            write_text_to_file(OPTION_SKIP, "w", CHAR, "");
            load_mux("assign");
        }
    }

    if (from_start) write_text_to_file(MANUAL_RA_LOAD, "w", INT, 1);
    if (load_message) toast_message(lang.GENERIC.LOADING, FOREVER);

    load_end:
    load_mux("explore");

    close_input();
    mux_input_stop();
}

static void handle_a(void) {
    if (hold_call) return;
    process_load(launch_flag(config.VISUAL.LAUNCH_SWAP, 0));
}

static void handle_a_hold(void) {
    if (msgbox_active || hold_call) return;
    process_load(launch_flag(config.VISUAL.LAUNCH_SWAP, 1));
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

    if (at_base(sys_dir, "ROMS")) {
        remove(EXPLORE_DIR);
    } else {
        char *base_dir = strrchr(sys_dir, '/');
        if (base_dir) write_text_to_file(EXPLORE_DIR, "w", CHAR, strndup(sys_dir, base_dir - sys_dir));
    }

    load_mux(file_exist(EXPLORE_DIR) ? "explore" : "launcher");

    close_input();
    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || !ui_count || hold_call) return;

    play_sound(SND_CONFIRM);

    write_text_to_file(MUOS_CIX_LOAD, "w", INT, current_item_index);

    if (is_ksk(kiosk.CONTENT.OPTION)) {
        if (!is_ksk(kiosk.CONTENT.SEARCH)) {
            load_mux("search");

            close_input();
            mux_input_stop();
        }
        return;
    }

    write_text_to_file(MUOS_SAA_LOAD, "w", INT, 1);
    write_text_to_file(MUOS_SAG_LOAD, "w", INT, 1);

    load_content_core(1, 0, sys_dir, items[current_item_index].name);
    load_content_governor(sys_dir, NULL, 1, 0, 0);

    load_mux("option");

    close_input();
    mux_input_stop();
}

static void handle_y(void) {
    if (msgbox_active || !ui_count || hold_call) return;

    if (items[current_item_index].content_type == FOLDER) {
        play_sound(SND_ERROR);
        toast_message(lang.MUXPLORE.ERROR.NO_FOLDER, SHORT);
    } else {
        if (is_ksk(kiosk.LAUNCH.COLLECTION) || is_ksk(kiosk.COLLECT.ADD_CON)) return;

        write_text_to_file(ADD_MODE_FROM, "w", CHAR, "explore");
        if (!load_content(1, sys_dir, items[current_item_index].name)) {
            remove(ADD_MODE_FROM);
            play_sound(SND_ERROR);
            toast_message(lang.MUXPLORE.ERROR.NO_CORE, SHORT);
        }
    }
}

static void handle_start(void) {
    if (msgbox_active || !ui_count || hold_call) return;

    play_sound(SND_CONFIRM);

    remove(EXPLORE_DIR);
    load_mux("explore");

    close_input();
    mux_input_stop();
}

static void handle_select(void) {
    if (msgbox_active || !ui_count || hold_call) return;

    toast_message(lang.GENERIC.REFRESH_RUN, FOREVER);

    const char *args[] = {(OPT_PATH "script/mount/union.sh"), "restart", NULL};
    run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);

    write_text_to_file(EXPLORE_DIR, "w", CHAR, sys_dir);
    load_mux("explore");

    close_input();
    mux_input_stop();
}

static void handle_menu(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    image_refresh("preview");

    show_info_box(items[current_item_index].display_name, load_content_description(), 1);
}

static void handle_random_select(void) {
    if (msgbox_active || ui_count < 2 || hold_call || !config.VISUAL.SHUFFLE) return;

    int dir = +1;
    int target = current_item_index;

    shuffle_index(current_item_index, &dir, &target);
    if (target < 0 || target >= ui_count || target == current_item_index) return;

    if (grid_mode_enabled) {
        current_item_index = target;
        update_grid_items(1);
        list_nav_next(0);
    } else {
        int steps = target - current_item_index;
        if (steps < 0) steps = -steps;

        list_nav_move(steps, dir);
    }
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_lblCounter_explore,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            ui_pnlMessage,
            NULL
    });
}

static void init_elements(void) {
    lv_obj_set_align(ui_imgBox, config.VISUAL.BOX_ART_ALIGN);
    lv_obj_set_align(ui_viewport_objects[0], config.VISUAL.BOX_ART_ALIGN);

    adjust_box_art();
    adjust_panels();
    header_and_footer_setup();
    lv_label_set_text(ui_lblPreviewHeader, lang.GENERIC.SWITCH_IMAGE);
    lv_obj_clear_flag(ui_lblPreviewHeaderGlyph, LV_OBJ_FLAG_HIDDEN);

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph,    "",                   1},
            {ui_lblNavA,         lang.GENERIC.OPEN,    1},
            {ui_lblNavBGlyph,    "",                   0},
            {ui_lblNavB,         lang.GENERIC.BACK,    0},
            {ui_lblNavXGlyph,    "",                   0},
            {ui_lblNavX,         lang.GENERIC.OPTION,  0},
            {ui_lblNavYGlyph,    "",                   1},
            {ui_lblNavY,         lang.GENERIC.COLLECT, 1},
            {ui_lblNavMenuGlyph, "",                   1},
            {ui_lblNavMenu,      lang.GENERIC.INFO,    1},
            {NULL, NULL,                               0}
    });

    overlay_display();
}

static void refresh_nav_items() {
    if (items[current_item_index].content_type == FOLDER) {
        lv_obj_add_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_clear_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void ui_refresh_task() {
    if (nav_moved) {
        starter_image = adjust_wallpaper_element(ui_group, starter_image, WALL_GENERAL);
        adjust_panels();

        const char *content_label = lv_obj_get_user_data(lv_group_get_focused(ui_group));
        snprintf(current_content_label, sizeof(current_content_label), "%s", content_label);

        if (!lv_obj_has_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
        }

        update_file_counter(ui_lblCounter_explore, file_count);
        lv_obj_move_foreground(overlay_image);

        refresh_nav_items();

        nav_moved = 0;
    }
}

int muxplore_main(int index, char *dir) {
    exit_status = 0;
    sys_index = -1;
    file_count = 0;
    dir_count = 0;
    starter_image = 0;
    splash_valid = 0;

    snprintf(sys_dir, sizeof(sys_dir), "%s", (strcmp(dir, "") == 0) ? UNION_ROM_PATH : dir);
    sys_index = index;

    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, "");
    init_ui_item_counter(&theme);

    ui_viewport_objects[0] = lv_obj_create(ui_pnlBox);
    ui_viewport_objects[1] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[2] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[3] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[4] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[5] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[6] = lv_img_create(ui_viewport_objects[0]);

    ui_imgSplash = lv_img_create(ui_screen);
    lv_obj_set_style_img_opa(ui_imgSplash, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    init_fonts();

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    reset_ui_groups();

    snprintf(prev_dir, sizeof(prev_dir), "%s", (file_exist(MUOS_PDI_LOAD)) ? read_all_char_from(MUOS_PDI_LOAD) : "");

    load_skip_patterns();
    create_content_items();
    ui_count = (int) item_count;
    init_elements();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, get_last_dir(sys_dir));
    if (strcasecmp(read_all_char_from(MUOS_PDI_LOAD), "ROMS") == 0) {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, get_last_subdir(sys_dir, '/', 4));
    }

    int nav_vis = 0;
    int collect_vis = 0;
    if (ui_count > 0) {
        nav_vis = 1;

        if (sys_index >= 0 && sys_index < ui_count) current_item_index = sys_index;
        else current_item_index = 0;

        focus_initial();

        collect_vis = items[current_item_index].content_type == ITEM ? 1 : 0;
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXPLORE.NONE);
    }

    struct nav_flag nav_e[] = {
            {ui_lblNavA,         nav_vis},
            {ui_lblNavAGlyph,    nav_vis},
            {ui_lblNavX,         nav_vis},
            {ui_lblNavXGlyph,    nav_vis},
            {ui_lblNavY,         collect_vis},
            {ui_lblNavYGlyph,    collect_vis},
            {ui_lblNavMenu,      nav_vis},
            {ui_lblNavMenuGlyph, nav_vis}
    };

    set_nav_flags(nav_e, A_SIZE(nav_e));
    adjust_panels();

    if (is_ksk(kiosk.LAUNCH.COLLECTION) || is_ksk(kiosk.COLLECT.ADD_CON)) {
        lv_obj_add_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    update_file_counter(ui_lblCounter_explore, file_count);

    if (file_exist(ADD_MODE_DONE)) {
        if (strcasecmp(read_all_char_from(ADD_MODE_DONE), "DONE") == 0) {
            toast_message(lang.GENERIC.ADD_COLLECT, SHORT);
        }
        remove(ADD_MODE_DONE);
    }

    init_timer(ui_refresh_task, NULL);

    if (ui_count > 0) set_label_long_mode(&theme, lv_group_get_focused(ui_group));

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1 ||
                          (grid_mode_enabled && theme.GRID.NAVIGATION_TYPE >= 1 && theme.GRID.NAVIGATION_TYPE <= 5)),
            .press_handler = {
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_SELECT] = handle_select,
                    [MUX_INPUT_START] = handle_start,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_list_nav_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_list_nav_right,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
                    [MUX_INPUT_R2] = handle_random_select,
            },
            .release_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_A] = handle_a_hold,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_DPAD_LEFT] = handle_list_nav_left_hold,
                    [MUX_INPUT_DPAD_RIGHT] = handle_list_nav_right_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
                    [MUX_INPUT_R2] = handle_random_select,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    free_items(&items, &item_count);

    return exit_status;
}
