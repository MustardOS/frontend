#include "../lvgl/lvgl.h"
#include "common.h"
#include "options.h"
#include "osk.h"

int key_show;
int key_curr;
int key_map;

lv_obj_t *key_entry;
lv_obj_t *num_entry;

const char *key_lower_map[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
                               "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
                               "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
                               "§", "z", "x", "c", "v", "b", "n", "m", "§", "\n",
                               OSK_UPPER, " ", OSK_DONE, NULL
};

const char *key_upper_map[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
                               "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
                               "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
                               "§", "Z", "X", "C", "V", "B", "N", "M", "§", "\n",
                               OSK_CHAR, " ", OSK_DONE, NULL
};

const char *key_special_map[] = {"!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "\n",
                                 "`", "~", "_", "-", "+", "=", "{", "}", "[", "]", "\n",
                                 "|", "\\", ":", ";", "\"", "'", "!", "@", "#", "\n",
                                 "§", "<", ">", ",", ".", "?", "/", "$", "§", "\n",
                                 OSK_LOWER, " ", OSK_DONE, NULL
};

const char *key_number_map[] = {"7", "8", "9", "\n",
                                "4", "5", "6", "\n",
                                "1", "2", "3", "\n",
                                "0", ".", OSK_DONE, NULL};

// Track modifier states
static int shift_pressed = 0;
static int caps_lock_active = 0;

// Function to map key codes to characters
char get_shifted_char(uint16_t key) {
    bool alpha_shifted = (shift_pressed && !caps_lock_active) || (!shift_pressed && caps_lock_active);

    switch (key) {
        case KEY_A: return alpha_shifted ? 'A' : 'a';
        case KEY_B: return alpha_shifted ? 'B' : 'b';
        case KEY_C: return alpha_shifted ? 'C' : 'c';
        case KEY_D: return alpha_shifted ? 'D' : 'd';
        case KEY_E: return alpha_shifted ? 'E' : 'e';
        case KEY_F: return alpha_shifted ? 'F' : 'f';
        case KEY_G: return alpha_shifted ? 'G' : 'g';
        case KEY_H: return alpha_shifted ? 'H' : 'h';
        case KEY_I: return alpha_shifted ? 'I' : 'i';
        case KEY_J: return alpha_shifted ? 'J' : 'j';
        case KEY_K: return alpha_shifted ? 'K' : 'k';
        case KEY_L: return alpha_shifted ? 'L' : 'l';
        case KEY_M: return alpha_shifted ? 'M' : 'm';
        case KEY_N: return alpha_shifted ? 'N' : 'n';
        case KEY_O: return alpha_shifted ? 'O' : 'o';
        case KEY_P: return alpha_shifted ? 'P' : 'p';
        case KEY_Q: return alpha_shifted ? 'Q' : 'q';
        case KEY_R: return alpha_shifted ? 'R' : 'r';
        case KEY_S: return alpha_shifted ? 'S' : 's';
        case KEY_T: return alpha_shifted ? 'T' : 't';
        case KEY_U: return alpha_shifted ? 'U' : 'u';
        case KEY_V: return alpha_shifted ? 'V' : 'v';
        case KEY_W: return alpha_shifted ? 'W' : 'w';
        case KEY_X: return alpha_shifted ? 'X' : 'x';
        case KEY_Y: return alpha_shifted ? 'Y' : 'y';
        case KEY_Z: return alpha_shifted ? 'Z' : 'z';
        case KEY_1: return shift_pressed ? '!' : '1';
        case KEY_2: return shift_pressed ? '@' : '2';
        case KEY_3: return shift_pressed ? '#' : '3';
        case KEY_4: return shift_pressed ? '$' : '4';
        case KEY_5: return shift_pressed ? '%' : '5';
        case KEY_6: return shift_pressed ? '^' : '6';
        case KEY_7: return shift_pressed ? '&' : '7';
        case KEY_8: return shift_pressed ? '*' : '8';
        case KEY_9: return shift_pressed ? '(' : '9';
        case KEY_0: return shift_pressed ? ')' : '0';
        case KEY_MINUS: return shift_pressed ? '_' : '-';
        case KEY_EQUAL: return shift_pressed ? '+' : '=';
        case KEY_LEFTBRACE: return shift_pressed ? '{' : '[';
        case KEY_RIGHTBRACE: return shift_pressed ? '}' : ']';
        case KEY_SEMICOLON: return shift_pressed ? ':' : ';';
        case KEY_APOSTROPHE: return shift_pressed ? '"' : '\'';
        case KEY_GRAVE: return shift_pressed ? '~' : '`';
        case KEY_BACKSLASH: return shift_pressed ? '|' : '\\';
        case KEY_COMMA: return shift_pressed ? '<' : ',';
        case KEY_DOT: return shift_pressed ? '>' : '.';
        case KEY_SLASH: return shift_pressed ? '?' : '/';
        case KEY_SPACE: return ' ';
        default: return 0;
    }
}

void process_key_event(struct input_event *ev, lv_obj_t *entry) {
    if (ev->type == EV_KEY) {
        if (ev->code == KEY_LEFTSHIFT || ev->code == KEY_RIGHTSHIFT) {
            shift_pressed = (ev->value > 0);
            return;
        }

        if (ev->code == KEY_CAPSLOCK && ev->value == 1) {
            caps_lock_active = !caps_lock_active;
            return;
        }

        if (ev->value > 0) {  // Key press event
            char key_char = get_shifted_char(ev->code);

            if (key_char) {
                lv_textarea_add_char(entry, key_char);
            } else if (ev->code == KEY_BACKSPACE) {
                lv_textarea_del_char(entry);
            }
        }
    }
}

void osk_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    lv_obj_t *element = lv_event_get_user_data(e);

    switch (code) {
        case LV_EVENT_SCROLL:
            lv_btnmatrix_set_selected_btn(obj, key_curr);
            lv_btnmatrix_set_btn_ctrl(obj, lv_btnmatrix_get_selected_btn(obj), LV_BTNMATRIX_CTRL_CHECKED);
            break;
        case LV_EVENT_CLICKED:
            lv_textarea_add_text(element, lv_btnmatrix_get_btn_text(obj, lv_btnmatrix_get_selected_btn(obj)));
            break;
        default:
            break;
    }
}

void init_osk(lv_obj_t *ui_pnlEntry, lv_obj_t *ui_txtEntry, bool include_numkey) {
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
    lv_obj_set_style_pad_top(ui_txtEntry, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_txtEntry, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_txtEntry, 6, LV_PART_MAIN | LV_STATE_DEFAULT);

    const lv_font_t *font = lv_obj_get_style_text_font(ui_txtEntry, LV_PART_MAIN);
    int32_t border_width = lv_obj_get_style_border_width(ui_txtEntry, LV_PART_MAIN);
    lv_coord_t font_height = lv_font_get_line_height(font);
    lv_obj_set_height(ui_txtEntry, font_height + 12 + border_width * 2);

    if (include_numkey) {
        num_entry = lv_btnmatrix_create(ui_pnlEntry);

        lv_obj_set_width(num_entry, device.MUX.WIDTH * 5 / 6);
        lv_obj_set_height(num_entry, device.MUX.HEIGHT * 5 / 9);
    
        lv_btnmatrix_set_one_checked(num_entry, 1);
        lv_btnmatrix_set_map(num_entry, key_number_map);
        lv_btnmatrix_set_selected_btn(num_entry, key_curr);
        lv_btnmatrix_set_btn_ctrl(num_entry, lv_btnmatrix_get_selected_btn(num_entry), LV_BTNMATRIX_CTRL_CHECKED);
        lv_obj_align(num_entry, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_event_cb(num_entry, osk_handler, LV_EVENT_ALL, ui_txtEntry);
    
        lv_obj_set_style_border_width(num_entry, 3, LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_width(num_entry, 1, LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(num_entry, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    
        lv_obj_set_style_bg_color(num_entry, lv_color_hex(theme.OSK.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(num_entry, lv_color_hex(theme.OSK.ITEM.BACKGROUND), LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(num_entry, lv_color_hex(theme.OSK.ITEM.BACKGROUND_FOCUS),
                                  LV_PART_ITEMS | LV_STATE_CHECKED);
    
        lv_obj_set_style_bg_opa(num_entry, theme.OSK.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(num_entry, theme.OSK.ITEM.BACKGROUND_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(num_entry, theme.OSK.ITEM.BACKGROUND_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);
    
        lv_obj_set_style_text_color(num_entry, lv_color_hex(theme.OSK.TEXT), LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(num_entry, lv_color_hex(theme.OSK.TEXT_FOCUS), LV_PART_ITEMS | LV_STATE_CHECKED);
    
        lv_obj_set_style_text_opa(num_entry, theme.OSK.TEXT_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(num_entry, theme.OSK.TEXT_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);
    
        lv_obj_set_style_border_color(num_entry, lv_color_hex(theme.OSK.BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(num_entry, lv_color_hex(theme.OSK.ITEM.BORDER), LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(num_entry, lv_color_hex(theme.OSK.ITEM.BORDER_FOCUS),
                                      LV_PART_ITEMS | LV_STATE_CHECKED);
    
        lv_obj_set_style_border_opa(num_entry, theme.OSK.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(num_entry, theme.OSK.ITEM.BORDER_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(num_entry, theme.OSK.ITEM.BORDER_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);
    
        lv_obj_set_style_radius(num_entry, theme.OSK.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(num_entry, theme.OSK.ITEM.RADIUS, LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(num_entry, theme.OSK.ITEM.RADIUS, LV_PART_ITEMS | LV_STATE_CHECKED);
    
        lv_obj_set_style_pad_top(num_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(num_entry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(num_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(num_entry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_gap(num_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void reset_osk(lv_obj_t *osk) {
    key_curr = 0;
    lv_btnmatrix_clear_btn_ctrl_all(osk, LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_set_selected_btn(osk, key_curr);
    lv_btnmatrix_set_btn_ctrl(osk, lv_btnmatrix_get_selected_btn(key_entry), LV_BTNMATRIX_CTRL_CHECKED);
}

void close_osk(lv_obj_t *osk, lv_group_t *ui, lv_obj_t *entry, lv_obj_t *panel) {
    play_sound("keypress", nav_sound, 0, 0);
    key_show = 0;
    reset_osk(osk);
    lv_textarea_set_text(entry, "");
    lv_group_set_focus_cb(ui, NULL);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

void key_up() {
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
    lv_event_send(num_entry, LV_EVENT_SCROLL, &key_curr);
}

void key_down() {
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
    lv_event_send(num_entry, LV_EVENT_SCROLL, &key_curr);
}

void key_left() {
    play_sound("keypress", nav_sound, 0, 0);

    if (key_curr >= 1) {
        key_curr--;
        if (key_curr < 0) {
            key_curr = 0;
        }
        if (strcasecmp(lv_btnmatrix_get_btn_text(key_entry, key_curr), "§") == 0) {
            key_curr--;
        }
        lv_event_send(key_entry, LV_EVENT_SCROLL, &key_curr);
        lv_event_send(num_entry, LV_EVENT_SCROLL, &key_curr);
    }
}

void key_right() {
    play_sound("keypress", nav_sound, 0, 0);

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
        lv_event_send(num_entry, LV_EVENT_SCROLL, &key_curr);
    }
}

void key_swap() {
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

void key_backspace(lv_obj_t *entry) {
    play_sound("keypress", nav_sound, 0, 0);
    lv_textarea_del_char(entry);
}
