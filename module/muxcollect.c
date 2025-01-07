#include "../lvgl/lvgl.h"
#include "../lvgl/src/drivers/fbdev.h"
#include "../lvgl/src/drivers/evdev.h"
#include "ui/ui_muxcollect.h"
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/img/nothing.h"
#include "../common/common.h"
#include "../common/osk.h"
#include "../common/language.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/collection.h"
#include "../common/json/json.h"
#include "../common/input.h"
#include "../common/input/list_nav.h"
#include "../common/log.h"
#include "../lookup/lookup.h"

#define COLLECTION_DIR "/tmp/collection_dir"

struct theme_config theme;

static int js_fd;
static int js_fd_sys;

int turbo_mode = 0;
int msgbox_active = 0;
int SD2_found = 0;
int nav_sound = 0;
int safe_quit = 0;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;
char *mux_module;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;
lv_obj_t *kiosk_image = NULL;

lv_obj_t *key_entry;

int progress_onscreen = -1;

size_t item_count = 0;
content_item *items = NULL;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

lv_obj_t *ui_imgSplash;

lv_obj_t *ui_viewport_objects[7];
lv_obj_t *ui_mux_panels[7];

char *prev_dir = "";
char *sys_dir = INFO_COL_PATH;
char new_dir[MAX_BUFFER_SIZE];

int add_mode = 0;
int key_show = 0;
int key_curr = 0;
int ui_count = 0;
int sys_index = -1;
int file_count = 0;
int dir_count = 0;
int current_item_index = 0;
int first_open = 1;
int nav_moved = 0;
int counter_fade = 0;
int fade_timeout = 3;
int starter_image = 0;
int splash_valid = 0;
int nogrid_file_exists = 0;

static char current_meta_text[MAX_BUFFER_SIZE];
static char current_content_label[MAX_BUFFER_SIZE];
static char box_image_previous_path[MAX_BUFFER_SIZE];
static char preview_image_previous_path[MAX_BUFFER_SIZE];
static char splash_image_previous_path[MAX_BUFFER_SIZE];

lv_timer_t *datetime_timer;
lv_timer_t *capacity_timer;
lv_timer_t *osd_timer;
lv_timer_t *glyph_timer;
lv_timer_t *ui_refresh_timer;

void check_for_disable_grid_file(char *item_curr_dir) {
    char no_grid_path[PATH_MAX];
    snprintf(no_grid_path, sizeof(no_grid_path), "%s/.nogrid", item_curr_dir);
    nogrid_file_exists = file_exist(no_grid_path);
}

char *load_content_governor(char *pointer) {
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

char *load_content_description() {
    char core_file[MAX_BUFFER_SIZE];
    snprintf(core_file, sizeof(core_file), "%s/%s.cfg",
             sys_dir, strip_ext(items[current_item_index].name));

    char pointer[MAX_BUFFER_SIZE];
    snprintf(pointer, sizeof(pointer), "%s/%s",
             INFO_COR_PATH, get_last_subdir(read_line_from_file(core_file, 1), '/', 6));

    char content_desc[MAX_BUFFER_SIZE];
    snprintf(content_desc, sizeof(content_desc), "%s/%s/text/%s.txt",
             INFO_CAT_PATH, read_line_from_file(pointer, 3),
             strip_ext(read_line_from_file(pointer, 7)));

    if (file_exist(content_desc)) {
        snprintf(current_meta_text, sizeof(current_meta_text), "%s", format_meta_text(content_desc));
        return current_meta_text;
    }

    snprintf(current_meta_text, sizeof(current_meta_text), " ");
    return lang.GENERIC.NO_INFO;
}

void update_file_counter() {
    if ((ui_count > 0 && file_count == 0 && config.VISUAL.COUNTERFOLDER) ||
        (file_count > 0 && config.VISUAL.COUNTERFILE)) {
        fade_timeout = 3;
        lv_obj_clear_flag(ui_lblCounter, LV_OBJ_FLAG_HIDDEN);
        counter_fade = (theme.COUNTER.BORDER_ALPHA + theme.COUNTER.BACKGROUND_ALPHA + theme.COUNTER.TEXT_ALPHA);
        if (counter_fade > 255) counter_fade = 255;
        lv_obj_set_style_opa(ui_lblCounter, counter_fade, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text_fmt(ui_lblCounter, "%d%s%d", current_item_index + 1, theme.COUNTER.TEXT_SEPARATOR, ui_count);
    } else {
        lv_obj_add_flag(ui_lblCounter, LV_OBJ_FLAG_HIDDEN);
    }
}

void viewport_refresh(char *artwork_config, char *catalogue_folder, char *content_name) {
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

void image_refresh(char *image_type) {
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
        snprintf(image_path, sizeof(image_path), "M:%s", image);
    } else {
        snprintf(image, sizeof(image), "%s/%s/%s/%s.png",
                 INFO_CAT_PATH, h_core_artwork, image_type, h_file_name);
        snprintf(image_path, sizeof(image_path), "M:%s/%s/%s/%s.png",
                 INFO_CAT_PATH, h_core_artwork, image_type, h_file_name);
    }
    snprintf(core_artwork, sizeof(core_artwork), "%s", h_core_artwork);

    LOG_INFO(mux_module, "Loading '%s' Artwork: %s", image_type, image)

    if (strcasecmp(image_type, "preview") == 0) {
        if (strcasecmp(preview_image_previous_path, image) != 0) {
            if (!file_exist(image)) {
                snprintf(image, sizeof(image), "%s/default.png", strip_dir(image));
                snprintf(image_path, sizeof(image_path), "M:%s", image);
            }

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
            if (!file_exist(image)) {
                snprintf(image, sizeof(image), "%s/default.png", strip_dir(image));
                snprintf(image_path, sizeof(image_path), "M:%s", image);
            }

            if (file_exist(image)) {
                splash_valid = 1;
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
            char *catalogue_folder = items[current_item_index].content_type == FOLDER ? "Collection" : core_artwork;
            char *content_name =
                    items[current_item_index].content_type == FOLDER ? items[current_item_index].name : h_file_name;
            char artwork_config_path[MAX_BUFFER_SIZE];
            snprintf(artwork_config_path, sizeof(artwork_config_path), "%s/%s.ini",
                     INFO_CAT_PATH, catalogue_folder);
            if (!file_exist(artwork_config_path)) {
                snprintf(artwork_config_path, sizeof(artwork_config_path), "%s/default.ini",
                         INFO_CAT_PATH);
            }

            if (file_exist(artwork_config_path)) {
                viewport_refresh(artwork_config_path, catalogue_folder, content_name);
                snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", image);
            } else {
                if (!file_exist(image)) {
                    snprintf(image, sizeof(image), "%s/default.png", strip_dir(image));
                    snprintf(image_path, sizeof(image_path), "M:%s", image);
                }

                if (file_exist(image)) {
                    starter_image = 1;
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

int32_t get_directory_item_count(const char *base_dir, const char *dir_name) {
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, dir_name);

    struct dirent *entry;
    DIR *dir = opendir(full_path);

    if (!dir) {
        perror(lang.SYSTEM.FAIL_DIR_OPEN);
        return 0;
    }

    load_skip_patterns();

    int32_t dir_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (!should_skip(entry->d_name)) {
            if (entry->d_type == DT_DIR) {
                if (strcasecmp(entry->d_name, ".") != 0 && strcasecmp(entry->d_name, "..") != 0) {
                    dir_count++;
                }
            } else if (entry->d_type == DT_REG) {
                dir_count++;
            }
        }
    }
    closedir(dir);
    return dir_count;
}

void add_directory_and_file_names(const char *base_dir, char ***dir_names, char ***file_names) {
    struct dirent *entry;
    DIR *dir = opendir(base_dir);

    if (!dir) {
        perror(lang.SYSTEM.FAIL_DIR_OPEN);
        return;
    }

    load_skip_patterns();

    while ((entry = readdir(dir)) != NULL) {
        if (!should_skip(entry->d_name)) {
            char full_path[PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, entry->d_name);
            if (entry->d_type == DT_DIR) {
                if (strcasecmp(entry->d_name, ".") != 0 && strcasecmp(entry->d_name, "..") != 0) {
                    int item_dir_count = get_directory_item_count(base_dir, entry->d_name);

                    if (config.VISUAL.FOLDEREMPTY || item_dir_count != 0) {
                        char *subdir_path = (char *) malloc(strlen(entry->d_name) + 2);
                        snprintf(subdir_path, strlen(entry->d_name) + 2, "%s", entry->d_name);

                        *dir_names = (char **) realloc(*dir_names, (dir_count + 1) * sizeof(char *));
                        (*dir_names)[dir_count] = subdir_path;
                        (dir_count)++;
                    }
                }
            } else if (entry->d_type == DT_REG) {
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

void gen_label(char *item_glyph, char *item_text) {
    lv_obj_t *ui_pnlCollection = lv_obj_create(ui_pnlContent);
    apply_theme_list_panel(&theme, &device, ui_pnlCollection);

    lv_obj_t *ui_lblCollectionItem = lv_label_create(ui_pnlCollection);
    apply_theme_list_item(&theme, ui_lblCollectionItem, item_text, true, false);

    lv_obj_t *ui_lblCollectionItemGlyph = lv_img_create(ui_pnlCollection);
    apply_theme_list_glyph(&theme, ui_lblCollectionItemGlyph, mux_module, item_glyph);

    lv_group_add_obj(ui_group, ui_lblCollectionItem);
    lv_group_add_obj(ui_group_glyph, ui_lblCollectionItemGlyph);
    lv_group_add_obj(ui_group_panel, ui_pnlCollection);

    apply_size_to_content(&theme, ui_pnlContent, ui_lblCollectionItem, ui_lblCollectionItemGlyph, item_text);
    apply_text_long_dot(&theme, ui_pnlContent, ui_lblCollectionItem, item_text);
}

void gen_item(char **file_names, int file_count) {
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
                sys_dir, file_names[i]);
        const char *stripped_name = read_line_from_file(collection_file, 3);;

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

    sort_items(items, item_count);

    for (size_t i = 0; i < item_count; i++) {
        if (items[i].content_type == ROM) {
            gen_label("collection", items[i].display_name);
        }
    }
}

char *get_friendly_folder_name(char *folder_name, int fn_valid, struct json fn_json) {
    char *friendly_folder_name = (char *) malloc(MAX_BUFFER_SIZE);
    strcpy(friendly_folder_name, folder_name);
    if (!config.VISUAL.FRIENDLYFOLDER || !fn_valid) return friendly_folder_name;

    struct json good_name_json = json_object_get(fn_json, folder_name);
    if (json_exists(good_name_json)) json_string_copy(good_name_json, friendly_folder_name, MAX_BUFFER_SIZE);

    return friendly_folder_name;
}

void update_title(char *folder_path, int fn_valid, struct json fn_json) {
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

void init_navigation_groups_grid() {
    grid_mode_enabled = 1;
    init_grid_info((int) item_count, theme.GRID.COLUMN_COUNT);
    create_grid_panel(&theme, (int) item_count);
    load_font_section(mux_module, FONT_PANEL_FOLDER, ui_pnlGrid);
    load_font_section(mux_module, FONT_PANEL_FOLDER, ui_lblGridCurrentItem);

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
        if (!load_image_catalogue("Collection", strip_ext(items[i].name), "default", mux_dimension, "grid",
                                  grid_image, sizeof(grid_image))) {
            load_image_catalogue("Collection", strip_ext(items[i].name), "default", "", "grid",
                                 grid_image, sizeof(grid_image));
        }

        char glyph_name_focused[MAX_BUFFER_SIZE];
        snprintf(glyph_name_focused, sizeof(glyph_name_focused), "%s_focused", strip_ext(items[i].name));

        char grid_image_focused[MAX_BUFFER_SIZE];
        if (!load_image_catalogue("Collection", glyph_name_focused, "default_focused", mux_dimension, "grid",
                                  grid_image_focused, sizeof(grid_image_focused))) {
            load_image_catalogue("Collection", glyph_name_focused, "default_focused", "", "grid",
                                 grid_image_focused, sizeof(grid_image_focused));
        }

        create_grid_item(&theme, cell_panel, cell_label, cell_image, col, row,
                         grid_image, grid_image_focused, items[i].display_name);

        lv_group_add_obj(ui_group, cell_label);
        lv_group_add_obj(ui_group_glyph, cell_image);
        lv_group_add_obj(ui_group_panel, cell_panel);
    }
}

void create_collection_items() {
    char **dir_names = NULL;
    char **file_names = NULL;
    add_directory_and_file_names(sys_dir, &dir_names, &file_names);

    int fn_valid = 0;
    struct json fn_json = {0};

    if (config.VISUAL.FRIENDLYFOLDER) {
        char folder_name_file[MAX_BUFFER_SIZE];
        snprintf(folder_name_file, sizeof(folder_name_file), "%s/folder.json",
                 INFO_NAM_PATH);

        if (json_valid(read_text_from_file(folder_name_file))) {
            fn_valid = 1;
            fn_json = json_parse(read_text_from_file(folder_name_file));
        }
    }

    update_title(sys_dir, fn_valid, fn_json);

    if (dir_count > 0 || file_count > 0) {
        for (int i = 0; i < dir_count; i++) {
            content_item *new_item = NULL;
            char *friendly_folder_name = get_friendly_folder_name(dir_names[i], fn_valid, fn_json);
            new_item = add_item(&items, &item_count, dir_names[i], friendly_folder_name, "", FOLDER);
            adjust_visual_label(new_item->display_name, config.VISUAL.NAME, config.VISUAL.DASH);
            if (config.VISUAL.FOLDERITEMCOUNT) {
                char display_name[MAX_BUFFER_SIZE];
                snprintf(display_name, sizeof(display_name), "%s (%d)",
                         new_item->display_name, get_directory_item_count(sys_dir, new_item->name));
                new_item->display_name = strdup(display_name);
            }

            free(friendly_folder_name);
            free(dir_names[i]);
        }

        sort_items(items, item_count);
        check_for_disable_grid_file(sys_dir);

        if (!nogrid_file_exists && theme.GRID.ENABLED && dir_count > 0 && file_count == 0) {
            init_navigation_groups_grid();
        } else {
            for (int i = 0; i < dir_count; i++) {
                gen_label("folder", items[i].display_name);
                if (strcasecmp(items[i].name, prev_dir) == 0) sys_index = i;
            }
        }

        gen_item(file_names, file_count);
        lv_obj_update_layout(ui_pnlContent);
    }

    free(file_names);
    free(dir_names);
}

int load_content(const char *content_name) {
    char pointer_file[MAX_BUFFER_SIZE];
    snprintf(pointer_file, sizeof(pointer_file), "%s/%s",
             sys_dir, content_name);

    char cache_file[MAX_BUFFER_SIZE];
    snprintf(cache_file, sizeof(cache_file), "%s",
             read_line_from_file(pointer_file, 1));

    char *assigned_gov = NULL;
    assigned_gov = load_content_governor(cache_file);
    if (assigned_gov == NULL) assigned_gov = device.CPU.DEFAULT;

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

    toast_message(lang.MUXCOLLECT.ERROR.LOAD, 1000, 1000);
    LOG_ERROR(mux_module, "Cache Pointer Not Found: %s", cache_file)

    return 0;
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            items[current_item_index].display_name);
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
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
    nav_moved = 1;
}

void list_nav_next(int steps) {
    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound, 0, 0);
    }

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            items[current_item_index].display_name);
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
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
    nav_moved = 1;
}

void handle_keyboard_press(void) {
    play_sound("navigate", nav_sound, 0, 0);

    const char *is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    if (strcasecmp(is_key, OSK_DONE) == 0) {
        key_show = 0;
        snprintf(new_dir, sizeof(new_dir), "%s/%s",
                 sys_dir, lv_textarea_get_text(ui_txtEntry));
        create_directories(new_dir);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, lv_textarea_get_text(ui_txtEntry));
        load_mux("collection");
        mux_input_stop();
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

void add_collection_item() {
    char *base_file_name = read_line_from_file(ADD_MODE_WORK, 1);
    char collection_file[MAX_BUFFER_SIZE];
    snprintf(collection_file, sizeof(collection_file), "%s/%s",
             sys_dir, base_file_name);

    char *cache_file = read_line_from_file(ADD_MODE_WORK, 2);

    char collection_content[MAX_BUFFER_SIZE];
    snprintf(collection_content, sizeof(collection_content), "%s\n%s\n%s",
             cache_file, read_line_from_file(ADD_MODE_WORK, 3), strip_ext(read_line_from_file(cache_file, 7)));

    int file_counter = 1;
    while(file_exist(collection_file)) {
        snprintf(collection_file, sizeof(collection_file), "%s/%s-%d.cfg",
             sys_dir, strip_ext(base_file_name), file_counter);
    }
    write_text_to_file(collection_file, "w", CHAR, collection_content);

    if (file_exist(ADD_MODE_WORK)) remove(ADD_MODE_WORK);
    write_text_to_file(ADD_MODE_DONE, "w", CHAR, "DONE");

    if (file_exist(COLLECTION_DIR)) remove(COLLECTION_DIR);
}

void handle_a() {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
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

    play_sound("confirm", nav_sound, 0, 1);

    int load_message;

    if (!add_mode) write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

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
                            refresh_screen(device.SCREEN.WAIT / 2);
                        }

                        sleep(1);
                    }
                }

                if (config.VISUAL.BLACKFADE) {
                    fade_to_black(ui_screen);
                } else {
                    unload_image_animation();
                }
            } else {
                return;
            }
        }
    }

    if (load_message) {
        toast_message(lang.GENERIC.LOADING, 0, 0);
        lv_obj_move_foreground(ui_pnlMessage);

        // Refresh and add a small delay to actually display the message!
        refresh_screen(device.SCREEN.WAIT);
        usleep(256);
    }

    acq:
    if (file_exist(ADD_MODE_DONE)) {
        load_mux(read_text_from_file(ADD_MODE_FROM));
    } else {
        load_mux("collection");
    }
    mux_input_stop();
}

void handle_b() {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (key_show) {
        close_osk(key_entry, ui_group, ui_txtEntry, ui_pnlEntry);
        return;
    }

    play_sound("back", nav_sound, 0, 1);

    if (sys_dir != NULL) {
        if (at_base(sys_dir, "collection")) {
            if (file_exist(ADD_MODE_WORK)) remove(ADD_MODE_WORK);
            if (add_mode) write_text_to_file(ADD_MODE_DONE, "w", CHAR, "CANCEL");
            if (file_exist(COLLECTION_DIR)) remove(COLLECTION_DIR);
        } else {
            char *base_dir = strrchr(sys_dir, '/');
            if (base_dir != NULL) write_text_to_file(COLLECTION_DIR, "w", CHAR, strndup(sys_dir, base_dir - sys_dir));
        }
    }

    if (file_exist(COLLECTION_DIR)) {
        load_mux("collection");
    } else {
        if (file_exist(ADD_MODE_DONE)) {
            load_mux(read_text_from_file(ADD_MODE_FROM));
            remove(ADD_MODE_FROM);
        } else {
            load_mux("launcher");
            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "collection");
        }
    }

    mux_input_stop();
}

void handle_x() {
    if (key_show) {
        key_backspace(ui_txtEntry);
        return;
    }

    if (msgbox_active || !ui_count || add_mode) return;

    if (items[current_item_index].content_type == FOLDER) {
        if (get_directory_item_count(sys_dir, items[current_item_index].name) > 0) {
            toast_message(lang.MUXCOLLECT.ERROR.REMOVE_DIR, 1000, 1000);
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
    mux_input_stop();
}

void handle_y() {
    if (msgbox_active) return;

    if (key_show) {
        key_swap();
        return;
    }

    if (at_base(sys_dir, "collection")) {
        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

        key_show = 1;

        lv_obj_clear_flag(ui_pnlEntry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ui_pnlEntry);

        lv_textarea_set_text(ui_txtEntry, "");
    }
}

void handle_menu() {
    if (msgbox_active || progress_onscreen != -1 || !ui_count) {
        return;
    }

    play_sound("confirm", nav_sound, 0, 0);
    image_refresh("preview");

    lv_obj_add_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlHelp, LV_OBJ_FLAG_HIDDEN);

    static lv_anim_t desc_anim;
    static lv_style_t desc_style;
    lv_anim_init(&desc_anim);
    lv_anim_set_delay(&desc_anim, 2000);
    lv_style_init(&desc_style);
    lv_style_set_anim(&desc_style, &desc_anim);
    lv_obj_add_style(ui_lblHelpContent, &desc_style, LV_PART_MAIN);
    lv_obj_set_style_anim_speed(ui_lblHelpContent, 25, LV_PART_MAIN);

    show_rom_info(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpPreviewHeader,
                  ui_lblHelpContent,
                  items[current_item_index].display_name,
                  load_content_description());
}

void handle_random_select() {
    if (msgbox_active || !ui_count) return;

    uint32_t random_select = random() % MAX_BUFFER_SIZE;
    int selected_index = (int) (random_select & INT16_MAX);

    !(selected_index & 1) ? list_nav_next(selected_index) : list_nav_prev(selected_index);
}

void handle_up(void) {
    if (key_show) {
        key_up();
        return;
    }

    handle_list_nav_up();
}

void handle_up_hold(void) {
    if (key_show) {
        key_up();
        return;
    }

    handle_list_nav_up_hold();
}

void handle_down(void) {
    if (key_show) {
        key_down();
        return;
    }

    handle_list_nav_down();
}

void handle_down_hold(void) {
    if (key_show) {
        key_down();
        return;
    }

    handle_list_nav_down_hold();
}

void handle_left(void) {
    if (key_show) {
        key_left();
        return;
    }
}

void handle_right(void) {
    if (key_show) {
        key_right();
        return;
    }
}

void handle_left_hold(void) {
    if (key_show) {
        key_left();
        return;
    }
}

void handle_right_hold(void) {
    if (key_show) {
        key_right();
        return;
    }
}

void handle_l1(void) {
    if (key_show) return;
    handle_list_nav_page_up();
}

void handle_r1(void) {
    if (key_show) return;
    handle_list_nav_page_down();
}

void init_elements() {
    lv_label_set_long_mode(ui_lblHelpContent, LV_LABEL_LONG_SCROLL_CIRCULAR);

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
            lv_obj_add_flag(ui_pnlBox, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_pnlBox, LV_OBJ_FLAG_FLOATING);
            break;
    }

    ui_mux_panels[0] = ui_pnlFooter;
    ui_mux_panels[1] = ui_pnlHeader;
    ui_mux_panels[2] = ui_lblCounter;
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

    lv_label_set_text(ui_lblMessage, osd_message);

    lv_label_set_text(ui_lblNavA, lang.GENERIC.OPEN);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);
    lv_label_set_text(ui_lblNavX, lang.GENERIC.REMOVE);
    lv_label_set_text(ui_lblNavY, lang.GENERIC.NEW);
    lv_label_set_text(ui_lblNavMenu, lang.GENERIC.INFO);

    lv_obj_t *nav_hide[] = {
            ui_lblNavCGlyph,
            ui_lblNavC,
            ui_lblNavZGlyph,
            ui_lblNavZ
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    if (TEST_IMAGE) display_testing_message(ui_screen);

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image, theme.MISC.IMAGE_OVERLAY);
}

void init_osk() {
    key_entry = lv_btnmatrix_create(ui_pnlEntry);

    lv_obj_set_width(key_entry, device.MUX.WIDTH * 5 / 6);
    lv_obj_set_height(key_entry, device.MUX.HEIGHT * 5 / 9);

    lv_btnmatrix_set_one_checked(key_entry, 1);

    lv_btnmatrix_set_map(key_entry, key_lower_map);

    lv_btnmatrix_set_btn_ctrl(key_entry, 29, LV_BTNMATRIX_CTRL_HIDDEN);
    lv_btnmatrix_set_btn_ctrl(key_entry, 37, LV_BTNMATRIX_CTRL_HIDDEN);

    lv_btnmatrix_set_btn_width(key_entry, 39, 3);

    lv_btnmatrix_set_selected_btn(key_entry, key_curr);

    lv_btnmatrix_set_btn_ctrl(key_entry, lv_btnmatrix_get_selected_btn(key_entry), LV_BTNMATRIX_CTRL_CHECKED);

    lv_obj_align(key_entry, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(key_entry, osk_handler, LV_EVENT_ALL, ui_txtEntry);

    lv_obj_set_style_border_width(key_entry, 3, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(key_entry, 1, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(key_entry, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(key_entry, lv_color_hex(theme.OSK.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(key_entry, lv_color_hex(theme.OSK.ITEM.BACKGROUND), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(key_entry, lv_color_hex(theme.OSK.ITEM.BACKGROUND_FOCUS),
                              LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_bg_opa(key_entry, theme.OSK.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(key_entry, theme.OSK.ITEM.BACKGROUND_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(key_entry, theme.OSK.ITEM.BACKGROUND_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_text_color(key_entry, lv_color_hex(theme.OSK.TEXT), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(key_entry, lv_color_hex(theme.OSK.TEXT_FOCUS), LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_text_opa(key_entry, theme.OSK.TEXT_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(key_entry, theme.OSK.TEXT_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_border_color(key_entry, lv_color_hex(theme.OSK.BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(key_entry, lv_color_hex(theme.OSK.ITEM.BORDER), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(key_entry, lv_color_hex(theme.OSK.ITEM.BORDER_FOCUS),
                                  LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_border_opa(key_entry, theme.OSK.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(key_entry, theme.OSK.ITEM.BORDER_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(key_entry, theme.OSK.ITEM.BORDER_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_radius(key_entry, theme.OSK.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(key_entry, theme.OSK.ITEM.RADIUS, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(key_entry, theme.OSK.ITEM.RADIUS, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_pad_top(key_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(key_entry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(key_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(key_entry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_gap(key_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_height(ui_txtEntry, 48);
    lv_obj_set_style_text_color(ui_txtEntry, lv_color_hex(theme.OSK.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_txtEntry, theme.OSK.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_txtEntry, lv_color_hex(theme.OSK.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_txtEntry, theme.OSK.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_txtEntry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_txtEntry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_txtEntry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void init_fonts() {
    load_font_text(mux_module, ui_screen);
    load_font_section(mux_module, FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(mux_module, FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(mux_module, FONT_FOOTER_FOLDER, ui_pnlFooter);
}

void glyph_task() {
    // TODO: Bluetooth connectivity!
    //update_bluetooth_status(ui_staBluetooth, &theme);

    update_network_status(ui_staNetwork, &theme);
    update_battery_capacity(ui_staCapacity, &theme);

    if (progress_onscreen > 0) {
        progress_onscreen -= 1;
    } else {
        if (!lv_obj_has_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN);
        }
        if (!lv_obj_has_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN);
        }
        if (!msgbox_active) {
            progress_onscreen = -1;
        }
    }
}

void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (!nav_moved & !fade_timeout) {
        if (counter_fade > 0) {
            lv_obj_set_style_opa(ui_lblCounter, counter_fade - theme.COUNTER.TEXT_FADE_TIME,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
            counter_fade -= theme.COUNTER.TEXT_FADE_TIME;
        }
        if (counter_fade < 0) {
            lv_obj_add_flag(ui_lblCounter, LV_OBJ_FLAG_HIDDEN);
            counter_fade = 0;
        }
    } else {
        fade_timeout--;
    }

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

int main(int argc, char *argv[]) {
    mux_module = basename(argv[0]);
    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    struct screen_dimension dims = get_device_dimensions();

    char *cmd_help = "\nmuOS Extras - Content Collection\nUsage: %s <-adi>\n\nOptions:\n"
                     "\t-a Add mode\n"
                     "\t-d Content directory\n"
                     "\t-i Index of content to skip to\n\n";

    int opt;
    while ((opt = getopt(argc, argv, "a:d:i:")) != -1) {
        switch (opt) {
            case 'a':
                add_mode = safe_atoi(optarg);
                break;
            case 'd':
                sys_dir = !strlen(optarg) ? INFO_COL_PATH : optarg;
                break;
            case 'i':
                sys_index = safe_atoi(optarg);
                break;
            default:
                fprintf(stderr, cmd_help, argv[0]);
                return 1;
        }
    }

    if (sys_index == -1) {
        fprintf(stderr, cmd_help, argv[0]);
        return 1;
    }

    lv_init();
    fbdev_init(device.SCREEN.DEVICE);

    static lv_disp_draw_buf_t disp_buf;
    uint32_t disp_buf_size = dims.WIDTH * dims.HEIGHT;

    lv_color_t *buf1 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));
    lv_color_t *buf2 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, disp_buf_size);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res = dims.WIDTH;
    disp_drv.ver_res = dims.HEIGHT;
    disp_drv.sw_rotate = device.SCREEN.ROTATE;
    disp_drv.rotated = device.SCREEN.ROTATE;
    disp_drv.full_refresh = 0;
    disp_drv.direct_mode = 0;
    disp_drv.antialiasing = 1;
    disp_drv.color_chroma_key = lv_color_hex(0xFF00FF);
    lv_disp_drv_register(&disp_drv);
    lv_disp_flush_ready(&disp_drv);

    load_theme(&theme, &config, &device, mux_module);

    ui_common_screen_init(&theme, &device, &lang, "");
    ui_init(ui_screen, ui_pnlContent, &theme);

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

    struct dt_task_param dt_par;
    struct bat_task_param bat_par;

    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;

    datetime_timer = lv_timer_create(datetime_task, UINT16_MAX / 2, &dt_par);
    lv_timer_ready(datetime_timer);

    capacity_timer = lv_timer_create(capacity_task, UINT16_MAX / 2, &bat_par);
    lv_timer_ready(capacity_timer);

    glyph_timer = lv_timer_create(glyph_task, UINT16_MAX / 64, NULL);
    lv_timer_ready(glyph_timer);

    ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
    lv_timer_ready(ui_refresh_timer);

    init_elements();

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND,
                   theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND, GENERAL);

    nav_sound = init_nav_sound(mux_module);
    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    if (file_exist(MUOS_PDI_LOAD)) prev_dir = read_text_from_file(MUOS_PDI_LOAD);

    create_collection_items();
    ui_count = dir_count > 0 || file_count > 0 ? (int) lv_group_get_obj_count(ui_group) : 0;

    if (sys_dir != NULL) {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, get_last_dir(sys_dir));
    }

    if (strcasecmp(read_text_from_file(MUOS_PDI_LOAD), "ROMS") == 0) {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, get_last_subdir(sys_dir, '/', 4));
    }

    js_fd = open(device.INPUT.EV1, O_RDONLY);
    if (js_fd < 0) {
        perror(lang.SYSTEM.NO_JOY);
        return 1;
    }

    js_fd_sys = open(device.INPUT.EV0, O_RDONLY);
    if (js_fd_sys < 0) {
        perror(lang.SYSTEM.NO_JOY);
        return 1;
    }

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);

    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = evdev_read;
    indev_drv.user_data = (void *) (intptr_t) js_fd;

    lv_indev_drv_register(&indev_drv);

    if (ui_count > 0) {
        if (sys_index > -1 && sys_index <= ui_count &&
            current_item_index < ui_count) {
            list_nav_next(sys_index);
        } else {
            image_refresh("box");
        }
        nav_moved = 1;
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXCOLLECT.NONE);
        lv_obj_clear_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
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
                int hidden[] = {0, 1, 2, 3, 6, 7};
                for (int i = 0; i < 6; ++i) {
                    nav_e[hidden[i]].visible = 0;
                }
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
    adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

    update_file_counter();
    init_osk();

    refresh_screen(device.SCREEN.WAIT);
    load_kiosk(&kiosk);

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = IDLE_MS,
            .swap_btn = config.SETTINGS.ADVANCED.SWAP,
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1 ||
                          (grid_mode_enabled && theme.GRID.NAVIGATION_TYPE >= 1 && theme.GRID.NAVIGATION_TYPE <= 5)),
            .stick_nav = true,
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
            },
            .combo = {
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_bright,
                            .hold_handler = ui_common_handle_bright,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_bright,
                            .hold_handler = ui_common_handle_bright,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_vol,
                            .hold_handler = ui_common_handle_vol,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_vol,
                            .hold_handler = ui_common_handle_vol,
                    },
            },
            .idle_handler = ui_common_handle_idle,
    };
    mux_input_task(&input_opts);

    free_items(items, item_count);
    close(js_fd);
    close(js_fd_sys);

    return 0;
}
