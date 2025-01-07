#include "../lvgl/lvgl.h"
#include "../lvgl/src/drivers/fbdev.h"
#include "../lvgl/src/drivers/evdev.h"
#include "ui/ui_muxsearch.h"
#include <unistd.h>
#include <fcntl.h>
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
#include "../common/input.h"
#include "../common/log.h"
#include "../common/collection.h"
#include "../common/json/json.h"
#include "../common/input/list_nav.h"

static int js_fd;
static int js_fd_sys;

int turbo_mode = 0;
int msgbox_active = 0;
int SD2_found = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;
char *mux_module;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int nav_moved = 1;
int current_item_index = 0;
int first_open = 1;
int ui_count = 0;
int starter_image = 0;
int progress_onscreen = -1;
int got_results = 0;

int key_show = 0;
int key_curr = 0;

static char SD1[MAX_BUFFER_SIZE];
static char SD2[MAX_BUFFER_SIZE];
static char E_USB[MAX_BUFFER_SIZE];

static char search_result[MAX_BUFFER_SIZE];
static char rom_dir[MAX_BUFFER_SIZE];
static char lookup_value[MAX_BUFFER_SIZE];
static char box_image_previous_path[MAX_BUFFER_SIZE];

struct json search_folders;

size_t all_item_count = 0;
content_item *all_items = NULL;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;
lv_obj_t *kiosk_image = NULL;

lv_obj_t *key_entry;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

lv_obj_t *ui_viewport_objects[7];
lv_obj_t *ui_mux_panels[7];

void list_nav_prev(int steps);

void list_nav_next(int steps);

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblLookup,       lang.MUXSEARCH.LOOKUP},
            {ui_lblSearchLocal,  lang.MUXSEARCH.LOCAL},
            {ui_lblSearchGlobal, lang.MUXSEARCH.GLOBAL},
    };

    char *message = lang.GENERIC.NO_HELP;
    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);

    for (int i = 0; i < num_messages; i++) {
        if (element_focused == help_messages[i].element) {
            message = help_messages[i].message;
            break;
        }
    }

    if (strlen(message) <= 1) message = lang.GENERIC.NO_HELP;

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
}

void init_navigation_groups() {
    lv_obj_t *ui_panels[] = {
            ui_pnlLookup,
            ui_pnlSearchLocal,
            ui_pnlSearchGlobal,
    };

    lv_obj_t *ui_labels[] = {
            ui_lblLookup,
            ui_lblSearchLocal,
            ui_lblSearchGlobal,
    };

    lv_obj_t *ui_values[] = {
            ui_lblLookupValue,
            ui_lblSearchLocalValue,
            ui_lblSearchGlobalValue,
    };

    lv_obj_t *ui_icons[] = {
            ui_icoLookup,
            ui_icoSearchLocal,
            ui_icoSearchGlobal,
    };

    apply_theme_list_panel(&theme, &device, ui_pnlLookup);
    apply_theme_list_panel(&theme, &device, ui_pnlSearchLocal);
    apply_theme_list_panel(&theme, &device, ui_pnlSearchGlobal);

    apply_theme_list_item(&theme, ui_lblLookup, lang.MUXSEARCH.LOOKUP, false, true);
    apply_theme_list_item(&theme, ui_lblSearchLocal, lang.MUXSEARCH.LOCAL, false, true);
    apply_theme_list_item(&theme, ui_lblSearchGlobal, lang.MUXSEARCH.GLOBAL, false, true);

    apply_theme_list_glyph(&theme, ui_icoLookup, mux_module, "lookup");
    apply_theme_list_glyph(&theme, ui_icoSearchLocal, mux_module, "local");
    apply_theme_list_glyph(&theme, ui_icoSearchGlobal, mux_module, "global");

    apply_theme_list_value(&theme, ui_lblLookupValue, "");
    apply_theme_list_value(&theme, ui_lblSearchLocalValue, "");
    apply_theme_list_value(&theme, ui_lblSearchGlobalValue, "");

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    ui_count = sizeof(ui_panels) / sizeof(ui_panels[0]);
    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_labels[i]);
        lv_group_add_obj(ui_group_value, ui_values[i]);
        lv_group_add_obj(ui_group_glyph, ui_icons[i]);
        lv_group_add_obj(ui_group_panel, ui_panels[i]);
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
    if (strcasecmp(image_type, "box") == 0 && config.VISUAL.BOX_ART == 8) {
        printf("BOX ART IS SET TO DISABLED\n");
        return;
    }

    char mux_dimension[15];
    get_mux_dimension(mux_dimension, sizeof(mux_dimension));

    char image[MAX_BUFFER_SIZE];
    char image_path[MAX_BUFFER_SIZE];
    char core_artwork[MAX_BUFFER_SIZE];

    char *file_name = get_last_subdir(strip_ext(all_items[current_item_index].name), '/', 4);
    char *last_dir = get_last_dir(strip_ext(all_items[current_item_index].name));

    char core_file[MAX_BUFFER_SIZE];
    snprintf(core_file, sizeof(core_file), "%s/%s.cfg",
             INFO_COR_PATH, str_replace(file_name, last_dir, ""));

    if (!file_exist(core_file)) {
        snprintf(core_file, sizeof(core_file), "%s/%score.cfg",
                 INFO_COR_PATH, str_replace(file_name, last_dir, ""));
        snprintf(core_artwork, sizeof(core_artwork), "%s",
                 read_line_from_file(core_file, 2));
    } else {
        snprintf(core_artwork, sizeof(core_artwork), "%s",
                 read_line_from_file(core_file, 3));
    }

    LOG_INFO(mux_module, "Reading Configuration: %s", core_file)

    if (strlen(core_artwork) <= 1) {
        snprintf(image, sizeof(image), "%s/%simage/none_%s.png",
                 STORAGE_THEME, mux_dimension, image_type);
        if (!file_exist(image)) {
            snprintf(image, sizeof(image), "%s/image/none_%s.png",
                     STORAGE_THEME, image_type);
        }
        snprintf(image_path, sizeof(image_path), "M:%s", image);
    } else {
        snprintf(image, sizeof(image), "%s/%s/%s/%s.png",
                 INFO_CAT_PATH, core_artwork, image_type, last_dir);
        snprintf(image_path, sizeof(image_path), "M:%s/%s/%s/%s.png",
                 INFO_CAT_PATH, core_artwork, image_type, last_dir);
    }

    LOG_INFO(mux_module, "Loading '%s' Artwork: %s", image_type, image)

    if (strcasecmp(box_image_previous_path, image) != 0) {
        char artwork_config_path[MAX_BUFFER_SIZE];
        snprintf(artwork_config_path, sizeof(artwork_config_path), "%s/%s.ini",
                 INFO_CAT_PATH, core_artwork);
        if (!file_exist(artwork_config_path)) {
            snprintf(artwork_config_path, sizeof(artwork_config_path), "%s/default.ini",
                     INFO_CAT_PATH);
        }

        if (file_exist(artwork_config_path)) {
            viewport_refresh(artwork_config_path, core_artwork, last_dir);
            snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", image);
        } else {
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

void gen_label(char *item_glyph, char *item_text, char *item_data, char *item_value) {
    lv_obj_t *ui_pnlResult = lv_obj_create(ui_pnlContent);
    apply_theme_list_panel(&theme, &device, ui_pnlResult);

    lv_obj_t *ui_lblResultItem = lv_label_create(ui_pnlResult);
    apply_theme_list_item(&theme, ui_lblResultItem, item_text, true, false);

    lv_obj_t *ui_lblResultItemValue = lv_label_create(ui_pnlResult);
    lv_label_set_text(ui_lblResultItemValue, item_value);
    lv_obj_set_style_text_opa(ui_lblResultItemValue, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_width(ui_lblResultItemValue, 0);

    lv_obj_t *ui_lblResultItemGlyph = lv_img_create(ui_pnlResult);
    apply_theme_list_glyph(&theme, ui_lblResultItemGlyph, mux_module, item_glyph);

    apply_size_to_content(&theme, ui_pnlContent, ui_lblResultItem, ui_lblResultItemGlyph, item_text);
    apply_text_long_dot(&theme, ui_pnlContent, ui_lblResultItem, item_text);

    lv_obj_set_user_data(ui_lblResultItem, item_data);

    if (strcasecmp(item_data, "content") == 0) {
        if (file_exist(FRIENDLY_RESULT)) {
            LOG_INFO(mux_module, "Reading Friendly Name Set: %s", FRIENDLY_RESULT)

            int fn_valid = 0;
            struct json fn_json;

            if (json_valid(read_text_from_file(FRIENDLY_RESULT))) {
                fn_valid = 1;
                fn_json = json_parse(read_text_from_file(FRIENDLY_RESULT));
            }

            if (fn_valid) {
                char fn_name[MAX_BUFFER_SIZE];
                struct json good_name_json = json_object_get(fn_json, strip_ext(item_text));

                if (json_exists(good_name_json)) {
                    json_string_copy(good_name_json, fn_name, sizeof(fn_name));
                    lv_label_set_text(ui_lblResultItem, fn_name);
                }
            }
        }

        lv_group_add_obj(ui_group, ui_lblResultItem);
        lv_group_add_obj(ui_group_value, ui_lblResultItemValue);
        lv_group_add_obj(ui_group_glyph, ui_lblResultItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlResult);

        ui_count++;
    }

    if (strcasecmp(item_data, "folder") == 0) {
        lv_obj_set_style_border_width(ui_pnlResult, 1,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(ui_pnlResult, lv_color_hex(theme.LIST_DEFAULT.TEXT),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(ui_pnlResult, theme.LIST_DEFAULT.TEXT_ALPHA,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(ui_pnlResult, LV_BORDER_SIDE_TOP,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void process_results(const char *json_results) {
    if (!json_valid(json_results)) {
        LOG_ERROR(mux_module, "Invalid JSON Format")
        return;
    }

    struct json root = json_parse(json_results);

    struct json lookup = json_object_get(root, "lookup");
    if (json_exists(lookup) && json_type(lookup) == JSON_STRING) {
        json_string_copy(lookup, lookup_value, sizeof(lookup_value));
    }

    struct json directories = json_object_get(root, "directories");
    if (json_exists(directories) && json_type(directories) == JSON_ARRAY) {
        if (json_array_count(directories) > 0) {
            list_nav_next(json_array_count(directories) == 1 ? 1 : 2);
        }
    }

    search_folders = json_object_get(root, "folders");
    if (json_exists(search_folders) && json_type(search_folders) == JSON_OBJECT) {
        size_t t_all_item_count = 0;
        content_item *t_all_items = NULL;

        struct json folder = json_first(search_folders);
        while (json_exists(folder)) {

            char folder_name[MAX_BUFFER_SIZE];

            if (json_type(folder) == JSON_STRING) {
                json_string_copy(folder, folder_name, sizeof(folder_name));

                char storage_name_short[MAX_BUFFER_SIZE];
                char *storage_name = NULL;

                if (str_extract(folder_name, "", "/ROMS/", &storage_name)) {
                    snprintf(storage_name_short, sizeof(storage_name_short), "%s",
                             storage_name);
                    free(storage_name);

                    if (strcasecmp(storage_name_short, device.STORAGE.ROM.MOUNT) == 0) {
                        snprintf(storage_name_short, sizeof(storage_name_short), "SD1");
                    } else if (strcasecmp(storage_name_short, device.STORAGE.SDCARD.MOUNT) == 0) {
                        snprintf(storage_name_short, sizeof(storage_name_short), "SD2");
                    } else if (strcasecmp(storage_name_short, device.STORAGE.USB.MOUNT) == 0) {
                        snprintf(storage_name_short, sizeof(storage_name_short), "USB");
                    }
                }

                char folder_name_short[MAX_BUFFER_SIZE];
                char *modified_name = NULL;

                if (str_replace_segment(folder_name, "/mnt/", "/ROMS/", "", &modified_name)) {
                    snprintf(folder_name_short, sizeof(folder_name_short), " %s/%s",
                             storage_name_short, str_replace(modified_name, "/mnt//ROMS/", ""));
                    free(modified_name);
                } else {
                    snprintf(folder_name_short, sizeof(folder_name_short), "%s",
                             folder_name);
                }

                if (strcasecmp(folder_name_short, ".") != 0) {
                    gen_label("folder", folder_name_short, "folder", "");
                }
            }

            size_t folder_item_count = 0;
            content_item *folder_items = NULL;

            struct json content = json_object_get(folder, "content");
            if (json_exists(content) && json_type(content) == JSON_ARRAY) {
                for (size_t i = 0; i < json_array_count(content); i++) {
                    struct json item = json_array_get(content, i);

                    if (json_type(item) == JSON_STRING) {
                        char content_name[MAX_BUFFER_SIZE];
                        json_string_copy(item, content_name, sizeof(content_name));

                        char content_path[MAX_BUFFER_SIZE];
                        json_string_copy(item, content_path, sizeof(content_path));

                        char content_full_path[MAX_BUFFER_SIZE];
                        if (folder_name[0] != '/') {
                            snprintf(content_full_path, sizeof(content_full_path), "%s/%s/%s",
                                     rom_dir, folder_name, content_path);
                        } else {
                            snprintf(content_full_path, sizeof(content_full_path), "%s/%s",
                                     folder_name, content_path);
                        }

                        adjust_visual_label(content_name, config.VISUAL.NAME, config.VISUAL.DASH);
                        add_item(&folder_items, &folder_item_count, content_name, content_name, content_full_path, ROM);
                    }
                }

                sort_items(folder_items, folder_item_count);

                for (size_t i = 0; i < folder_item_count; i++) {
                    if (folder_items[i].content_type == ROM) {
                        add_item(&t_all_items, &t_all_item_count, folder_items[i].extra_data,
                                 folder_items[i].extra_data, folder_items[i].extra_data, ROM);
                        gen_label("content", strip_ext(folder_items[i].display_name),
                                  "content", folder_items[i].extra_data);
                    }
                }

                free_items(folder_items, folder_item_count);

                folder_item_count = 0;
                folder_items = NULL;
            }

            folder = json_next(folder);
        }

        // We are done with the friendly results JSON
        if (file_exist(FRIENDLY_RESULT)) {
            remove(FRIENDLY_RESULT);
        }

        // Add the three top labels - lookup, local, and global
        add_item(&all_items, &all_item_count, "", "", "", FOLDER);
        add_item(&all_items, &all_item_count, "", "", "", FOLDER);
        add_item(&all_items, &all_item_count, "", "", "", FOLDER);

        for (size_t i = 0; i < t_all_item_count; i++) {
            if (t_all_items[i].content_type == ROM) {
                add_item(&all_items, &all_item_count, t_all_items[i].name,
                         t_all_items[i].display_name, t_all_items[i].extra_data, ROM);
                char display_name[MAX_BUFFER_SIZE];
                snprintf(display_name, sizeof(display_name), "%s",
                         strip_ext(get_last_dir(t_all_items[i].display_name)));
                adjust_visual_label(display_name, config.VISUAL.NAME, config.VISUAL.DASH);
                all_items[i].display_name = strdup(display_name);
            }
        }

        free_items(t_all_items, t_all_item_count);

        t_all_item_count = 0;
        t_all_items = NULL;
    }
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        if (all_item_count > 0 && all_items[current_item_index].content_type == ROM) {
            apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                                all_items[current_item_index].display_name);
        }
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_value, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    scroll_object_to_middle(ui_pnlContent, lv_group_get_focused(ui_group_panel));
    if (all_item_count > 0 && all_items[current_item_index].content_type == ROM) {
        image_refresh("box");
        set_label_long_mode(&theme, lv_group_get_focused(ui_group), all_items[current_item_index].display_name);
    } else {
        lv_img_set_src(ui_imgBox, &ui_image_Nothing);
        snprintf(box_image_previous_path, sizeof(box_image_previous_path), "");
    }
    nav_moved = 1;
}

void list_nav_next(int steps) {
    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound, 0, 0);
    }
    for (int step = 0; step < steps; ++step) {
        if (all_item_count > 0 && all_items[current_item_index].content_type == ROM) {
            apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                                all_items[current_item_index].display_name);
        }
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_value, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    scroll_object_to_middle(ui_pnlContent, lv_group_get_focused(ui_group_panel));
    if (all_item_count > 0 && all_items[current_item_index].content_type == ROM) {
        image_refresh("box");
        set_label_long_mode(&theme, lv_group_get_focused(ui_group), all_items[current_item_index].display_name);
    } else {
        lv_img_set_src(ui_imgBox, &ui_image_Nothing);
        snprintf(box_image_previous_path, sizeof(box_image_previous_path), "");
    }
    nav_moved = 1;
}

void handle_keyboard_press(void) {
    play_sound("navigate", nav_sound, 0, 0);

    const char *is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    if (strcasecmp(is_key, OSK_DONE) == 0) {
        key_show = 0;
        struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

        if (element_focused == ui_lblLookup) {
            lv_label_set_text(ui_lblLookupValue,
                              lv_textarea_get_text(ui_txtEntry));
        }

        reset_osk(key_entry);

        lv_textarea_set_text(ui_txtEntry, "");
        lv_group_set_focus_cb(ui_group, NULL);
        lv_obj_add_flag(ui_pnlEntry, LV_OBJ_FLAG_HIDDEN);
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

void handle_confirm(void) {
    play_sound("confirm", nav_sound, 0, 1);

    if (file_exist(MUOS_SAA_LOAD)) remove(MUOS_SAA_LOAD);
    if (file_exist(MUOS_SAG_LOAD)) remove(MUOS_SAG_LOAD);

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

    if (element_focused == ui_lblLookup) {
        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

        key_show = 1;

        lv_obj_clear_flag(ui_pnlEntry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ui_pnlEntry);

        lv_textarea_set_text(ui_txtEntry, lv_label_get_text(lv_group_get_focused(ui_group_value)));
    } else if (element_focused == ui_lblSearchLocal || element_focused == ui_lblSearchGlobal) {
        if (strlen(lv_label_get_text(ui_lblLookupValue)) <= 2) {
            toast_message(lang.MUXSEARCH.ERROR, 1000, 1000);
            return;
        }

        lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(ui_lblMessage, lang.MUXSEARCH.SEARCH);
        refresh_screen(device.SCREEN.WAIT);

        if (element_focused == ui_lblSearchLocal) {
            run_exec((const char *[]) {
                    (INTERNAL_PATH "script/mux/find.sh"),
                    str_trim(lv_label_get_text(ui_lblLookupValue)), rom_dir,
                    NULL
            });
        } else {
            run_exec((const char *[]) {
                    (INTERNAL_PATH "script/mux/find.sh"),
                    str_trim(lv_label_get_text(ui_lblLookupValue)), SD1, SD2, E_USB,
                    NULL
            });
        }

        if (file_exist(MUOS_RES_LOAD)) remove(MUOS_RES_LOAD);

        load_mux("search");
        mux_input_stop();
    } else {
        if (strcasecmp(lv_obj_get_user_data(element_focused), "content") == 0) {
            write_text_to_file(MUOS_RES_LOAD, "w", CHAR,
                               str_replace(lv_label_get_text(lv_group_get_focused(ui_group_value)), "/./", "/"));

            load_mux("explore");
            mux_input_stop();
        }
    }
}

void handle_random_select() {
    if (msgbox_active || !ui_count) return;

    uint32_t random_select = random() % ui_count;
    int selected_index = (int) (random_select & INT16_MAX);

    !(selected_index & 1) ? list_nav_next(selected_index) : list_nav_prev(selected_index);
}

void handle_back(void) {
    play_sound("back", nav_sound, 0, 1);

    if (file_exist(MUOS_RES_LOAD)) remove(MUOS_RES_LOAD);
    if (strlen(rom_dir) == 0 || strcasecmp(rom_dir, CONTENT_PATH) == 0 || kiosk.CONTENT.OPTION) load_mux("explore");

    mux_input_stop();
}

void handle_a(void) {
    if (msgbox_active) return;

    if (key_show) {
        handle_keyboard_press();
        return;
    }

    handle_confirm();
}

void handle_b(void) {
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

    handle_back();
}

void handle_x(void) {
    if (msgbox_active) return;

    if (key_show) {
        key_backspace(ui_txtEntry);
        return;
    }

    if (file_exist(MUOS_RES_LOAD)) remove(MUOS_RES_LOAD);
    if (file_exist(search_result)) remove(search_result);

    load_mux("search");
    mux_input_stop();
}

void handle_y(void) {
    if (msgbox_active) return;

    if (key_show) {
        key_swap();
        return;
    }

    // TODO: A way to directly add the item to a collection
}

void handle_help(void) {
    if (msgbox_active || key_show) return;

    if (progress_onscreen == -1 && all_items[current_item_index].content_type != ROM) {
        play_sound("confirm", nav_sound, 0, 0);
        show_help(lv_group_get_focused(ui_group));
    }
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
    ui_mux_panels[2] = ui_pnlHelp;
    ui_mux_panels[3] = ui_pnlEntry;
    ui_mux_panels[4] = ui_pnlProgressBrightness;
    ui_mux_panels[5] = ui_pnlProgressVolume;
    ui_mux_panels[6] = ui_pnlMessage;

    adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

    if (bar_footer) lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblNavA, lang.GENERIC.SELECT);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);
    lv_label_set_text(ui_lblNavX, lang.GENERIC.CLEAR);

    lv_obj_t *nav_hide[] = {
            ui_lblNavCGlyph,
            ui_lblNavC,
            ui_lblNavYGlyph,
            ui_lblNavY,
            ui_lblNavZGlyph,
            ui_lblNavZ,
            ui_lblNavMenuGlyph,
            ui_lblNavMenu
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblLookup, "lookup");
    lv_obj_set_user_data(ui_lblSearchLocal, "local");
    lv_obj_set_user_data(ui_lblSearchGlobal, "global");

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

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

    if (nav_moved) {
        starter_image = adjust_wallpaper_element(ui_group, starter_image, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

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

    char *cmd_help = "\nmuOS Extras - Content Search\nUsage: %s <-d>\n\nOptions:\n"
                     "\t-d Name of directory to search\n\n";

    int opt;
    while ((opt = getopt(argc, argv, "d:")) != -1) {
        if (opt == 'd') {
            snprintf(rom_dir, sizeof(rom_dir), "%s", optarg);
        } else {
            fprintf(stderr, cmd_help, argv[0]);
            return 1;
        }
    }

    snprintf(search_result, sizeof(search_result), "%s/%s/search.json",
             device.STORAGE.ROM.MOUNT, MUOS_INFO_PATH);

    char *json_content;

    if (file_exist(search_result)) {
        json_content = read_text_from_file(search_result);
        if (json_content) {
            got_results = 1;
        } else {
            LOG_ERROR(mux_module, "Error reading search results")
        }
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

    load_theme(&theme, &config, &device, basename(argv[0]));

    ui_common_screen_init(&theme, &device, &lang, lang.MUXSEARCH.TITLE);
    ui_init(ui_screen, ui_pnlContent, &theme);

    ui_viewport_objects[0] = lv_obj_create(ui_pnlBox);
    ui_viewport_objects[1] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[2] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[3] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[4] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[5] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[6] = lv_img_create(ui_viewport_objects[0]);

    snprintf(SD1, sizeof(SD1), "%s/ROMS/", device.STORAGE.ROM.MOUNT);
    snprintf(SD2, sizeof(SD2), "%s/ROMS/", device.STORAGE.SDCARD.MOUNT);
    snprintf(E_USB, sizeof(E_USB), "%s/ROMS/", device.STORAGE.USB.MOUNT);

    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND,
                   theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND, GENERAL);

    load_font_text(basename(argv[0]), ui_screen);
    load_font_section(basename(argv[0]), FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(mux_module, FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(mux_module, FONT_FOOTER_FOLDER, ui_pnlFooter);

    nav_sound = init_nav_sound(mux_module);
    init_navigation_groups();

    if (got_results) {
        process_results(json_content);
        lv_label_set_text(ui_lblLookupValue, lookup_value);
        free(json_content);
    }

    struct dt_task_param dt_par;
    struct bat_task_param bat_par;
    struct osd_task_param osd_par;

    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;
    osd_par.lblMessage = ui_lblMessage;
    osd_par.pnlMessage = ui_pnlMessage;
    osd_par.count = 0;

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

    lv_timer_t *datetime_timer = lv_timer_create(datetime_task, UINT16_MAX / 2, &dt_par);
    lv_timer_ready(datetime_timer);

    lv_timer_t *capacity_timer = lv_timer_create(capacity_task, UINT16_MAX / 2, &bat_par);
    lv_timer_ready(capacity_timer);

    lv_timer_t *osd_timer = lv_timer_create(osd_task, UINT16_MAX / 32, &osd_par);
    lv_timer_ready(osd_timer);

    lv_timer_t *glyph_timer = lv_timer_create(glyph_task, UINT16_MAX / 64, NULL);
    lv_timer_ready(glyph_timer);

    lv_timer_t *ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
    lv_timer_ready(ui_refresh_timer);

    init_osk();

    refresh_screen(device.SCREEN.WAIT);
    load_kiosk(&kiosk);

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = IDLE_MS,
            .swap_btn = config.SETTINGS.ADVANCED.SWAP,
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .stick_nav = true,
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
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

    close(js_fd);
    close(js_fd_sys);

    return 0;
}
