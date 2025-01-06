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
