#include "muxshare.h"
#include "muxhistory.h"
#include "ui/ui_muxhistory.h"
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "../common/init.h"
#include "../common/img/nothing.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/json/json.h"
#include "../common/input/list_nav.h"
#include "../common/log.h"
#include "../lookup/lookup.h"

static lv_obj_t *ui_imgSplash;

static lv_obj_t *ui_viewport_objects[7];
static lv_obj_t *ui_mux_panels[7];

static int exit_status = 0;
static int file_count = 0;
static int starter_image = 0;
static int splash_valid = 0;

static char current_meta_text[MAX_BUFFER_SIZE];
static char current_content_label[MAX_BUFFER_SIZE];
static char box_image_previous_path[MAX_BUFFER_SIZE];
static char preview_image_previous_path[MAX_BUFFER_SIZE];
static char splash_image_previous_path[MAX_BUFFER_SIZE];

static char *load_content_governor(char *pointer) {
    pointer = read_text_from_file(pointer);

    char content_gov[MAX_BUFFER_SIZE];
    snprintf(content_gov, sizeof(content_gov), "%s.gov",
             strip_ext(pointer));

    if (file_exist(content_gov)) {
        LOG_SUCCESS(mux_module, "Loading Individual Governor: %s", content_gov)
        return read_text_from_file(content_gov);
    } else {
        snprintf(content_gov, sizeof(content_gov), "%s/%s/core.gov",
                 INFO_COR_PATH, str_replace(get_last_subdir(pointer, '/', 6), get_last_dir(pointer), ""));
    }

    snprintf(content_gov, sizeof(content_gov), "%s", str_replace(content_gov, "//", "/"));

    if (file_exist(content_gov)) {
        LOG_SUCCESS(mux_module, "Loading Global Governor: %s", content_gov)
        return read_text_from_file(content_gov);
    }

    LOG_INFO(mux_module, "No governor detected")
    return NULL;
}

static char *load_content_description() {
    char core_file[MAX_BUFFER_SIZE];
    snprintf(core_file, sizeof(core_file), "%s/%s.cfg",
             INFO_HIS_PATH, strip_ext(items[current_item_index].name));

    char pointer[MAX_BUFFER_SIZE];
    snprintf(pointer, sizeof(pointer), "%s/%s",
             INFO_COR_PATH, get_last_subdir(read_line_from_file(core_file, 1), '/', 6));

    char content_desc[MAX_BUFFER_SIZE];
    snprintf(content_desc, sizeof(content_desc), "%s/%s/text/%s.txt",
             INFO_CAT_PATH, read_line_from_file(pointer, 3),
             strip_ext(read_line_from_file(pointer, 7)));

    if (file_exist(content_desc)) {
        return read_text_from_file(content_desc);
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
        fade_label(ui_lblCounter_history, counter_text, 100, theme.COUNTER.TEXT_FADE_TIME * 60);
    } else {
        lv_obj_add_flag(ui_lblCounter_history, LV_OBJ_FLAG_HIDDEN);
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
    if (strcasecmp(image_type, "box") == 0 && config.VISUAL.BOX_ART == 8) {
        printf("BOX ART IS SET TO DISABLED\n");
        return;
    }

    char mux_dimension[15];
    get_mux_dimension(mux_dimension, sizeof(mux_dimension));

    char image[MAX_BUFFER_SIZE];
    char image_path[MAX_BUFFER_SIZE];
    char core_artwork[MAX_BUFFER_SIZE];

    char core_file[MAX_BUFFER_SIZE];
    snprintf(core_file, sizeof(core_file), "%s/%s.cfg",
             INFO_HIS_PATH, strip_ext(items[current_item_index].name));

    char pointer[MAX_BUFFER_SIZE];
    snprintf(pointer, sizeof(pointer), "%s/%s",
             INFO_COR_PATH, get_last_subdir(read_line_from_file(core_file, 1), '/', 6));

    char *h_file_name = strip_ext(read_line_from_file(pointer, 7));

    char *h_core_artwork = read_line_from_file(pointer, 3);
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
                     INFO_CAT_PATH, core_artwork);
            if (!file_exist(artwork_config_path)) {
                snprintf(artwork_config_path, sizeof(artwork_config_path), "%s/default.ini",
                         INFO_CAT_PATH);
            }

            if (file_exist(artwork_config_path)) {
                viewport_refresh(artwork_config_path, core_artwork, h_file_name);
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

static void add_file_names(const char *base_dir, char ***file_names) {
    struct dirent *entry;
    DIR *dir = opendir(base_dir);

    if (!dir) {
        perror(lang.SYSTEM.FAIL_DIR_OPEN);
        return;
    }

    while ((entry = readdir(dir))) {
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, entry->d_name);
        if (entry->d_type == DT_REG) {
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
    lv_obj_t *ui_pnlHistory = lv_obj_create(ui_pnlContent);
    lv_obj_t *ui_lblHistoryItem = lv_label_create(ui_pnlHistory);
    lv_obj_t *ui_lblHistoryItemGlyph = lv_img_create(ui_pnlHistory);

    lv_group_add_obj(ui_group, ui_lblHistoryItem);
    lv_group_add_obj(ui_group_glyph, ui_lblHistoryItemGlyph);
    lv_group_add_obj(ui_group_panel, ui_pnlHistory);

    apply_theme_list_panel(ui_pnlHistory);
    apply_theme_list_item(&theme, ui_lblHistoryItem, item_text);
    apply_theme_list_glyph(&theme, ui_lblHistoryItemGlyph, mux_module, item_glyph);

    apply_size_to_content(&theme, ui_pnlContent, ui_lblHistoryItem, ui_lblHistoryItemGlyph, item_text);
    apply_text_long_dot(&theme, ui_pnlContent, ui_lblHistoryItem, item_text);
}

static char *get_glyph_name(size_t index) {
    char history_file[PATH_MAX];
    snprintf(history_file, sizeof(history_file), "%s/%s.cfg",
             INFO_HIS_PATH, strip_ext(items[index].name));

    const char *file_name = strip_ext(items[index].name);
    const char *system_name = read_line_from_file(history_file, 2);

    if (search_for_config(INFO_COL_PATH, file_name, system_name)) return "collection";
    return "history";
}

static void gen_item(char **file_names, int file_count) {
    char custom_lookup[MAX_BUFFER_SIZE];
    snprintf(custom_lookup, sizeof(custom_lookup), "%s/content.json",
             INFO_NAM_PATH);

    int fn_valid = 0;
    struct json fn_json = {0};

    if (json_valid(read_text_from_file(custom_lookup))) {
        fn_valid = 1;
        fn_json = json_parse(read_text_from_file(custom_lookup));
    }

    for (int i = 0; i < file_count; i++) {
        int has_custom_name = 0;
        char fn_name[MAX_BUFFER_SIZE];
        char collection_file[MAX_BUFFER_SIZE];
        snprintf(collection_file, sizeof(collection_file), "%s/%s",
                 INFO_HIS_PATH, file_names[i]);
        const char *stripped_name = read_line_from_file(collection_file, 3);
        if (stripped_name && stripped_name[0] == '\0') {
            const char *cache_file = read_line_from_file(collection_file, 1);
            stripped_name = strip_ext(read_line_from_file(cache_file, 7));
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

        content_item *new_item = add_item(&items, &item_count, file_names[i], fn_name, "", ROM);
        adjust_visual_label(new_item->display_name, config.VISUAL.NAME, config.VISUAL.DASH);
    }

    sort_items_time(items, item_count);

    char *glyph_icons[item_count];
    for (size_t i = 0; i < item_count; i++) {
        glyph_icons[i] = (items[i].content_type == ROM) ? get_glyph_name(i) : "unknown";
    }

    for (size_t i = 0; i < item_count; i++) {
        if (items[i].content_type == ROM) {
            gen_label(glyph_icons[i], items[i].display_name);
        }
    }
}

static void create_history_items() {
    char **file_names = NULL;

    lv_label_set_text(ui_lblTitle, lang.MUXHISTORY.TITLE);
    add_file_names(INFO_HIS_PATH, &file_names);

    if (file_count > 0) {
        gen_item(file_names, file_count);
        lv_obj_update_layout(ui_pnlContent);
    }

    free(file_names);
}

static void remove_from_history() {
    char history_file[MAX_BUFFER_SIZE];
    snprintf(history_file, sizeof(history_file), "%s/%s.cfg",
             INFO_HIS_PATH, strip_ext(items[current_item_index].name));

    if (file_exist(history_file)) {
        remove(history_file);
        load_mux("history");

        close_input();
        mux_input_stop();
    } else {
        play_sound(SND_ERROR, 0);
        toast_message(lang.MUXHISTORY.ERROR.REMOVE, 1000, 1000);
    }
}

static void add_to_collection() {
    char pointer_file[MAX_BUFFER_SIZE];
    snprintf(pointer_file, sizeof(pointer_file), INFO_HIS_PATH "/%s",
             items[current_item_index].name);

    char history_content[MAX_BUFFER_SIZE];
    snprintf(history_content, sizeof(history_content), "%s.cfg\n%s\n%s",
             strip_ext(items[current_item_index].name),
             read_line_from_file(pointer_file, 1),
             read_line_from_file(pointer_file, 2)
    );

    write_text_to_file(ADD_MODE_WORK, "w", CHAR, history_content);
    write_text_to_file(ADD_MODE_FROM, "w", CHAR, "history");

    write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

    load_mux("collection");

    close_input();
    mux_input_stop();
}

static int load_content(const char *content_name) {
    char pointer_file[MAX_BUFFER_SIZE];
    snprintf(pointer_file, sizeof(pointer_file), INFO_HIS_PATH "/%s",
             content_name);

    char *assigned_gov = NULL;
    assigned_gov = load_content_governor(pointer_file);
    if (!assigned_gov) assigned_gov = device.CPU.DEFAULT;

    char cache_file[MAX_BUFFER_SIZE];
    snprintf(cache_file, sizeof(cache_file), "%s",
             read_line_from_file(pointer_file, 1));

    if (file_exist(cache_file)) {
        char *assigned_core = read_line_from_file(cache_file, 2);
        LOG_INFO(mux_module, "Assigned Core: %s", assigned_core)
        LOG_INFO(mux_module, "Assigned Governor: %s", assigned_gov)
        LOG_INFO(mux_module, "Using Configuration: %s", cache_file)

        char add_to_history[MAX_BUFFER_SIZE];
        snprintf(add_to_history, sizeof(add_to_history), "%s/%s",
                 INFO_HIS_PATH, content_name);

        write_text_to_file(add_to_history, "w", CHAR, read_text_from_file(pointer_file));
        write_text_to_file(LAST_PLAY_FILE, "w", CHAR, read_line_from_file(pointer_file, 1));
        write_text_to_file(MUOS_GVR_LOAD, "w", CHAR, assigned_gov);
        write_text_to_file(MUOS_ROM_LOAD, "w", CHAR, read_text_from_file(cache_file));
        return 1;
    }

    toast_message(lang.MUXHISTORY.ERROR.LOAD, 1000, 1000);
    LOG_ERROR(mux_module, "Cache Pointer Not Found: %s", cache_file)

    return 0;
}


static void list_nav_move(int steps, int direction) {
    if (ui_count <= 0) return;
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE, 0);

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

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group), items[current_item_index].display_name);

    image_refresh("box");
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a() {
    if (!ui_count) return;

    if (msgbox_active) {
        play_sound(SND_CONFIRM, 0);
        if (lv_obj_has_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    play_sound(SND_CONFIRM, 0);

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

        if (config.VISUAL.BLACKFADE) {
            fade_to_black(ui_screen);
        } else {
            unload_image_animation();
        }
        exit_status = 1;
    } else {
        return;
    }

    load_mux("history");
    close_input();
    mux_input_stop();
}

static void handle_b() {
    if (msgbox_active) {
        play_sound(SND_CONFIRM, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK, 0);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "history");
    close_input();
    mux_input_stop();
}

static void handle_x() {
    if (msgbox_active || !ui_count) return;

    play_sound(SND_CONFIRM, 0);
    remove_from_history();
}

static void handle_y() {
    if (msgbox_active || !ui_count) return;

    play_sound(SND_CONFIRM, 0);
    add_to_collection();
}

static void handle_menu() {
    if (msgbox_active || progress_onscreen != -1 || !ui_count) {
        return;
    }

    play_sound(SND_CONFIRM, 0);
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

    !(selected_index & 1) ? list_nav_move(selected_index, +1) : list_nav_move(selected_index, -1);
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

    ui_mux_panels[0] = ui_pnlFooter;
    ui_mux_panels[1] = ui_pnlHeader;
    ui_mux_panels[2] = ui_lblCounter_history;
    ui_mux_panels[3] = ui_pnlHelp;
    ui_mux_panels[4] = ui_pnlProgressBrightness;
    ui_mux_panels[5] = ui_pnlProgressVolume;
    ui_mux_panels[6] = ui_pnlMessage;

    adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

    if (bar_footer) lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, "");

    lv_label_set_text(ui_lblNavA, lang.GENERIC.OPEN);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);
    lv_label_set_text(ui_lblNavX, lang.GENERIC.REMOVE);
    lv_label_set_text(ui_lblNavY, lang.GENERIC.COLLECT);
    lv_label_set_text(ui_lblNavMenu, lang.GENERIC.INFO);

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavBGlyph,
            ui_lblNavB,
            ui_lblNavXGlyph,
            ui_lblNavX,
            ui_lblNavYGlyph,
            ui_lblNavY,
            ui_lblNavMenuGlyph,
            ui_lblNavMenu,
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    }

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

static void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        starter_image = adjust_wallpaper_element(ui_group, starter_image, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

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

int muxhistory_main(int his_index) {
    exit_status = 0;
    file_count = 0;
    starter_image = 0;
    splash_valid = 0;

    init_module("muxhistory");

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, "");
    init_muxhistory(ui_screen, &theme);

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
    init_elements();

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    create_history_items();

    int nav_vis = 0;
    if (ui_count > 0) {
        nav_vis = 1;
        if (his_index > -1 && his_index <= ui_count && current_item_index < ui_count) {
            list_nav_move(his_index, +1);
        } else {
            image_refresh("box");
        }
        nav_moved = 1;
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXHISTORY.NONE);
    }

    struct nav_flag nav_e[] = {
            {ui_lblNavA,         nav_vis},
            {ui_lblNavAGlyph,    nav_vis},
            {ui_lblNavX,         nav_vis},
            {ui_lblNavXGlyph,    nav_vis},
            {ui_lblNavY,         nav_vis},
            {ui_lblNavYGlyph,    nav_vis},
            {ui_lblNavMenu,      nav_vis},
            {ui_lblNavMenuGlyph, nav_vis}
    };
    set_nav_flags(nav_e, sizeof(nav_e) / sizeof(nav_e[0]));

    update_file_counter();
    load_kiosk(&kiosk);

    if (file_exist(ADD_MODE_DONE)) {
        if (!strcasecmp(read_text_from_file(ADD_MODE_DONE), "DONE")) {
            toast_message(lang.GENERIC.ADD_COLLECT, 1000, 1000);
            refresh_screen(ui_screen);
        }
        remove(ADD_MODE_DONE);
    }

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_list_nav_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_list_nav_right,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
                    [MUX_INPUT_R2] = handle_random_select,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_DPAD_LEFT] = handle_list_nav_left_hold,
                    [MUX_INPUT_DPAD_RIGHT] = handle_list_nav_right_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
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
