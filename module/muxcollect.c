#include "muxshare.h"
#include "ui/ui_muxcollect.h"

static lv_obj_t *ui_imgSplash;
static lv_obj_t *ui_viewport_objects[7];

static char *prev_dir = "";
static char sys_dir[MAX_BUFFER_SIZE];
static char new_dir[MAX_BUFFER_SIZE];

static int exit_status = 0;
static int add_mode = 0;
static int sys_index = -1;
static int file_count = 0;
static int dir_count = 0;
static int starter_image = 0;
static int splash_valid = 0;
static int nogrid_file_exists = 0;

static char current_meta_text[MAX_BUFFER_SIZE];
static char current_content_label[MAX_BUFFER_SIZE];
static char box_image_previous_path[MAX_BUFFER_SIZE];
static char preview_image_previous_path[MAX_BUFFER_SIZE];
static char splash_image_previous_path[MAX_BUFFER_SIZE];

static void check_for_disable_grid_file(char *item_curr_dir) {
    char no_grid_path[PATH_MAX];
    snprintf(no_grid_path, sizeof(no_grid_path), "%s/.nogrid", item_curr_dir);
    nogrid_file_exists = file_exist(no_grid_path);
}

static char *load_content_governor(char *pointer) {
    char content_gov[MAX_BUFFER_SIZE];
    snprintf(content_gov, sizeof(content_gov), "%s.gov",
             strip_ext(pointer));

    if (file_exist(content_gov)) {
        LOG_SUCCESS(mux_module, "Loading Individual Governor: %s", content_gov)
        return read_all_char_from(content_gov);
    } else {
        snprintf(content_gov, sizeof(content_gov), "%s/%s/core.gov",
                 INFO_COR_PATH, str_replace(get_last_subdir(pointer, '/', 6), get_last_dir(pointer), ""));
    }

    snprintf(content_gov, sizeof(content_gov), "%s", str_replace(content_gov, "//", "/"));

    if (file_exist(content_gov)) {
        LOG_SUCCESS(mux_module, "Loading Global Governor: %s", content_gov)
        return read_all_char_from(content_gov);
    }

    LOG_INFO(mux_module, "No governor detected")
    return NULL;
}

static char *load_content_description() {
    char core_file[MAX_BUFFER_SIZE];
    snprintf(core_file, sizeof(core_file), "%s/%s.cfg",
             sys_dir, strip_ext(items[current_item_index].name));

    char pointer[MAX_BUFFER_SIZE];
    snprintf(pointer, sizeof(pointer), "%s/%s",
             INFO_COR_PATH, get_last_subdir(read_line_char_from(core_file, 1), '/', 6));

    char *h_file_name = items[current_item_index].content_type == FOLDER ? items[current_item_index].name : strip_ext(
            read_line_char_from(pointer, 7));
    char *h_core_artwork =
            items[current_item_index].content_type == FOLDER ? "Collection" : read_line_char_from(pointer, 3);

    char content_desc[MAX_BUFFER_SIZE];
    snprintf(content_desc, sizeof(content_desc), "%s/%s/text/%s.txt",
             INFO_CAT_PATH, h_core_artwork, h_file_name);

    if (file_exist(content_desc)) {
        return read_all_char_from(content_desc);
    }

    snprintf(current_meta_text, sizeof(current_meta_text), " ");
    return lang.GENERIC.NO_INFO;
}

static void update_file_counter() {
    if ((ui_count > 0 && file_count == 0 && config.VISUAL.COUNTERFOLDER) ||
        (file_count > 0 && config.VISUAL.COUNTERFILE)) {
        char counter_text[MAX_BUFFER_SIZE];
        snprintf(counter_text, sizeof(counter_text), "%d%s%d", current_item_index + 1, theme.COUNTER.TEXT_SEPARATOR,
                 ui_count);
        counter_message(ui_lblCounter_collect, counter_text, theme.COUNTER.TEXT_FADE_TIME * 60);
    } else {
        lv_obj_add_flag(ui_lblCounter_collect, LV_OBJ_FLAG_HIDDEN);
    }
}

static void viewport_refresh(char *artwork_config, char *catalogue_folder, char *content_name) {
    mini_t *artwork_config_ini = mini_try_load(artwork_config);

    int device_width = device.MUX.WIDTH / 2;

    int16_t viewport_width = get_ini_int(artwork_config_ini, "viewport", "WIDTH", (int16_t) device_width);
    int16_t viewport_height = get_ini_int(artwork_config_ini, "viewport", "HEIGHT", 400);
    int16_t column_mode = get_ini_int(artwork_config_ini, "viewport", "COLUMN_MODE", 0);
    int16_t column_mode_alignment = get_ini_int(artwork_config_ini, "viewport", "COLUMN_MODE_ALIGNMENT", 2);

    lv_obj_set_width(ui_viewport_objects[0], viewport_width == 0 ? LV_SIZE_CONTENT : viewport_width);
    lv_obj_set_height(ui_viewport_objects[0], viewport_height == 0 ? LV_SIZE_CONTENT : viewport_height);
    if (column_mode) {
        lv_obj_set_flex_flow(ui_viewport_objects[0], LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(ui_viewport_objects[0], LV_FLEX_ALIGN_CENTER, column_mode_alignment,
                              LV_FLEX_ALIGN_CENTER);
    }

    for (int index = 1; index < 6; index++) {
        char section_name[15];
        snprintf(section_name, sizeof(section_name), "image%d", index);
        char *folder_name = get_ini_string(artwork_config_ini, section_name, "FOLDER", "");

        char image[MAX_BUFFER_SIZE];
        snprintf(image, sizeof(image), "%s/%s/%s/%s.png",
                 INFO_CAT_PATH, catalogue_folder, folder_name, content_name);

        if (!file_exist(image)) {
            snprintf(image, sizeof(image), "%s/%s/%s/default.png",
                     INFO_CAT_PATH, catalogue_folder, folder_name);
        }

        struct ImageSettings image_settings = {
                image,
                get_ini_int(artwork_config_ini, section_name, "ALIGN", 9),
                get_ini_int(artwork_config_ini, section_name, "MAX_WIDTH", 0),
                get_ini_int(artwork_config_ini, section_name, "MAX_HEIGHT", 0),
                get_ini_int(artwork_config_ini, section_name, "PAD_LEFT", 0),
                get_ini_int(artwork_config_ini, section_name, "PAD_RIGHT", 0),
                get_ini_int(artwork_config_ini, section_name, "PAD_TOP", 0),
                get_ini_int(artwork_config_ini, section_name, "PAD_BOTTOM", 0)
        };

        update_image(ui_viewport_objects[index], image_settings);
    }

    mini_free(artwork_config_ini);
}

static void image_refresh(char *image_type) {
    if (strcasecmp(image_type, "box") == 0 && config.VISUAL.BOX_ART == 8) return;

    char mux_dimension[15];
    get_mux_dimension(mux_dimension, sizeof(mux_dimension));

    char image[MAX_BUFFER_SIZE];
    char image_path[MAX_BUFFER_SIZE];
    char core_artwork[MAX_BUFFER_SIZE];

    char core_file[MAX_BUFFER_SIZE];
    snprintf(core_file, sizeof(core_file), "%s/%s.cfg",
             sys_dir, strip_ext(items[current_item_index].name));

    char pointer[MAX_BUFFER_SIZE];
    snprintf(pointer, sizeof(pointer), "%s/%s",
             INFO_COR_PATH, get_last_subdir(read_line_char_from(core_file, 1), '/', 6));

    char *h_file_name = items[current_item_index].content_type == FOLDER ? items[current_item_index].name : strip_ext(
            read_line_char_from(pointer, 7));

    char *h_core_artwork =
            items[current_item_index].content_type == FOLDER ? "Collection" : read_line_char_from(pointer, 3);

    if (strlen(h_core_artwork) <= 1) {
        snprintf(image, sizeof(image), "%s/%simage/none_%s.png",
                 STORAGE_THEME, mux_dimension, image_type);
        if (!file_exist(image)) {
            snprintf(image, sizeof(image), "%s/image/none_%s.png",
                     STORAGE_THEME, image_type);
        }
    } else {
        load_image_catalogue(h_core_artwork, h_file_name, "default", mux_dimension, image_type,
                             image, sizeof(image));
        if (!strcasecmp(image_type, "splash") && !file_exist(image)) {
            load_splash_image_fallback(mux_dimension, image, sizeof(image));
        }
    }
    snprintf(core_artwork, sizeof(core_artwork), "%s", h_core_artwork);

    LOG_INFO(mux_module, "Loading '%s' Artwork: %s", image_type, image)

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
            char artwork_config_path[MAX_BUFFER_SIZE];
            snprintf(artwork_config_path, sizeof(artwork_config_path), "%s/%s.ini",
                     INFO_CAT_PATH, h_core_artwork);
            if (!file_exist(artwork_config_path)) {
                snprintf(artwork_config_path, sizeof(artwork_config_path), "%s/default.ini",
                         INFO_CAT_PATH);
            }

            if (file_exist(artwork_config_path)) {
                viewport_refresh(artwork_config_path, h_core_artwork, h_file_name);
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

static int32_t get_directory_item_count(const char *base_dir, const char *dir_name) {
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, dir_name);

    DIR *dir = opendir(full_path);
    if (!dir) {
        perror(lang.SYSTEM.FAIL_DIR_OPEN);
        return 0;
    }

    struct dirent *entry;
    int32_t dir_count = 0;

    while ((entry = readdir(dir))) if (entry->d_type == DT_REG) dir_count++;

    closedir(dir);
    return dir_count;
}

static void add_directory_and_file_names(const char *base_dir, char ***dir_names, char ***file_names) {
    file_count = 0;
    dir_count = 0;
    struct dirent *entry;
    DIR *dir = opendir(base_dir);

    if (!dir) {
        perror(lang.SYSTEM.FAIL_DIR_OPEN);
        return;
    }

    while ((entry = readdir(dir))) {
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, entry->d_name);
        if (entry->d_type == DT_DIR && at_base(sys_dir, "collection")) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                char *subdir_path = (char *) malloc(strlen(entry->d_name) + 2);
                snprintf(subdir_path, strlen(entry->d_name) + 2, "%s", entry->d_name);

                *dir_names = (char **) realloc(*dir_names, (dir_count + 1) * sizeof(char *));
                (*dir_names)[dir_count] = subdir_path;
                (dir_count)++;
            }
        } else if (entry->d_type == DT_REG) {
            char *file_path = (char *) malloc(strlen(entry->d_name) + 2);
            snprintf(file_path, strlen(entry->d_name) + 2, "%s", entry->d_name);

            *file_names = (char **) realloc(*file_names, (file_count + 1) * sizeof(char *));
            (*file_names)[file_count] = file_path;
            (file_count)++;
        }
    }

    closedir(dir);
}

static void gen_label(char *item_glyph, char *item_text) {
    lv_obj_t *ui_pnlCollection = lv_obj_create(ui_pnlContent);
    lv_obj_t *ui_lblCollectionItem = lv_label_create(ui_pnlCollection);
    lv_obj_t *ui_lblCollectionItemGlyph = lv_img_create(ui_pnlCollection);

    lv_group_add_obj(ui_group, ui_lblCollectionItem);
    lv_group_add_obj(ui_group_glyph, ui_lblCollectionItemGlyph);
    lv_group_add_obj(ui_group_panel, ui_pnlCollection);

    apply_theme_list_panel(ui_pnlCollection);
    apply_theme_list_item(&theme, ui_lblCollectionItem, item_text);
    apply_theme_list_glyph(&theme, ui_lblCollectionItemGlyph, mux_module, item_glyph);

    apply_size_to_content(&theme, ui_pnlContent, ui_lblCollectionItem, ui_lblCollectionItemGlyph, item_text);
    apply_text_long_dot(&theme, ui_pnlContent, ui_lblCollectionItem, item_text);
}

static void gen_item(char **file_names, int file_count) {
    char custom_lookup[MAX_BUFFER_SIZE];
    snprintf(custom_lookup, sizeof(custom_lookup), "%s/content.json",
             INFO_NAM_PATH);

    int fn_valid = 0;
    struct json fn_json = {0};

    if (json_valid(read_all_char_from(custom_lookup))) {
        fn_valid = 1;
        fn_json = json_parse(read_all_char_from(custom_lookup));
    }

    for (int i = 0; i < file_count; i++) {
        int has_custom_name = 0;
        char fn_name[MAX_BUFFER_SIZE];
        char collection_file[MAX_BUFFER_SIZE];
        snprintf(collection_file, sizeof(collection_file), "%s/%s",
                 sys_dir, file_names[i]);
        const char *stripped_name = read_line_char_from(collection_file, 3);
        if (stripped_name && stripped_name[0] == '\0') {
            const char *cache_file = read_line_char_from(collection_file, 1);
            stripped_name = strip_ext(read_line_char_from(cache_file, 7));
        }

        if (fn_valid) {
            struct json custom_lookup_json = json_object_get(fn_json, stripped_name);
            if (json_exists(custom_lookup_json)) {
                json_string_copy(custom_lookup_json, fn_name, sizeof(fn_name));
                has_custom_name = 1;
            }
        }

        if (!has_custom_name) {
            const char *lookup_result = lookup(stripped_name);
            snprintf(fn_name, sizeof(fn_name), "%s",
                     lookup_result ? lookup_result : stripped_name);
        }

        char curr_item[MAX_BUFFER_SIZE];
        snprintf(curr_item, sizeof(curr_item), "%s :: %d", fn_name, ui_count++);

        content_item *new_item = add_item(&items, &item_count, file_names[i], fn_name, "", ITEM);
        adjust_visual_label(new_item->display_name, config.VISUAL.NAME, config.VISUAL.DASH);
    }

    sort_items(items, item_count);

    for (size_t i = 0; i < item_count; i++) {
        if (items[i].content_type == ITEM) {
            gen_label("collection", items[i].display_name);
        }
    }
}

static char *get_friendly_folder_name(char *folder_name, int fn_valid, struct json fn_json) {
    char *friendly_folder_name = (char *) malloc(MAX_BUFFER_SIZE);
    strcpy(friendly_folder_name, folder_name);
    if (!config.VISUAL.FRIENDLYFOLDER || !fn_valid) return friendly_folder_name;

    struct json good_name_json = json_object_get(fn_json, str_tolower(strdup(folder_name)));
    if (json_exists(good_name_json)) json_string_copy(good_name_json, friendly_folder_name, MAX_BUFFER_SIZE);

    return friendly_folder_name;
}

static void update_title(char *folder_path, int fn_valid, struct json fn_json) {
    char *display_title = get_friendly_folder_name(get_last_dir(folder_path), fn_valid, fn_json);
    adjust_visual_label(display_title, config.VISUAL.NAME, config.VISUAL.DASH);

    char title[PATH_MAX];
    char *label = lang.MUXCOLLECT.TITLE;
    char *module_type = "";
    char *module_path = INFO_COL_PATH;

    if (!config.VISUAL.TITLEINCLUDEROOTDRIVE) module_type = "";

    folder_path = str_replace(folder_path, "/", "");
    module_path = str_replace(module_path, "/", "");

    if (strcasecmp(folder_path, module_path) == 0 && label[0] != '\0') {
        snprintf(title, sizeof(title), "%s%s",
                 label, module_type);
    } else {
        snprintf(title, sizeof(title), "%s%s",
                 display_title, module_type);
    }

    lv_label_set_text(ui_lblTitle, title);
    free(display_title);
}

static void init_navigation_group_grid() {
    grid_mode_enabled = 1;
    init_grid_info((int) item_count, theme.GRID.COLUMN_COUNT);
    create_grid_panel(&theme, (int) item_count);
    load_font_section(FONT_PANEL_FOLDER, ui_pnlGrid);
    load_font_section(FONT_PANEL_FOLDER, ui_lblGridCurrentItem);

    for (size_t i = 0; i < item_count; i++) {
        if (strcasecmp(items[i].name, prev_dir) == 0) sys_index = (int) i;

        uint8_t col = i % theme.GRID.COLUMN_COUNT;
        uint8_t row = i / theme.GRID.COLUMN_COUNT;

        lv_obj_t *cell_panel = lv_obj_create(ui_pnlGrid);
        lv_obj_t *cell_image = lv_img_create(cell_panel);
        lv_obj_t *cell_label = lv_label_create(cell_panel);

        char mux_dimension[15];
        get_mux_dimension(mux_dimension, sizeof(mux_dimension));

        char grid_image[MAX_BUFFER_SIZE];
        load_image_catalogue("Collection", strip_ext(items[i].name), "default", mux_dimension, "grid",
                             grid_image, sizeof(grid_image));

        char glyph_name_focused[MAX_BUFFER_SIZE];
        snprintf(glyph_name_focused, sizeof(glyph_name_focused), "%s_focused", strip_ext(items[i].name));

        char grid_image_focused[MAX_BUFFER_SIZE];
        load_image_catalogue("Collection", glyph_name_focused, "default_focused", mux_dimension, "grid",
                             grid_image_focused, sizeof(grid_image_focused));

        create_grid_item(&theme, cell_panel, cell_label, cell_image, col, row,
                         grid_image, grid_image_focused, items[i].display_name);

        lv_group_add_obj(ui_group, cell_label);
        lv_group_add_obj(ui_group_glyph, cell_image);
        lv_group_add_obj(ui_group_panel, cell_panel);
    }
}

static void create_collection_items() {
    char **dir_names = NULL;
    char **file_names = NULL;
    add_directory_and_file_names(sys_dir, &dir_names, &file_names);

    int fn_valid = 0;
    struct json fn_json = {0};

    if (config.VISUAL.FRIENDLYFOLDER) {
        char folder_name_file[MAX_BUFFER_SIZE];
        snprintf(folder_name_file, sizeof(folder_name_file), "%s/folder.json",
                 INFO_NAM_PATH);

        char *file_content = read_all_char_from(folder_name_file);
        if (file_content && json_valid(file_content)) {
            fn_valid = 1;
            fn_json = json_parse(file_content);
        }

        free(file_content);
    }

    update_title(sys_dir, fn_valid, fn_json);

    if (dir_count > 0 || file_count > 0) {
        if (at_base(sys_dir, "collection")) {
            for (int i = 0; i < dir_count; i++) {
                char *friendly_folder_name = get_friendly_folder_name(dir_names[i], fn_valid, fn_json);

                content_item *new_item = add_item(&items, &item_count, dir_names[i], friendly_folder_name, "", FOLDER);
                adjust_visual_label(new_item->display_name, config.VISUAL.NAME, config.VISUAL.DASH);

                if (config.VISUAL.FOLDERITEMCOUNT) {
                    char display_name[MAX_BUFFER_SIZE];
                    snprintf(display_name, sizeof(display_name), "%s (%d)",
                             new_item->display_name, get_directory_item_count(sys_dir, new_item->name));
                    new_item->display_name = strdup(display_name);
                }

                free(dir_names[i]);
                free(friendly_folder_name);
            }
        }

        sort_items(items, item_count);
        check_for_disable_grid_file(sys_dir);

        if (!nogrid_file_exists && theme.GRID.ENABLED && dir_count > 0 && file_count == 0) {
            init_navigation_group_grid();
        } else {
            for (int i = 0; i < dir_count; i++) {
                gen_label("folder", items[i].display_name);
                if (strcasecmp(items[i].name, prev_dir) == 0) sys_index = i;
            }
        }

        gen_item(file_names, file_count);

        if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);

        free(file_names);
        free(dir_names);
    }
}

static int load_content(const char *content_name) {
    char pointer_file[MAX_BUFFER_SIZE];
    snprintf(pointer_file, sizeof(pointer_file), "%s/%s",
             sys_dir, content_name);

    char cache_file[MAX_BUFFER_SIZE];
    snprintf(cache_file, sizeof(cache_file), "%s",
             read_line_char_from(pointer_file, 1));

    char *assigned_gov = NULL;
    assigned_gov = load_content_governor(cache_file);
    if (!assigned_gov) assigned_gov = device.CPU.DEFAULT;

    if (file_exist(cache_file)) {
        char *assigned_core = read_line_char_from(cache_file, 2);
        LOG_INFO(mux_module, "Assigned Core: %s", assigned_core)
        LOG_INFO(mux_module, "Assigned Governor: %s", assigned_gov)
        LOG_INFO(mux_module, "Using Configuration: %s", cache_file)

        char add_to_history[MAX_BUFFER_SIZE];
        snprintf(add_to_history, sizeof(add_to_history), "%s/%s",
                 INFO_HIS_PATH, content_name);

        write_text_to_file(add_to_history, "w", CHAR, read_all_char_from(pointer_file));
        write_text_to_file(LAST_PLAY_FILE, "w", CHAR, read_line_char_from(pointer_file, 1));
        write_text_to_file(MUOS_GOV_LOAD, "w", CHAR, assigned_gov);
        write_text_to_file(MUOS_ROM_LOAD, "w", CHAR, read_all_char_from(cache_file));
        return 1;
    }

    toast_message(lang.MUXCOLLECT.ERROR.LOAD, 1000);
    LOG_ERROR(mux_module, "Cache Pointer Not Found: %s", cache_file)

    return 0;
}

static void update_footer_glyph() {
    if (!add_mode) return;
    lv_label_set_text(ui_lblNavA,
                      items[current_item_index].content_type == FOLDER ? lang.GENERIC.OPEN : lang.GENERIC.ADD);
}

static void list_nav_move(int steps, int direction) {
    if (ui_count <= 0) return;
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            items[current_item_index].display_name);

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    if (grid_mode_enabled) {
        update_grid_scroll_position(theme.GRID.COLUMN_COUNT, theme.GRID.ROW_COUNT, theme.GRID.ROW_HEIGHT,
                                    current_item_index, ui_pnlGrid);
    } else {
        update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count,
                               current_item_index, ui_pnlContent);
    }

    set_label_long_mode(&theme, lv_group_get_focused(ui_group), items[current_item_index].display_name);
    lv_label_set_text(ui_lblGridCurrentItem, items[current_item_index].display_name);

    image_refresh("box");
    update_footer_glyph();
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_keyboard_OK_press(void) {
    key_show = 0;

    snprintf(new_dir, sizeof(new_dir), "%s/%s",
             sys_dir, lv_textarea_get_text(ui_txtEntry_collect));
    create_directories(new_dir);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, lv_textarea_get_text(ui_txtEntry_collect));
    load_mux("collection");

    close_input();
    mux_input_stop();
}

static void handle_keyboard_press(void) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    const char *is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    if (strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_OK_press();
    } else if (strcmp(is_key, OSK_UPPER) == 0) {
        lv_btnmatrix_set_map(key_entry, key_upper_map);
    } else if (strcmp(is_key, OSK_CHAR) == 0) {
        lv_btnmatrix_set_map(key_entry, key_special_map);
    } else if (strcmp(is_key, OSK_LOWER) == 0) {
        lv_btnmatrix_set_map(key_entry, key_lower_map);
    } else {
        lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
    }
}

static void add_collection_item() {
    char *base_file_name = read_line_char_from(ADD_MODE_WORK, 1);
    char *cache_file = read_line_char_from(ADD_MODE_WORK, 2);

    char collection_content[MAX_BUFFER_SIZE];
    snprintf(collection_content, sizeof(collection_content), "%s\n%s\n%s",
             cache_file, read_line_char_from(ADD_MODE_WORK, 3), strip_ext(read_line_char_from(cache_file, 7)));

    char collection_file[MAX_BUFFER_SIZE];
    snprintf(collection_file, sizeof(collection_file), "%s/%s-%08X.cfg",
             sys_dir, strip_ext(base_file_name), fnv1a_hash(cache_file));

    write_text_to_file(collection_file, "w", CHAR, collection_content);

    if (file_exist(ADD_MODE_WORK)) remove(ADD_MODE_WORK);
    write_text_to_file(ADD_MODE_DONE, "w", CHAR, "DONE");

    if (file_exist(COLLECTION_DIR)) remove(COLLECTION_DIR);
}

static void handle_a() {
    if (msgbox_active) {
        play_sound(SND_CONFIRM);
        if (lv_obj_has_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    if (key_show) {
        handle_keyboard_press();
        return;
    }

    if (!add_mode && !ui_count) return;

    play_sound(SND_CONFIRM);

    int load_message;

    if (add_mode && !ui_count) {
        add_collection_item();
        goto acq;
    }

    if (items[current_item_index].content_type == FOLDER) {
        load_message = 1;

        char n_dir[MAX_BUFFER_SIZE];
        snprintf(n_dir, sizeof(n_dir), "%s/%s",
                 sys_dir, items[current_item_index].name);

        write_text_to_file(COLLECTION_DIR, "w", CHAR, n_dir);
    } else {
        load_message = 0;
        if (add_mode) {
            add_collection_item();
            goto acq;
        } else {
            write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);
            char f_content[MAX_BUFFER_SIZE];
            snprintf(f_content, sizeof(f_content), "%s.cfg",
                     strip_ext(items[current_item_index].name));

            if (load_content(f_content)) {
                if (config.VISUAL.LAUNCHSPLASH) {
                    image_refresh("splash");
                    if (splash_valid) {
                        lv_obj_center(ui_imgSplash);
                        lv_obj_move_foreground(ui_imgSplash);
                        lv_obj_move_foreground(overlay_image);

                        for (unsigned int i = 0; i <= 255; i += 15) {
                            lv_obj_set_style_img_opa(ui_imgSplash, i, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_task_handler();
                            usleep(128);
                        }

                        sleep(1);
                    }
                }

                config.VISUAL.BLACKFADE ? fade_to_black(ui_screen) : unload_image_animation();
                exit_status = 1;
            } else {
                return;
            }
        }
    }

    if (load_message) {
        toast_message(lang.GENERIC.LOADING, 0);
        lv_obj_move_foreground(ui_pnlMessage);

        // Refresh and add a small delay to actually display the message!
        lv_task_handler();
        usleep(256);
    }

    acq:
    if (file_exist(ADD_MODE_DONE)) {
        load_mux(read_all_char_from(ADD_MODE_FROM));
    } else {
        load_mux("collection");
    }

    close_input();
    mux_input_stop();
}

static void handle_b() {
    if (msgbox_active) {
        play_sound(SND_CONFIRM);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (key_show) {
        close_osk(key_entry, ui_group, ui_txtEntry_collect, ui_pnlEntry_collect);
        return;
    }

    play_sound(SND_BACK);

    if (at_base(sys_dir, "collection")) {
        if (file_exist(ADD_MODE_WORK)) remove(ADD_MODE_WORK);
        if (add_mode) write_text_to_file(ADD_MODE_DONE, "w", CHAR, "CANCEL");
        if (file_exist(COLLECTION_DIR)) remove(COLLECTION_DIR);
    } else {
        char *base_dir = strrchr(sys_dir, '/');
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

    close_input();
    mux_input_stop();
}

static void handle_x() {
    if (key_show) {
        key_backspace(ui_txtEntry_collect);
        return;
    }

    if (msgbox_active || !ui_count || add_mode) return;

    if (items[current_item_index].content_type == FOLDER) {
        if (get_directory_item_count(sys_dir, items[current_item_index].name) > 0) {
            play_sound(SND_ERROR);
            toast_message(lang.MUXCOLLECT.ERROR.REMOVE_DIR, 1000);
            return;
        } else {
            char empty_dir[MAX_BUFFER_SIZE];
            snprintf(empty_dir, sizeof(empty_dir), "%s/%s",
                     sys_dir, items[current_item_index].name);

            remove(empty_dir);
        }
    } else {
        char collection_file[MAX_BUFFER_SIZE];
        snprintf(collection_file, sizeof(collection_file), "%s/%s.cfg",
                 sys_dir, strip_ext(items[current_item_index].name));

        remove(collection_file);
    }

    load_mux("collection");

    close_input();
    mux_input_stop();
}

static void handle_y() {
    if (msgbox_active) return;

    if (key_show) {
        key_swap();
        return;
    }

    if (at_base(sys_dir, "collection")) {
        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

        key_show = 1;

        lv_obj_clear_flag(ui_pnlEntry_collect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ui_pnlEntry_collect);

        lv_textarea_set_text(ui_txtEntry_collect, "");
    }
}

static void handle_menu() {
    if (msgbox_active || progress_onscreen != -1 || !ui_count) {
        return;
    }

    play_sound(SND_CONFIRM);
    image_refresh("preview");

    lv_obj_add_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlHelp, LV_OBJ_FLAG_HIDDEN);

    show_rom_info(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpPreviewHeader,
                  ui_lblHelpContent,
                  items[current_item_index].display_name,
                  load_content_description());
}

static void handle_random_select() {
    if (msgbox_active || !ui_count) return;

    uint32_t random_select = random() % MAX_BUFFER_SIZE;
    int selected_index = (int) (random_select & INT16_MAX);

    !(selected_index & 1) ? list_nav_next(selected_index) : list_nav_prev(selected_index);
}

static void handle_up(void) {
    if (key_show) {
        key_up();
        return;
    }

    handle_list_nav_up();
}

static void handle_up_hold(void) {
    if (key_show) {
        key_up();
        return;
    }

    handle_list_nav_up_hold();
}

static void handle_down(void) {
    if (key_show) {
        key_down();
        return;
    }

    handle_list_nav_down();
}

static void handle_down_hold(void) {
    if (key_show) {
        key_down();
        return;
    }

    handle_list_nav_down_hold();
}

static void handle_left(void) {
    if (key_show) {
        key_left();
        return;
    }

    handle_list_nav_left();
}

static void handle_right(void) {
    if (key_show) {
        key_right();
        return;
    }

    handle_list_nav_right();
}

static void handle_left_hold(void) {
    if (key_show) {
        key_left();
        return;
    }

    handle_list_nav_left_hold();
}

static void handle_right_hold(void) {
    if (key_show) {
        key_right();
        return;
    }

    handle_list_nav_right_hold();
}

static void handle_l1(void) {
    if (key_show) return;
    handle_list_nav_page_up();
}

static void handle_r1(void) {
    if (key_show) return;
    handle_list_nav_page_down();
}

static void adjust_panels() {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_lblCounter_collect,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            ui_pnlMessage,
            NULL
    });
}

static void init_elements() {
    lv_obj_set_align(ui_imgBox, config.VISUAL.BOX_ART_ALIGN);
    lv_obj_set_align(ui_viewport_objects[0], config.VISUAL.BOX_ART_ALIGN);

    switch (config.VISUAL.BOX_ART) {
        case 0: // Behind
            lv_obj_move_background(ui_pnlBox);
            lv_obj_move_background(ui_pnlWall);
            break;
        case 1: // Front
            lv_obj_move_foreground(ui_pnlBox);
            break;
        case 2: // Fullscreen + Behind
            lv_obj_set_y(ui_pnlBox, 0);
            lv_obj_set_height(ui_pnlBox, device.MUX.HEIGHT);
            lv_obj_move_background(ui_pnlBox);
            lv_obj_move_background(ui_pnlWall);
            break;
        case 3: // Fullscreen + Front
            lv_obj_set_y(ui_pnlBox, 0);
            lv_obj_set_height(ui_pnlBox, device.MUX.HEIGHT);
            lv_obj_move_foreground(ui_pnlBox);
            break;
        case 4: // Disabled
            lv_obj_add_flag(ui_pnlBox, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
            break;
    }

    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph,    "",                  0},
            {ui_lblNavA,         lang.GENERIC.OPEN,   0},
            {ui_lblNavBGlyph,    "",                  0},
            {ui_lblNavB,         lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph,    "",                  0},
            {ui_lblNavX,         lang.GENERIC.REMOVE, 0},
            {ui_lblNavYGlyph,    "",                  0},
            {ui_lblNavY,         lang.GENERIC.NEW,    0},
            {ui_lblNavMenuGlyph, "",                  0},
            {ui_lblNavMenu,      lang.GENERIC.INFO,   0},
            {NULL, NULL,                              0}
    });

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        starter_image = adjust_wallpaper_element(ui_group, starter_image, GENERAL);
        adjust_panels();

        const char *content_label = lv_obj_get_user_data(lv_group_get_focused(ui_group));
        snprintf(current_content_label, sizeof(current_content_label), "%s", content_label);

        if (!lv_obj_has_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
        }

        update_file_counter();
        lv_obj_move_foreground(overlay_image);

        nav_moved = 0;
    }
}

static void on_key_event(struct input_event ev) {
    if (ev.code == KEY_ENTER && ev.value == 1) {
        handle_keyboard_OK_press();
    }

    if (ev.code == KEY_ESC && ev.value == 1) {
        handle_b();
    } else {
        process_key_event(&ev, ui_txtEntry_collect);
    }
}

int muxcollect_main(int add, char *dir, int last_index) {
    exit_status = 0;
    add_mode = add;
    sys_index = last_index;
    file_count = 0;
    dir_count = 0;
    starter_image = 0;
    splash_valid = 0;
    nogrid_file_exists = 0;

    snprintf(sys_dir, sizeof(sys_dir), "%s", (strcmp(dir, "") == 0) ? INFO_COL_PATH : dir);
    init_module("muxcollect");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, "");
    init_muxcollect(ui_screen, &theme);

    ui_viewport_objects[0] = lv_obj_create(ui_pnlBox);
    ui_viewport_objects[1] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[2] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[3] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[4] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[5] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[6] = lv_img_create(ui_viewport_objects[0]);

    ui_imgSplash = lv_img_create(ui_screen);
    lv_obj_set_style_img_opa(ui_imgSplash, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    init_fonts();

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    if (file_exist(MUOS_PDI_LOAD)) prev_dir = read_all_char_from(MUOS_PDI_LOAD);

    create_collection_items();
    init_elements();

    ui_count = dir_count > 0 || file_count > 0 ? (int) lv_group_get_obj_count(ui_group) : 0;

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, get_last_dir(sys_dir));
    if (strcasecmp(read_all_char_from(MUOS_PDI_LOAD), "ROMS") == 0) {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, get_last_subdir(sys_dir, '/', 4));
    }

    if (ui_count > 0) {
        if (sys_index > -1 && sys_index <= ui_count &&
            current_item_index < ui_count) {
            list_nav_move(sys_index, +1);
        } else {
            image_refresh("box");
        }
        nav_moved = 1;
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXCOLLECT.NONE);
    }

    struct nav_flag nav_e[] = {
            {ui_lblNavA,         1},
            {ui_lblNavAGlyph,    1},
            {ui_lblNavX,         1},
            {ui_lblNavXGlyph,    1},
            {ui_lblNavY,         1},
            {ui_lblNavYGlyph,    1},
            {ui_lblNavMenu,      1},
            {ui_lblNavMenuGlyph, 1}
    };

    /* the below is absolutely fucked logic and I hate it */
    if (add_mode) {
        if (at_base(sys_dir, "collection")) {
            if (!ui_count) {
                int hidden[] = {2, 3, 6, 7};
                for (int i = 0; i < 4; ++i) {
                    nav_e[hidden[i]].visible = 0;
                }
                lv_label_set_text(ui_lblNavA, lang.GENERIC.ADD);
            } else {
                int hidden[] = {2, 3};
                for (int i = 0; i < 2; ++i) {
                    nav_e[hidden[i]].visible = 0;
                }
            }
        } else { // not at base
            lv_label_set_text(ui_lblNavA, lang.GENERIC.ADD);
            if (!ui_count) {
                int hidden[] = {2, 3, 4, 5, 6, 7};
                for (int i = 0; i < 6; ++i) {
                    nav_e[hidden[i]].visible = 0;
                }
            } else {
                int hidden[] = {2, 3, 4, 5};
                for (int i = 0; i < 4; ++i) {
                    nav_e[hidden[i]].visible = 0;
                }
            }
        }
    } else { // not in add mode
        if (at_base(sys_dir, "collection")) {
            if (!ui_count) {
                int hidden[] = {0, 1, 2, 3, 6, 7};
                for (int i = 0; i < 6; ++i) {
                    nav_e[hidden[i]].visible = 0;
                }
            }
        } else { // not at base
            if (!ui_count) {
                int hidden[] = {0, 1, 2, 3, 4, 5, 6, 7};
                for (int i = 0; i < 8; ++i) {
                    nav_e[hidden[i]].visible = 0;
                }
            } else {
                int hidden[] = {4, 5};
                for (int i = 0; i < 2; ++i) {
                    nav_e[hidden[i]].visible = 0;
                }
            }
        }
    }

    set_nav_flags(nav_e, sizeof(nav_e) / sizeof(nav_e[0]));
    adjust_panels();

    update_file_counter();
    init_osk(ui_pnlEntry_collect, ui_txtEntry_collect, false);

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1 ||
                          (grid_mode_enabled && theme.GRID.NAVIGATION_TYPE >= 1 && theme.GRID.NAVIGATION_TYPE <= 5)),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
                    [MUX_INPUT_L1] = handle_l1,
                    [MUX_INPUT_R1] = handle_r1,
                    [MUX_INPUT_R2] = handle_random_select,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_down_hold,
                    [MUX_INPUT_DPAD_LEFT] = handle_left_hold,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right_hold,
                    [MUX_INPUT_L1] = handle_l1,
                    [MUX_INPUT_R1] = handle_r1,
                    [MUX_INPUT_R2] = handle_random_select,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    register_key_event_callback(on_key_event);
    mux_input_task(&input_opts);

    free_items(&items, &item_count);

    return exit_status;
}
