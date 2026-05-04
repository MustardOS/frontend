#include "muxshare.h"

#define LARGE_CONTENT_LOAD 512

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

static int *union_dir_item_count = NULL;
static int union_dir_item_count_len = 0;

static void navigate_to_dir(const char *new_dir, int restore_index);

static void assign_item_buckets(void) {
    for (size_t i = 0; i < item_count; i++) {
        content_item *it = &items[i];
        it->sort_bucket = config.SORT.DEFAULT;
        it->group_tag[0] = '\0';

        int sort_special = -1;

        if (config.SORT.COLLECTION != config.SORT.DEFAULT && is_in_list(collection_items, collection_item_count, sys_dir, it->name)) {
            sort_special = config.SORT.COLLECTION;
        }

        if (config.SORT.HISTORY != config.SORT.DEFAULT && is_in_list(history_items, history_item_count, sys_dir, it->name)) {
            if (config.SORT.HISTORY > sort_special) sort_special = config.SORT.HISTORY;
        }

        if (it->use_module && strcasecmp(it->use_module, "muxtag") == 0) {
            int tag_sort_bucket = get_tag_sort_bucket(tag_items, tag_item_count, it->glyph_icon);
            if (tag_sort_bucket > sort_special) sort_special = tag_sort_bucket;
        }

        if (sort_special > -1) it->sort_bucket = sort_special;
    }
}

static char *load_content_description(void) {
    char content_desc[MAX_BUFFER_SIZE];

    char *content_label = get_file_name(items[current_item_index].name);
    char *desc_name = strip_ext(content_label);

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

    char *file_name = get_file_name(items[current_item_index].name);

    char file_name_no_ext[MAX_BUFFER_SIZE];
    snprintf(file_name_no_ext, sizeof(file_name_no_ext), "%s", file_name);

    char *dot = strrchr(file_name_no_ext, '.');
    if (dot) *dot = '\0';

    char core_artwork[MAX_BUFFER_SIZE];
    char h_core_artwork_buf[MAX_BUFFER_SIZE];
    char *h_core_artwork;

    if (union_is_root(sys_dir) || at_base(sys_dir, MAIN_ROM_DIR)) {
        snprintf(h_core_artwork_buf, sizeof(h_core_artwork_buf), "Folder");
        h_core_artwork = h_core_artwork_buf;
    } else {
        get_catalogue_name(sys_dir, file_name, core_artwork, sizeof(core_artwork));

        if (items[current_item_index].content_type == FOLDER) {
            char *catalogue_name = get_catalogue_name_from_rom_path(sys_dir, file_name);
            snprintf(h_core_artwork_buf, sizeof(h_core_artwork_buf), "%s", strlen(catalogue_name) > 0 ? catalogue_name : "Folder");

            h_core_artwork = h_core_artwork_buf;
        } else {
            h_core_artwork = core_artwork;
        }
    }

    render_image_refresh(image_type, h_core_artwork, file_name_no_ext, ui_imgSplash, ui_viewport_objects, &starter_image, &splash_valid);
}

static void add_directory_and_file_names(const char *base_dir, char ***dir_names, char ***dir_paths, char ***file_names, char ***file_paths) {
    char **all_dir_names = NULL;
    char **all_dir_paths = NULL;

    char **all_file_names = NULL;
    char **all_file_paths = NULL;

    int all_dir_count = 0;
    int all_file_count = 0;

    int *raw_counts = NULL;

    file_count = 0;
    dir_count = 0;

    *dir_names = NULL;
    *dir_paths = NULL;

    *file_names = NULL;
    *file_paths = NULL;

    free(union_dir_item_count);
    union_dir_item_count = NULL;
    union_dir_item_count_len = 0;

    union_collect(base_dir, &all_dir_names, &all_dir_paths, &all_dir_count, &all_file_names, &all_file_paths, &all_file_count, &raw_counts);

    if (all_dir_count > 0) {
        if (config.VISUAL.DISPLAYEMPTYFOLDER) {
            *dir_names = all_dir_names;
            *dir_paths = all_dir_paths;
            dir_count = all_dir_count;

            union_dir_item_count = raw_counts;
            union_dir_item_count_len = all_dir_count;

            all_dir_names = NULL;
            all_dir_paths = NULL;

            raw_counts = NULL;
        } else {
            char **kept_dirs = malloc((size_t) all_dir_count * sizeof(char *));
            char **kept_paths = malloc((size_t) all_dir_count * sizeof(char *));
            int *kept_counts = malloc((size_t) all_dir_count * sizeof(int));

            if (kept_dirs && kept_paths && kept_counts) {
                for (int i = 0; i < all_dir_count; i++) {
                    int cnt = -1;

                    if (raw_counts) cnt = raw_counts[i];
                    if (cnt < 0) cnt = union_get_directory_item_count(base_dir, all_dir_names[i], COUNT_BOTH);

                    if (cnt > 0) {
                        kept_dirs[dir_count] = all_dir_names[i];
                        kept_paths[dir_count] = all_dir_paths[i];
                        kept_counts[dir_count] = cnt;

                        dir_count++;
                    } else {
                        free(all_dir_names[i]);
                        free(all_dir_paths[i]);
                    }
                }

                if (dir_count > 0) {
                    char **dirs_tmp = realloc(kept_dirs, (size_t) dir_count * sizeof(char *));
                    char **paths_tmp = realloc(kept_paths, (size_t) dir_count * sizeof(char *));
                    int *counts_tmp = realloc(kept_counts, (size_t) dir_count * sizeof(int));

                    *dir_names = dirs_tmp ? dirs_tmp : kept_dirs;
                    *dir_paths = paths_tmp ? paths_tmp : kept_paths;

                    union_dir_item_count = counts_tmp ? counts_tmp : kept_counts;
                    union_dir_item_count_len = dir_count;
                } else {
                    free(kept_dirs);
                    free(kept_paths);
                    free(kept_counts);

                    *dir_names = NULL;
                    *dir_paths = NULL;

                    union_dir_item_count = NULL;
                    union_dir_item_count_len = 0;
                }
            } else {
                free(kept_dirs);
                free(kept_paths);
                free(kept_counts);

                for (int i = 0; i < all_dir_count; i++) {
                    free(all_dir_names[i]);
                    free(all_dir_paths[i]);
                }

                *dir_names = NULL;
                *dir_paths = NULL;

                dir_count = 0;

                union_dir_item_count = NULL;
                union_dir_item_count_len = 0;
            }
        }
    }

    free(raw_counts);

    free(all_dir_names);
    free(all_dir_paths);

    *file_names = all_file_names;
    *file_paths = all_file_paths;

    file_count = all_file_count;
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

static void gen_item(char **file_names, char **file_paths, int file_count) {
    char init_meta_dir[MAX_BUFFER_SIZE];
    char sub_path[PATH_MAX];

    union_get_relative_path(sys_dir, sub_path, sizeof(sub_path));

    if (strncasecmp(sub_path, MAIN_ROM_DIR, 4) == 0) {
        char *p = sub_path + 4;
        while (*p == '/') p++;
        memmove(sub_path, p, strlen(p) + 1);
    }

    snprintf(init_meta_dir, sizeof(init_meta_dir), INFO_CON_PATH "/%s/", sub_path);

    remove_double_slashes(init_meta_dir);
    create_directories(init_meta_dir, 0);

    SkipList skiplist;
    init_skiplist(&skiplist);

    typedef struct {
        char *name;
        char *full_path;
        char display[MAX_BUFFER_SIZE];
    } temp_item;

    temp_item *tmp = malloc((size_t) file_count * sizeof(temp_item));
    if (!tmp) {
        free_skiplist(&skiplist);
        return;
    }

    int tmp_count = 0;
    int show_hidden = config.VISUAL.HIDDEN;

    if (!show_hidden) {
        for (int i = 0; i < file_count; i++) {
            if (ends_with(file_names[i], ".cue")) {
                process_cue_file(sys_dir, file_names[i], &skiplist);
            } else if (ends_with(file_names[i], ".gdi")) {
                process_gdi_file(sys_dir, file_names[i], &skiplist);
            } else if (ends_with(file_names[i], ".m3u")) {
                process_m3u_file(sys_dir, file_names[i], &skiplist);
            }
        }
    }

    for (int i = 0; i < file_count; i++) {
        if ((i % 500) == 0 && i > 0) {
            LOG_DEBUG(mux_module, "Content Collect: %d/%d", i, file_count);
        }

        char *name = file_names[i];
        char *full_path = file_paths[i];

        if (!name || !full_path) {
            free(name);
            free(full_path);
            continue;
        }

        if (!show_hidden && in_skiplist(&skiplist, full_path)) {
            free(name);
            free(full_path);
            continue;
        }

        char base[MAX_BUFFER_SIZE];
        snprintf(base, sizeof(base), "%s", name);

        char *dot = strrchr(base, '.');
        if (dot) *dot = '\0';

        resolve_friendly_name(sys_dir, base, tmp[tmp_count].display);

        tmp[tmp_count].name = name;
        tmp[tmp_count].full_path = full_path;
        tmp_count++;
    }

    free_skiplist(&skiplist);

    for (int i = 0; i < tmp_count; i++) {
        add_item(&items, &item_count, tmp[i].name, tmp[i].display, tmp[i].full_path, ITEM);
        free(tmp[i].full_path);
    }

    free(tmp);

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
        free(e_name_line);
    }

    remove_match_items("History", config.VISUAL.CONTENTHISTORY, &history_items, &history_item_count,
                       populate_history_items, &items, &item_count, sys_dir);

    remove_match_items("Collected", config.VISUAL.CONTENTCOLLECT, &collection_items, &collection_item_count,
                       populate_collection_items, &items, &item_count, sys_dir);

    char content_tag[PATH_MAX];

    for (size_t i = 0; i < item_count; ++i) {
        if (items[i].content_type != ITEM) continue;

        char basename[MAX_BUFFER_SIZE];
        snprintf(basename, sizeof(basename), "%s", items[i].name);

        char *dot = strrchr(basename, '.');
        if (dot) *dot = '\0';

        snprintf(content_tag, sizeof(content_tag), INFO_CON_PATH "/%s/%s.tag", sub_path, basename);
        remove_double_slashes(content_tag);

        if (file_exist(content_tag)) {
            char *line = read_line_char_from(content_tag, 1);

            if (line && *line) {
                str_remchar(line, ' ');
                items[i].glyph_icon = line;
            } else {
                if (line) free(line);
                items[i].glyph_icon = "default";
            }

            items[i].use_module = "muxtag";
        } else {
            items[i].glyph_icon = get_content_explorer_glyph_name(items[i].extra_data);
            items[i].use_module = mux_module;
        }
    }
}

static void init_navigation_group_grid(void) {
    grid_mode_enabled = 1;

    init_grid_info((int) item_count, theme.GRID.COLUMN_COUNT);
    create_grid_panel(&theme, (int) item_count);

    load_font_section(FONT_PANEL_DIR, ui_pnlGrid);
    load_font_section(FONT_PANEL_DIR, ui_lblGridCurrentItem);

    if (is_carousel_grid_mode()) {
        create_carousel_grid();
        int prev_dir_index = get_item_index_by_extra_data(items, item_count, prev_dir);
        if (prev_dir_index > -1) sys_index = prev_dir_index;
    } else {
        for (int i = 0; i < item_count; i++) {
            if (strcasecmp(items[i].extra_data, prev_dir) == 0) sys_index = (int) i;
            if (i < theme.GRID.COLUMN_COUNT * theme.GRID.ROW_COUNT) gen_grid_item(i);
        }
    }
}

static int get_visible_dir_count(const char *base_dir, const char *name, int idx) {
    if (union_dir_item_count && idx >= 0 && idx < union_dir_item_count_len) {
        int cnt = union_dir_item_count[idx];
        if (cnt >= 0) return cnt;
    }

    return union_get_directory_item_count(base_dir, name, COUNT_BOTH);
}

static void create_content_items(void) {
    char item_curr_dir[PATH_MAX];
    snprintf(item_curr_dir, sizeof(item_curr_dir), "%s", sys_dir);
    automatic_assign_core(item_curr_dir);

    char **dir_names = NULL;
    char **dir_paths = NULL;

    char **file_names = NULL;
    char **file_paths = NULL;

    add_directory_and_file_names(item_curr_dir, &dir_names, &dir_paths, &file_names, &file_paths);

    int fn_valid = 0;
    struct json fn_json = {0};

    turbo_time(1, 1);

    if (config.VISUAL.FRIENDLYFOLDER) {
        const char *folder_name_file = resolve_info_path("name/folder.json");

        if (folder_name_file) {
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

    {
        char root_dir[PATH_MAX];
        union_get_title_root(item_curr_dir, root_dir, sizeof(root_dir));
        update_title(item_curr_dir, fn_valid, fn_json, lang.MUXPLORE.TITLE, root_dir);
    }

    for (int i = 0; i < dir_count; i++) {
        if (!dir_names || !dir_names[i] || !dir_paths || !dir_paths[i]) continue;

        int cnt = get_visible_dir_count(item_curr_dir, dir_names[i], i);
        if (!config.VISUAL.DISPLAYEMPTYFOLDER && cnt <= 0) {
            free(dir_names[i]);
            free(dir_paths[i]);

            dir_names[i] = NULL;
            dir_paths[i] = NULL;

            continue;
        }

        if (folder_has_launch_file(sys_dir, dir_names[i])) {
            free(dir_names[i]);
            free(dir_paths[i]);

            dir_names[i] = NULL;
            dir_paths[i] = NULL;

            continue;
        }

        char *friendly_folder_name = get_friendly_folder_name(dir_names[i], fn_valid, fn_json);
        if (!friendly_folder_name) {
            free(dir_names[i]);
            free(dir_paths[i]);

            dir_names[i] = NULL;
            dir_paths[i] = NULL;

            continue;
        }

        automatic_assign_core(dir_paths[i]);
        content_item *new_item = add_item(&items, &item_count, dir_names[i], friendly_folder_name, dir_paths[i], FOLDER);

        if (new_item) {
            new_item->glyph_icon = "folder";
            adjust_visual_label(new_item->display_name, config.VISUAL.NAME, config.VISUAL.DASH);

            if (config.VISUAL.FOLDERITEMCOUNT) {
                char display_name[MAX_BUFFER_SIZE];
                snprintf(display_name, sizeof(display_name), "%s (%d)", new_item->display_name, cnt);

                char *new_display = strdup(display_name);
                if (new_display) new_item->display_name = new_display;
            }
        }

        free(friendly_folder_name);
        free(dir_paths[i]);

        dir_names[i] = NULL;
        dir_paths[i] = NULL;
    }

    free(union_dir_item_count);
    union_dir_item_count = NULL;
    union_dir_item_count_len = 0;

    gen_item(file_names, file_paths, file_count);

    assign_item_buckets();
    qsort(items, item_count, sizeof(content_item), bucket_item_compare);

    if (file_exist(MUOS_IDX_LOAD)) {
        int idx = read_line_int_from(MUOS_IDX_LOAD, 1);
        remove(MUOS_IDX_LOAD);

        if (idx >= 0 && idx < (int) item_count) sys_index = idx;
    }

    if (sys_index < 0 && file_exist(MUOS_HST_LOAD)) {
        char *last_name = read_all_char_from(MUOS_HST_LOAD);
        remove(MUOS_HST_LOAD);

        if (last_name) {
            int idx = get_item_index_by_name(items, item_count, last_name, ITEM);
            if (idx >= 0) sys_index = idx;

            free(last_name);
        }
    }

    if (sys_index < 0) {
        for (size_t i = 0; i < item_count; i++) {
            if (strcasecmp(items[i].extra_data, prev_dir) == 0) {
                sys_index = (int) i;
                break;
            }
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
    free(file_paths);

    free(dir_names);
    free(dir_paths);

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

static void focus_grid_index(int index) {
    if (index < 0 || index >= (int) item_count) return;
    current_item_index = index;

    if (is_carousel_grid_mode()) {
        update_grid_items(0);
        update_grid_items(1);
    } else {
        update_grid_items(0);

        // This is such a bullshit workaround...
        uint32_t grid_item_count = lv_obj_get_child_cnt(ui_pnlGrid);
        for (int i = 0; i < grid_item_count; i++) {
            lv_obj_t *panel = lv_obj_get_child(ui_pnlGrid, i);
            if (!panel) continue;

            int panel_map = IFU(lv_obj_get_user_data(panel));
            if (panel_map == current_item_index) {
                lv_group_focus_obj(panel);
                lv_obj_invalidate(panel);
                break;
            }
        }
    }

    lv_label_set_text(ui_lblGridCurrentItem, items[current_item_index].display_name);
    if (config.VISUAL.BOX_ART_HIDE && config.VISUAL.BOX_ART < 4) image_refresh("box");

    nav_moved = 1;
}

static int focus_list_index(void) {
    int before = (theme.MUX.ITEM.COUNT - theme.MUX.ITEM.COUNT % 2) / 2;
    int after = (theme.MUX.ITEM.COUNT - 1) / 2;

    if (current_item_index < before) return current_item_index;
    if (current_item_index >= item_count - after) return (int) (theme.MUX.ITEM.COUNT - (item_count - current_item_index));

    return before;
}

static void focus_initial(void) {
    if (grid_mode_enabled) {
        focus_grid_index(current_item_index);
        return;
    }

    int count = theme.MUX.ITEM.COUNT;

    if (item_count <= (size_t) count) {
        focus_group(current_item_index);

        if (config.VISUAL.BOX_ART < 4) image_refresh("box");
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

    if (config.VISUAL.BOX_ART < 4) image_refresh("box");
    nav_moved = 1;
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

    play_sound(SND_NAVIGATE);

    const int visible_count = theme.MUX.ITEM.COUNT;
    const int static_list = !grid_mode_enabled && item_count <= visible_count;
    const int multi_list = !grid_mode_enabled && item_count > visible_count;

    const int items_before = (visible_count - visible_count % 2) / 2;
    const int items_after = (visible_count - 1) / 2;

    if (!grid_mode_enabled) apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));

    if (static_list) {
        for (int step = 0; step < steps; ++step) {
            move_index(direction);
        }
        focus_group(current_item_index);

        set_label_long_mode(&theme, lv_group_get_focused(ui_group));
        lv_label_set_text(ui_lblGridCurrentItem, items[current_item_index].display_name);

        if (config.VISUAL.BOX_ART < 4) image_refresh("box");
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

        if (!grid_mode_enabled) focus_group(focus_list_index());
    }

    if (!grid_mode_enabled) set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    lv_label_set_text(ui_lblGridCurrentItem, items[current_item_index].display_name);

    if (config.VISUAL.BOX_ART < 4) image_refresh("box");
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

static int fwd_hist_parse(char *line, char *key, int *out_val) {
    const char *eq = strchr(line, '=');
    if (!eq) return 0;

    size_t len = (size_t) (eq - line);
    if (len >= 4096) len = 4096 - 1;

    memcpy(key, line, len);
    key[len] = '\0';
    str_trim(key);

    const char *val_str = eq + 1;
    while (*val_str == ' ' || *val_str == '\t') val_str++;

    char *end_ptr;
    long tmp = strtol(val_str, &end_ptr, 10);

    if (val_str == end_ptr) return 0;
    if (tmp < 0 || tmp > INT_MAX) return 0;

    *out_val = (int) tmp;
    return 1;
}

static void fwd_hist_set(char *dir, int index) {
    if (!config.VISUAL.FORWARDHISTORY || index < 0 || union_is_root(dir) || at_base(dir, MAIN_ROM_DIR)) return;

    char rel[PATH_MAX];
    union_get_relative_path(dir, rel, sizeof(rel));
    if (!*rel) return;

    FILE *in = fopen(FWD_HIST_FILE, "r");
    FILE *out = fopen(FWD_HIST_FILE ".tmp", "w");
    if (!out) {
        if (in) fclose(in);
        return;
    }

    int found = 0;

    if (in) {
        char line[PATH_MAX + 32];
        char key[PATH_MAX];
        int val;

        while (fgets(line, sizeof(line), in)) {
            if (fwd_hist_parse(line, key, &val)) {
                if (strcmp(key, rel) == 0) {
                    fprintf(out, "%s = %d\n", rel, index);
                    found = 1;
                } else {
                    fprintf(out, "%s = %d\n", key, val);
                }
            }
        }

        fclose(in);
    }

    if (!found) fprintf(out, "%s = %d\n", rel, index);

    fclose(out);
    rename(FWD_HIST_FILE ".tmp", FWD_HIST_FILE);
}

static int fwd_hist_get(char *dir) {
    if (!config.VISUAL.FORWARDHISTORY || union_is_root(dir) || at_base(dir, MAIN_ROM_DIR)) return -1;

    char rel[PATH_MAX];
    union_get_relative_path(dir, rel, sizeof(rel));
    if (!*rel) return -1;

    FILE *f = fopen(FWD_HIST_FILE, "r");
    if (!f) return -1;

    char line[PATH_MAX + 32];
    char key[PATH_MAX];
    int val;
    int result = -1;

    while (fgets(line, sizeof(line), f)) {
        if (fwd_hist_parse(line, key, &val)) {
            if (strcmp(key, rel) == 0) {
                result = val;
                break;
            }
        }
    }

    fclose(f);
    return result;
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

    if (items[current_item_index].content_type == FOLDER) {
        char folder_path[PATH_MAX];
        snprintf(folder_path, sizeof(folder_path), "%s", items[current_item_index].extra_data);

        if (union_get_directory_item_count(sys_dir, items[current_item_index].name, COUNT_BOTH) >= LARGE_CONTENT_LOAD) {
            toast_message(lang.GENERIC.LOADING, FOREVER);
        }

        fwd_hist_set(sys_dir, current_item_index);
        navigate_to_dir(folder_path, -1);
        return;
    }

    write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

    if (load_content(0, items[current_item_index].extra_data)) {
        if (config.SETTINGS.ADVANCED.PASSCODE) {
            int result = 0;

            while (result != 1) {
                result = muxpass_main(PCT_LAUNCH);

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
                        if (file_exist(MUOS_HST_LOAD)) remove(MUOS_HST_LOAD);

                        write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);
                        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, sys_dir);

                        goto load_end;
                }
            }
        } else {
            write_text_to_file(MUOS_HST_LOAD, "w", CHAR, items[current_item_index].name);
            show_splash();
            fade_out_screen();
            exit_status = 1;
        }
    } else {
        write_text_to_file(MUOS_ASS_FROM, "w", CHAR, "explore");
        write_text_to_file(OPTION_SKIP, "w", CHAR, "");
        load_mux("assign");
    }

    if (from_start) write_text_to_file(MANUAL_RA_LOAD, "w", INT, 1);

    toast_message(lang.GENERIC.LOADING, MEDIUM);

    load_end:
    load_mux("explore");
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

static void reload_explore(void) {
    remove(EXPLORE_DIR);
    load_mux("launcher");
    mux_input_stop();
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

    if (at_base(sys_dir, MAIN_ROM_DIR)) {
        reload_explore();
        return;
    }

    char parent[PATH_MAX];
    snprintf(parent, sizeof(parent), "%s", sys_dir);

    char *slash = strrchr(parent, '/');
    if (!slash || slash == parent) {
        reload_explore();
        return;
    }

    *slash = '\0';

    fwd_hist_set(sys_dir, current_item_index);
    navigate_to_dir(parent, -1);
}

static void handle_x(void) {
    if (msgbox_active || !ui_count || hold_call) return;

    play_sound(SND_CONFIRM);

    write_text_to_file(EXPLORE_DIR, "w", CHAR, sys_dir);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, sys_dir);
    write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);
    write_text_to_file(MUOS_CIX_LOAD, "w", INT, current_item_index);

    if (is_ksk(kiosk.CONTENT.OPTION)) {
        if (!is_ksk(kiosk.CONTENT.SEARCH)) {
            load_mux("search");

            mux_input_stop();
        }
        return;
    }

    write_text_to_file(MUOS_SAA_LOAD, "w", INT, 1);
    write_text_to_file(MUOS_SAG_LOAD, "w", INT, 1);
    write_text_to_file(MUOS_HST_LOAD, "w", CHAR, items[current_item_index].name);

    load_content_core(1, 0, items[current_item_index].extra_data);
    load_content_governor(sys_dir, NULL, 1, 0, 0);

    load_mux("option");

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
        write_text_to_file(EXPLORE_DIR, "w", CHAR, sys_dir);
        if (!load_content(1, items[current_item_index].extra_data)) {
            remove(ADD_MODE_FROM);
            remove(EXPLORE_DIR);
            play_sound(SND_ERROR);
            toast_message(lang.MUXPLORE.ERROR.NO_CORE, SHORT);
        }
    }
}

static void handle_start(void) {
    if (msgbox_active || !ui_count || hold_call) return;

    play_sound(SND_CONFIRM);

    char root_dir[PATH_MAX];
    union_get_roms_root(root_dir, sizeof(root_dir));

    navigate_to_dir(root_dir, 0);

    remove(EXPLORE_DIR);
}

static void handle_select(void) {
    if (msgbox_active || hold_call) return;

    if (is_ksk(kiosk.CONTENT.SEARCH)) {
        kiosk_denied();
        return;
    }

    play_sound(SND_CONFIRM);

    if (ui_count > 0) {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, sys_dir);
        write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);
    }

    load_mux("search");

    mux_input_stop();
}

static void handle_help(void) {
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
    if (!ui_count) return;

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

        if (ui_count > 0) {
            lv_obj_t *e_focused = lv_group_get_focused(ui_group);
            const char *content_label = e_focused ? lv_obj_get_user_data(e_focused) : NULL;

            if (content_label) {
                snprintf(current_content_label, sizeof(current_content_label), "%s", content_label);
            } else {
                current_content_label[0] = '\0';
            }
        } else {
            current_content_label[0] = '\0';
        }

        if (!lv_obj_has_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
        }

        update_file_counter(ui_lblCounter_explore, file_count);
        lv_obj_move_foreground(overlay_image);

        refresh_nav_items();

        nav_moved = 0;
    }
}

static void navigate_to_dir(const char *new_dir, int restore_index) {
    char dest[PATH_MAX];
    snprintf(dest, sizeof(dest), "%s", new_dir);
    snprintf(prev_dir, sizeof(prev_dir), "%s", sys_dir);

    free_items(&items, &item_count);
    reset_ui_groups();

    if (grid_mode_enabled) {
        lv_obj_clean(ui_pnlGrid);
        lv_obj_add_flag(ui_lblGridCurrentItem, MU_OBJ_FLAG_HIDE_FLOAT);
        grid_mode_enabled = 0;
    }

    lv_obj_clean(ui_pnlContent);
    snprintf(sys_dir, sizeof(sys_dir), "%s", dest);

    ui_count = 0;
    file_count = 0;
    dir_count = 0;
    starter_image = 0;
    splash_valid = 0;
    current_item_index = 0;
    sys_index = restore_index;

    if (sys_index < 0) {
        int fwd_idx = fwd_hist_get(dest);
        if (fwd_idx >= 0) sys_index = fwd_idx;
    }

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, sys_dir);

    create_content_items();
    ui_count = (int) item_count;

    // This took almost an hour to fucking correct, so if grid or carousel mode is enabled
    // and then content is in a list view we need to swap our navigation around so that we
    // can use the UP and DOWN on the DPAD instead of LEFT and RIGHT which happened before
    swap_axis = (theme.MISC.NAVIGATION_TYPE == 1 || (grid_mode_enabled && theme.GRID.NAVIGATION_TYPE >= 1 && theme.GRID.NAVIGATION_TYPE <= 5));

    if (ui_count > 0) {
        if (sys_index >= 0 && sys_index < ui_count) {
            current_item_index = sys_index;
        } else {
            current_item_index = 0;
        }
    }

    int nav_vis = (ui_count > 0);
    int collect_vis = (ui_count > 0 && items[current_item_index].content_type == ITEM);

    if (nav_vis) {
        lv_obj_add_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);

        focus_initial();
        first_open = 0;

        set_label_long_mode(&theme, lv_group_get_focused(ui_group));

        nav_moved = 1;
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXPLORE.NONE);
        lv_obj_clear_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);

        current_content_label[0] = '\0';
        lv_obj_add_flag(ui_lblGridCurrentItem, MU_OBJ_FLAG_HIDE_FLOAT);

        nav_moved = 0;
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

    if (is_ksk(kiosk.LAUNCH.COLLECTION) || is_ksk(kiosk.COLLECT.ADD_CON)) {
        lv_obj_add_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    update_file_counter(ui_lblCounter_explore, file_count);
    adjust_panels();
}

int muxplore_main(int index, char *dir) {
    exit_status = 0;
    sys_index = -1;
    file_count = 0;
    dir_count = 0;
    starter_image = 0;
    splash_valid = 0;

    if (dir && *dir) {
        snprintf(sys_dir, sizeof(sys_dir), "%s", dir);
    } else if (file_exist(MUOS_PDI_LOAD)) {
        char *saved_dir = read_all_char_from(MUOS_PDI_LOAD);
        if (saved_dir && *saved_dir) {
            snprintf(sys_dir, sizeof(sys_dir), "%s", saved_dir);
        } else {
            union_get_roms_root(sys_dir, sizeof(sys_dir));
        }
        free(saved_dir);
    } else {
        union_get_roms_root(sys_dir, sizeof(sys_dir));
    }

    if (file_exist(EXPLORE_DIR)) remove(EXPLORE_DIR);
    if (index > 0) sys_index = index;

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
    load_tag_items(&tag_items, &tag_item_count);
    create_content_items();
    ui_count = (int) item_count;
    init_elements();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, sys_dir);

    int nav_vis = 0;
    int collect_vis = 0;
    if (ui_count > 0) {
        nav_vis = 1;

        if (sys_index >= 0 && sys_index < ui_count) current_item_index = sys_index;
        else current_item_index = 0;

        focus_initial();
        first_open = 0;

        collect_vis = (items[current_item_index].content_type == ITEM);
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXPLORE.NONE);
        lv_obj_clear_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
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

    init_timer(ui_refresh_task, NULL);

    if (ui_count > 0) set_label_long_mode(&theme, lv_group_get_focused(ui_group));

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1 ||
                          (grid_mode_enabled && theme.GRID.NAVIGATION_TYPE >= 1 && theme.GRID.NAVIGATION_TYPE <= 5)),
            .press_handler = {
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_START] = handle_start,
                    [MUX_INPUT_SELECT] = handle_select,
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
                    [MUX_INPUT_MENU] = handle_help,
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
    free_tag_items(&tag_items, &tag_item_count);

    return exit_status;
}
