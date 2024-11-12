#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/input.h"
#include "../common/log.h"
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

struct mux_config config;
struct mux_device device;
struct theme_config theme;

int nav_moved = 1;
int current_item_index = 0;
int first_open = 1;
int ui_count = 0;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;

int progress_onscreen = -1;
int got_results = 0;
int key_show = 0;
int key_curr = 0;
int key_map = 0;

static char rom_dir[MAX_BUFFER_SIZE];
static char lookup_value[MAX_BUFFER_SIZE];

struct json search_folders;

lv_obj_t *key_entry;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

lv_obj_t *ui_mux_panels[7];

static const char *key_lower_map[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
                                      "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
                                      "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
                                      "§", "z", "x", "c", "v", "b", "n", "m", "§", "\n",
                                      "ABC", " ", "OK", NULL
};

static const char *key_upper_map[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
                                      "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
                                      "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
                                      "§", "Z", "X", "C", "V", "B", "N", "M", "§", "\n",
                                      "!@#", " ", "OK", NULL
};

static const char *key_special_map[] = {"!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "\n",
                                        "`", "~", "_", "-", "+", "=", "{", "}", "[", "]", "\n",
                                        "|", "\\", ":", ";", "\"", "'", "!", "@", "#", "\n",
                                        "§", "<", ">", ",", ".", "?", "/", "$", "§", "\n",
                                        "abc", " ", "OK", NULL
};

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblLookup, TS("Enter in a search time to find stuff and things")},
            {ui_lblSearch, TS("Do the search thing")},
    };

    char *message = TG("No Help Information Found");
    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);

    for (int i = 0; i < num_messages; i++) {
        if (element_focused == help_messages[i].element) {
            message = help_messages[i].message;
            break;
        }
    }

    if (strlen(message) <= 1) message = TG("No Help Information Found");

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
}

void init_navigation_groups() {
    lv_obj_t *ui_panels[] = {
            ui_pnlLookup,
            ui_pnlSearch,
    };

    lv_obj_t *ui_labels[] = {
            ui_lblLookup,
            ui_lblSearch,
    };

    lv_obj_t *ui_values[] = {
            ui_lblLookupValue,
            ui_lblSearchValue,
    };

    lv_obj_t *ui_icons[] = {
            ui_icoLookup,
            ui_icoSearch,
    };

    apply_theme_list_panel(&theme, &device, ui_pnlLookup);
    apply_theme_list_panel(&theme, &device, ui_pnlSearch);

    apply_theme_list_item(&theme, ui_lblLookup, TS("Lookup"), false, true);
    apply_theme_list_item(&theme, ui_lblSearch, TS("Search"), false, true);

    apply_theme_list_glyph(&theme, ui_icoLookup, mux_module, "lookup");
    apply_theme_list_glyph(&theme, ui_icoSearch, mux_module, "search");

    apply_theme_list_value(&theme, ui_lblLookupValue, "");
    apply_theme_list_value(&theme, ui_lblSearchValue, "");

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

void gen_label(char *item_glyph, char *item_text) {
    lv_obj_t *ui_pnlResult = lv_obj_create(ui_pnlContent);
    apply_theme_list_panel(&theme, &device, ui_pnlResult);

    lv_obj_t *ui_lblResultItem = lv_label_create(ui_pnlResult);
    apply_theme_list_item(&theme, ui_lblResultItem, item_text, true, false);

    lv_obj_t *ui_lblResultItemGlyph = lv_img_create(ui_pnlResult);
    apply_theme_list_glyph(&theme, ui_lblResultItemGlyph, mux_module, item_glyph);

    lv_group_add_obj(ui_group, ui_lblResultItem);
    lv_group_add_obj(ui_group_glyph, ui_lblResultItemGlyph);
    lv_group_add_obj(ui_group_panel, ui_pnlResult);

    apply_size_to_content(&theme, ui_pnlContent, ui_lblResultItem, ui_lblResultItemGlyph, item_text);
    apply_text_long_dot(&theme, ui_pnlContent, ui_lblResultItem, item_text);

    ui_count++;
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

    struct json directory = json_object_get(root, "directory");
    if (json_exists(directory) && json_type(directory) == JSON_STRING) {
        json_string_copy(directory, rom_dir, sizeof(rom_dir));
    }

    search_folders = json_object_get(root, "folders");
    if (json_exists(search_folders) && json_type(search_folders) == JSON_OBJECT) {
        struct json folder = json_first(search_folders);
        while (json_exists(folder)) {

            if (json_type(folder) == JSON_STRING) {
                char folder_name[MAX_BUFFER_SIZE];
                json_string_copy(folder, folder_name, sizeof(folder_name));

                static char bracket_folder_name[MAX_BUFFER_SIZE];
                snprintf(bracket_folder_name, sizeof(bracket_folder_name), "[%s]",
                         folder_name);

                LOG_DEBUG(mux_module, "FOLDER\t\t%s", folder_name)
                gen_label("", "");
                gen_label("folder", bracket_folder_name);
            }

            struct json content = json_object_get(folder, "content");
            if (json_exists(content) && json_type(content) == JSON_ARRAY) {
                for (size_t i = 0; i < json_array_count(content); i++) {
                    struct json item = json_array_get(content, i);
                    if (json_type(item) == JSON_STRING) {
                        char content_name[MAX_BUFFER_SIZE];
                        json_string_copy(item, content_name, sizeof(content_name));
                        LOG_DEBUG(mux_module, "CONTENT\t\t\t%s", content_name)
                        gen_label("content", content_name);
                    }
                }
            }

            folder = json_next(folder);
        }
    }
}

void reset_osk() {
    key_curr = 0;
    lv_btnmatrix_clear_btn_ctrl_all(key_entry, LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_set_selected_btn(key_entry, key_curr);
    lv_btnmatrix_set_btn_ctrl(key_entry, lv_btnmatrix_get_selected_btn(key_entry), LV_BTNMATRIX_CTRL_CHECKED);
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_value, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound, 0, 0);
    }
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_value, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void handle_keyboard_press(void) {
    play_sound("navigate", nav_sound, 0, 0);

    const char *is_key;

    is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);

    if (strcasecmp(is_key, "OK") == 0) {
        key_show = 0;
        struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

        if (element_focused == ui_lblLookup) {
            lv_label_set_text(ui_lblLookupValue,
                              lv_textarea_get_text(ui_txtEntry));
        }

        reset_osk();

        lv_textarea_set_text(ui_txtEntry, "");
        lv_group_set_focus_cb(ui_group, NULL);
        lv_obj_add_flag(ui_pnlEntry, LV_OBJ_FLAG_HIDDEN);
    } else if (strcmp(is_key, "ABC") == 0) {
        lv_btnmatrix_set_map(key_entry, key_upper_map);
    } else if (strcmp(is_key, "!@#") == 0) {
        lv_btnmatrix_set_map(key_entry, key_special_map);
    } else if (strcmp(is_key, "abc") == 0) {
        lv_btnmatrix_set_map(key_entry, key_lower_map);
    } else {
        lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
    }
}

void handle_keyboard_close(void) {
    play_sound("keypress", nav_sound, 0, 0);
    key_show = 0;
    reset_osk();
    lv_textarea_set_text(ui_txtEntry, "");
    lv_group_set_focus_cb(ui_group, NULL);
    lv_obj_add_flag(ui_pnlEntry, LV_OBJ_FLAG_HIDDEN);
}

void handle_keyboard_backspace(void) {
    play_sound("keypress", nav_sound, 0, 0);
    lv_textarea_del_char(ui_txtEntry);
}

void handle_keyboard_swap(void) {
    play_sound("keypress", nav_sound, 0, 0);

    if (key_show == 1) {
        switch (key_map) {
            case 0:
                lv_btnmatrix_set_map(key_entry, key_upper_map);
                key_map = 1;
                break;
            case 1:
                lv_btnmatrix_set_map(key_entry, key_special_map);
                key_map = 2;
                break;
            case 2:
                lv_btnmatrix_set_map(key_entry, key_lower_map);
                key_map = 0;
                break;
            default:
                break;
        }
    }
}

void handle_keyboard_up(void) {
    play_sound("keypress", nav_sound, 0, 0);

    if (key_curr >= 1) {
        switch (key_curr) {
            case 26:
                key_curr = 17;
                break;
            case 27:
                key_curr = 18;
                break;
            case 28:
                key_curr = 19;
                break;
            case 30 ... 36:
                key_curr = key_curr - 9;
                break;
            case 38:
                key_curr = 30;
                break;
            case 39:
                key_curr = 33;
                break;
            case 40:
                key_curr = 36;
                break;
            default:
                if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
                    key_curr = key_curr - 3;
                } else {
                    key_curr = key_curr - 10;
                }
                break;
        }
        if (key_curr < 0) {
            key_curr = 0;
        }
    }
    if (strcasecmp(lv_btnmatrix_get_btn_text(key_entry, key_curr), "§") == 0) {
        key_curr--;
    }
    lv_event_send(key_entry, LV_EVENT_SCROLL, &key_curr);
}

void handle_keyboard_down(void) {
    play_sound("keypress", nav_sound, 0, 0);

    int max_key;
    if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
        max_key = 11;
    } else {
        max_key = 40;
    }
    if (key_curr <= max_key) {
        switch (key_curr) {
            case 17:
                key_curr = 26;
                break;
            case 18:
                key_curr = 27;
                break;
            case 19:
                key_curr = 28;
                break;
            case 21 ... 27:
                key_curr = key_curr + 9;
                break;
            case 28:
                key_curr = 36;
                break;
            case 30:
                key_curr = 38;
                break;
            case 31 ... 35:
                key_curr = 39;
                break;
            case 36:
                key_curr = 40;
                break;
            default:
                if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
                    key_curr = key_curr + 3;
                } else {
                    key_curr = key_curr + 10;
                }
                break;
        }
        if (key_curr > max_key) {
            key_curr = max_key;
        }
    }
    if (strcasecmp(lv_btnmatrix_get_btn_text(key_entry, key_curr), "§") == 0) {
        key_curr++;
    }
    lv_event_send(key_entry, LV_EVENT_SCROLL, &key_curr);
}

void handle_keyboard_left(void) {
    if (key_curr >= 1) {
        key_curr--;
        if (key_curr < 0) {
            key_curr = 0;
        }
        if (strcasecmp(lv_btnmatrix_get_btn_text(key_entry, key_curr), "§") == 0) {
            key_curr--;
        }
        lv_event_send(key_entry, LV_EVENT_SCROLL, &key_curr);
    }
}

void handle_keyboard_right(void) {
    int max_key;
    if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
        max_key = 11;
    } else {
        max_key = 40;
    }
    if (key_curr <= max_key) {
        key_curr++;
        if (key_curr > max_key) {
            key_curr = max_key;
        }
        if (strcasecmp(lv_btnmatrix_get_btn_text(key_entry, key_curr), "§") == 0) {
            key_curr++;
        }
        lv_event_send(key_entry, LV_EVENT_SCROLL, &key_curr);
    }
}

void handle_confirm(void) {
    play_sound("confirm", nav_sound, 0, 1);
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

    if (element_focused == ui_lblLookup) {
        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

        key_show = 1;
        lv_obj_clear_flag(ui_pnlEntry, LV_OBJ_FLAG_HIDDEN);
        lv_textarea_set_text(ui_txtEntry, lv_label_get_text(lv_group_get_focused(ui_group_value)));
    } else {
        if (strlen(lv_label_get_text(ui_lblLookupValue)) <= 2) {
            toast_message(TS("Lookup has to be 3 characters or more!"), 1000, 1000);
            return;
        }

        toast_message(TS("Searching..."), 1000, 1000);

        static char command[MAX_BUFFER_SIZE];
        snprintf(command, sizeof(command), "/opt/muos/script/mux/find.sh \"%s\" \"%s\"",
                 rom_dir, lv_label_get_text(ui_lblLookupValue));

        system(command);

        load_mux("search");
        mux_input_stop();
    }
}

void handle_back(void) {
    play_sound("back", nav_sound, 0, 1);
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
        handle_keyboard_close();
        return;
    }

    handle_back();
}

void handle_x(void) {
    if (msgbox_active) return;

    if (key_show) {
        handle_keyboard_backspace();
        return;
    }
}

void handle_y(void) {
    if (msgbox_active) return;

    if (key_show) {
        handle_keyboard_swap();
        return;
    }
}

void handle_help(void) {
    if (msgbox_active || key_show) {
        return;
    }

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 0, 0);
        show_help(lv_group_get_focused(ui_group));
    }
}

void handle_up(void) {
    if (key_show) {
        handle_keyboard_up();
        return;
    }

    handle_list_nav_up();
}

void handle_up_hold(void) {
    if (key_show) {
        handle_keyboard_up();
        return;
    }

    handle_list_nav_up_hold();
}

void handle_down(void) {
    if (key_show) {
        handle_keyboard_down();
        return;
    }

    handle_list_nav_down();
}

void handle_down_hold(void) {
    if (key_show) {
        handle_keyboard_down();
        return;
    }

    handle_list_nav_down_hold();
}

void handle_left(void) {
    if (key_show) {
        handle_keyboard_left();
        return;
    }
}

void handle_right(void) {
    if (key_show) {
        handle_keyboard_right();
        return;
    }
}

void handle_left_hold(void) {
    if (key_show) {
        handle_keyboard_left();
        return;
    }
}

void handle_right_hold(void) {
    if (key_show) {
        handle_keyboard_right();
        return;
    }
}

void handle_l1(void) {
    if (key_show) {
        return;
    }

    handle_list_nav_page_up();
}

void handle_r1(void) {
    if (key_show) {
        return;
    }

    handle_list_nav_page_down();
}

static void osk_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    switch (code) {
        case LV_EVENT_SCROLL:
            lv_btnmatrix_set_selected_btn(obj, key_curr);
            lv_btnmatrix_set_btn_ctrl(obj, lv_btnmatrix_get_selected_btn(obj), LV_BTNMATRIX_CTRL_CHECKED);
            break;
        case LV_EVENT_CLICKED:
            lv_textarea_add_text(ui_txtEntry, lv_btnmatrix_get_btn_text(obj, lv_btnmatrix_get_selected_btn(obj)));
            break;
        default:
            break;
    }
}

void init_elements() {
    ui_mux_panels[0] = ui_pnlFooter;
    ui_mux_panels[1] = ui_pnlHeader;
    ui_mux_panels[2] = ui_pnlHelp;
    ui_mux_panels[3] = ui_pnlEntry;
    ui_mux_panels[4] = ui_pnlProgressBrightness;
    ui_mux_panels[5] = ui_pnlProgressVolume;
    ui_mux_panels[6] = ui_pnlMessage;

    adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

    if (bar_footer) {
        lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (bar_header) {
        lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblNavA, TG("Select"));
    lv_label_set_text(ui_lblNavB, TG("Back"));

    lv_obj_t *nav_hide[] = {
            ui_lblNavCGlyph,
            ui_lblNavC,
            ui_lblNavXGlyph,
            ui_lblNavX,
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
    lv_obj_set_user_data(ui_lblSearch, "search");

    if (TEST_IMAGE) display_testing_message(ui_screen);

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

    lv_obj_add_event_cb(key_entry, osk_handler, LV_EVENT_ALL, NULL);

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
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    mux_module = basename(argv[0]);
    load_device(&device);

    static char search_result[MAX_BUFFER_SIZE];
    snprintf(search_result, sizeof(search_result), "%s/MUOS/info/search.json",
             device.STORAGE.ROM.MOUNT);

    char *json_content;

    if (file_exist(search_result)) {
        json_content = read_text_from_file(search_result);
        if (json_content) {
            got_results = 1;
        } else {
            LOG_ERROR(mux_module, "Error reading search results")
        }
    } else {
        char *cmd_help = "\nmuOS Extras - Content Search\nUsage: %s <-d>\n\nOptions:\n"
                         "\t-d Name of directory to search\n\n";

        int opt;
        while ((opt = getopt(argc, argv, "d:")) != -1) {
            switch (opt) {
                case 'd':
                    snprintf(rom_dir, sizeof(rom_dir), "%s", optarg);
                    break;
                default:
                    fprintf(stderr, cmd_help, argv[0]);
                    return 1;
            }
        }
    }

    lv_init();
    fbdev_init(device.SCREEN.DEVICE);

    static lv_disp_draw_buf_t disp_buf;
    uint32_t disp_buf_size = device.SCREEN.WIDTH * device.SCREEN.HEIGHT;

    lv_color_t * buf1 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));
    lv_color_t * buf2 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, disp_buf_size);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res = device.SCREEN.WIDTH;
    disp_drv.ver_res = device.SCREEN.HEIGHT;
    disp_drv.sw_rotate = device.SCREEN.ROTATE;
    disp_drv.rotated = device.SCREEN.ROTATE;
    disp_drv.full_refresh = 0;
    disp_drv.direct_mode = 0;
    lv_disp_drv_register(&disp_drv);

    load_config(&config);
    load_theme(&theme, &config, &device, basename(argv[0]));
    load_language(mux_module);

    ui_common_screen_init(&theme, &device, TS("SEARCH CONTENT"));
    ui_init(ui_screen, ui_pnlContent, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, basename(argv[0]));

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
        perror("Failed to open joystick device");
        return 1;
    }

    js_fd_sys = open(device.INPUT.EV0, O_RDONLY);
    if (js_fd_sys < 0) {
        perror("Failed to open joystick device");
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

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = 16 /* ~60 FPS */,
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
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_down_hold,
                    [MUX_INPUT_DPAD_LEFT] = handle_left_hold,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right_hold,
                    [MUX_INPUT_L1] = handle_l1,
                    [MUX_INPUT_R1] = handle_r1,
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
