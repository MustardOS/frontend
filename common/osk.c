#include "../lvgl/lvgl.h"
#include "common.h"
#include "config.h"
#include "device.h"
#include "options.h"
#include "theme.h"
#include "language.h"
#include "ui_common.h"
#include "osk.h"

int key_show;
int key_curr;
int key_map;

lv_obj_t *key_entry;
lv_obj_t *num_entry;

typedef struct {
    lv_obj_t *box;
    lv_obj_t *glyph;
    lv_obj_t *label;
} osk_nav_hint_t;

static lv_obj_t *nav_hint_container = NULL;

static osk_nav_hint_t nav_hint_a = {NULL, NULL, NULL};
static osk_nav_hint_t nav_hint_b = {NULL, NULL, NULL};
static osk_nav_hint_t nav_hint_x = {NULL, NULL, NULL};
static osk_nav_hint_t nav_hint_y = {NULL, NULL, NULL};

enum {
    OSK_MAP_LOWER = 0,
    OSK_MAP_UPPER_ONCE = 1,
    OSK_MAP_SPECIAL = 2,
    OSK_MAP_UPPER_LOCK = 3
};

static int last_letter_btn = -1;
static uint16_t osk_max_len = 0;
static bool shift_once = false;
static int shift_pressed = 0;
static int caps_lock_active = 0;

#define OSK_PRESS_FLASH_MS 80

#define OSK_MODE_BTN_INDEX  40
#define OSK_SPACE_BTN_INDEX 41
#define OSK_OK_BTN_INDEX    42

#define OSK_SIDE_BTN_WIDTH  2
#define OSK_SPACE_BTN_WIDTH 7

#define OSK_MAX_BTNS 64
#define OSK_MAX_ROWS 8

#define OSK_NAV_HINT_PAIR_GAP  23
#define OSK_NAV_HINT_ITEM_GAP  7

const char *key_lower_map[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
        "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
        "a", "s", "d", "f", "g", "h", "j", "k", "l", "'", "\n",
        "z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "\n",
        "abc", " ", OSK_DONE, NULL
};

const char *key_upper_map[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
        "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
        "A", "S", "D", "F", "G", "H", "J", "K", "L", "\"", "\n",
        "Z", "X", "C", "V", "B", "N", "M", "<", ">", "?", "\n",
        "ABC", " ", OSK_DONE, NULL
};

const char *key_upper_lock_map[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
        "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
        "A", "S", "D", "F", "G", "H", "J", "K", "L", "\"", "\n",
        "Z", "X", "C", "V", "B", "N", "M", "<", ">", "?", "\n",
        "CAP", " ", OSK_DONE, NULL
};

const char *key_special_map[] = {
        "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "\n",
        "`", "~", "_", "-", "+", "=", "[", "]", "{", "}", "\n",
        "|", "\\", ":", ";", "<", ">", "?", "/", "\"", "'", "\n",
        "€", "£", "¥", "•", "·", "§", "¶", "©", "®", "™", "\n",
        "#$&", " ", OSK_DONE, NULL
};

const char *key_number_map[] = {
        "7", "8", "9", "\n",
        "4", "5", "6", "\n",
        "1", "2", "3", "\n",
        "0", ".", OSK_DONE, NULL
};

typedef struct {
    uint16_t btn_count;
    uint16_t row_count;
    uint16_t row_start[OSK_MAX_ROWS];
    uint16_t row_size[OSK_MAX_ROWS];
    uint16_t row_of[OSK_MAX_BTNS];
    uint16_t col_of[OSK_MAX_BTNS];
} osk_layout_t;

static osk_layout_t key_layout;

static void rebuild_layout(lv_obj_t *osk, osk_layout_t *out) {
    memset(out, 0, sizeof(*out));
    if (!osk || !lv_obj_is_valid(osk)) return;

    const char **map = lv_btnmatrix_get_map(osk);
    if (!map) return;

    uint16_t btn_idx = 0;
    uint16_t row = 0;
    uint16_t col_in_row = 0;

    out->row_start[0] = 0;

    for (uint16_t i = 0; map[i] != NULL && map[i][0] != '\0'; i++) {
        if (strcmp(map[i], "\n") == 0) {
            out->row_size[row] = col_in_row;
            row++;

            if (row >= OSK_MAX_ROWS) break;

            out->row_start[row] = btn_idx;
            col_in_row = 0;

            continue;
        }

        if (btn_idx >= OSK_MAX_BTNS) break;

        out->row_of[btn_idx] = row;
        out->col_of[btn_idx] = col_in_row;

        btn_idx++;
        col_in_row++;
    }

    if (row < OSK_MAX_ROWS && out->row_size[row] == 0 && col_in_row > 0) {
        out->row_size[row] = col_in_row;
        row++;
    }

    out->btn_count = btn_idx;
    out->row_count = row;
}

static void osk_apply_selection(lv_obj_t *osk, uint16_t btn) {
    if (!osk || !lv_obj_is_valid(osk)) return;

    osk_layout_t tmp;
    rebuild_layout(osk, &tmp);

    if (tmp.btn_count == 0) return;

    uint16_t clamped = (btn < tmp.btn_count) ? btn : (uint16_t) (tmp.btn_count - 1);

    lv_btnmatrix_set_selected_btn(osk, clamped);
    lv_btnmatrix_set_btn_ctrl(osk, lv_btnmatrix_get_selected_btn(osk), LV_BTNMATRIX_CTRL_CHECKED);
}

static void osk_dispatch(void) {
    lv_event_send(key_entry, LV_EVENT_SCROLL, &key_curr);
    if (num_entry && lv_obj_is_valid(num_entry)) lv_event_send(num_entry, LV_EVENT_SCROLL, &key_curr);
}

static void style_osk_nav_hint_box(lv_obj_t *box) {
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_pad_all(box, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(box, OSK_NAV_HINT_ITEM_GAP, MU_OBJ_MAIN_DEFAULT);
}

static void style_osk_nav_hint_label(lv_obj_t *label) {
    lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_text_color(label, lv_color_hex(theme.OSK.TEXT), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(label, theme.OSK.TEXT_ALPHA, MU_OBJ_MAIN_DEFAULT);
}

static void set_osk_nav_hint_glyph(lv_obj_t *glyph, const char *glyph_name, struct footer_glyph nav_glyph) {
    if (!glyph || !lv_obj_is_valid(glyph)) return;

    char image_path[MAX_BUFFER_SIZE];
    char image_embed[MAX_BUFFER_SIZE];

    if (nav_glyph.GLYPH_ALPHA > 0 && generate_image_embed(mux_dim, "footer", glyph_name, image_path, sizeof(image_path), image_embed, sizeof(image_embed))) {
        lv_img_set_src(glyph, image_embed);
        lv_obj_clear_flag(glyph, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(glyph, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_set_size(glyph, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(glyph, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_img_opa(glyph, nav_glyph.GLYPH_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor(glyph, lv_color_hex(nav_glyph.GLYPH), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_img_recolor_opa(glyph, nav_glyph.GLYPH_RECOLOUR_ALPHA, MU_OBJ_MAIN_DEFAULT);
}

static osk_nav_hint_t create_osk_nav_hint_item(lv_obj_t *parent, const char *text) {
    osk_nav_hint_t item;

    item.box = lv_obj_create(parent);
    style_osk_nav_hint_box(item.box);

    item.glyph = lv_img_create(item.box);

    item.label = lv_label_create(item.box);
    lv_label_set_text(item.label, text);
    style_osk_nav_hint_label(item.label);

    return item;
}

static void set_osk_nav_hint_item(osk_nav_hint_t *item, const char *glyph_name, struct footer_glyph nav_glyph, const char *text) {
    if (!item || !item->box || !lv_obj_is_valid(item->box)) return;

    set_osk_nav_hint_glyph(item->glyph, glyph_name, nav_glyph);
    if (item->label && lv_obj_is_valid(item->label)) lv_label_set_text(item->label, text);
}

static void update_nav_hint(void) {
    if (!nav_hint_container || !lv_obj_is_valid(nav_hint_container)) return;

    const int swap = config.SETTINGS.REMAP.LAYOUT;

    set_osk_nav_hint_item(&nav_hint_a, swap ? "B" : "A", theme.NAV.A, lang.GENERIC.TYPE);
    set_osk_nav_hint_item(&nav_hint_b, swap ? "A" : "B", theme.NAV.B, lang.GENERIC.BACKSPACE);
    set_osk_nav_hint_item(&nav_hint_x, swap ? "Y" : "X", theme.NAV.X, lang.GENERIC.CLOSE);
    set_osk_nav_hint_item(&nav_hint_y, swap ? "X" : "Y", theme.NAV.Y, lang.GENERIC.SPACE);

    if (nav_hint_y.box && lv_obj_is_valid(nav_hint_y.box)) {
        if (key_show == 2) {
            lv_obj_add_flag(nav_hint_y.box, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(nav_hint_y.box, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void apply_osk_theme(lv_obj_t *osk) {
    lv_obj_set_style_border_width(osk, 3, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(osk, 1, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(osk, 2, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_bg_color(osk, lv_color_hex(theme.OSK.BACKGROUND), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(osk, lv_color_hex(theme.OSK.ITEM.BACKGROUND), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(osk, lv_color_hex(theme.OSK.ITEM.BACKGROUND_FOCUS), LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_bg_opa(osk, theme.OSK.BACKGROUND_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(osk, theme.OSK.ITEM.BACKGROUND_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(osk, theme.OSK.ITEM.BACKGROUND_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_text_color(osk, lv_color_hex(theme.OSK.TEXT), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(osk, lv_color_hex(theme.OSK.TEXT_FOCUS), LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_text_opa(osk, theme.OSK.TEXT_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(osk, theme.OSK.TEXT_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_border_color(osk, lv_color_hex(theme.OSK.BORDER), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(osk, lv_color_hex(theme.OSK.ITEM.BORDER), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(osk, lv_color_hex(theme.OSK.ITEM.BORDER_FOCUS), LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_border_opa(osk, theme.OSK.BORDER_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(osk, theme.OSK.ITEM.BORDER_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(osk, theme.OSK.ITEM.BORDER_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_radius(osk, theme.OSK.RADIUS, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(osk, theme.OSK.ITEM.RADIUS, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(osk, theme.OSK.ITEM.RADIUS, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_pad_top(osk, 10, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(osk, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(osk, 10, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(osk, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_gap(osk, 10, MU_OBJ_MAIN_DEFAULT);
}

typedef struct {
    lv_obj_t *osk;
} osk_flash_ctx_t;

static void flash_revert_cb(lv_timer_t *timer) {
    osk_flash_ctx_t *ctx = (osk_flash_ctx_t *) timer->user_data;

    if (ctx && ctx->osk && lv_obj_is_valid(ctx->osk)) {
        lv_obj_set_style_bg_color(ctx->osk, lv_color_hex(theme.OSK.ITEM.BACKGROUND_FOCUS), LV_PART_ITEMS | LV_STATE_CHECKED);
    }

    lv_mem_free(ctx);
    lv_timer_del(timer);
}

static void osk_flash_press(lv_obj_t *osk) {
    lv_color_t flash = lv_color_mix(lv_color_white(), lv_color_hex(theme.OSK.ITEM.BACKGROUND_FOCUS), LV_OPA_30);
    lv_obj_set_style_bg_color(osk, flash, LV_PART_ITEMS | LV_STATE_CHECKED);

    osk_flash_ctx_t *ctx = lv_mem_alloc(sizeof(osk_flash_ctx_t));
    if (!ctx) return;

    ctx->osk = osk;
    lv_timer_create(flash_revert_cb, OSK_PRESS_FLASH_MS, ctx);
}

static void apply_action_row_widths(lv_obj_t *osk) {
    if (!osk || !lv_obj_is_valid(osk)) return;

    lv_btnmatrix_set_btn_width(osk, OSK_MODE_BTN_INDEX, OSK_SIDE_BTN_WIDTH);
    lv_btnmatrix_set_btn_width(osk, OSK_SPACE_BTN_INDEX, OSK_SPACE_BTN_WIDTH);
    lv_btnmatrix_set_btn_width(osk, OSK_OK_BTN_INDEX, OSK_SIDE_BTN_WIDTH);
}

void osk_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    lv_obj_t *element = lv_event_get_user_data(e);

    if (!obj || !lv_obj_is_valid(obj)) return;
    if (!element || !lv_obj_is_valid(element)) return;

    switch (code) {
        case LV_EVENT_SCROLL:
            osk_apply_selection(obj, (uint16_t) key_curr);
            break;
        case LV_EVENT_CLICKED: {
            uint16_t sel = lv_btnmatrix_get_selected_btn(obj);

            const char *txt = lv_btnmatrix_get_btn_text(obj, sel);
            if (!txt) break;

            if (obj == key_entry && sel == OSK_MODE_BTN_INDEX) {
                osk_flash_press(obj);
                key_swap();
                break;
            }

            if (strcmp(txt, OSK_DONE) == 0) {
                osk_flash_press(obj);
                break;
            }

            if (osk_max_len > 0) {
                const char *current = lv_textarea_get_text(element);
                if (current && strlen(current) >= osk_max_len) break;
            }

            lv_textarea_add_text(element, txt);
            osk_flash_press(obj);

            if (shift_once && key_map == OSK_MAP_UPPER_ONCE) {
                shift_once = false;
                key_map = OSK_MAP_LOWER;

                lv_btnmatrix_set_map(key_entry, key_lower_map);
                apply_action_row_widths(key_entry);

                rebuild_layout(key_entry, &key_layout);
                osk_apply_selection(key_entry, (uint16_t) key_curr);
            }
            break;
        }

        default:
            break;
    }
}

char get_shifted_char(uint16_t key) {
    bool alpha_shifted = (shift_pressed && !caps_lock_active) || (!shift_pressed && caps_lock_active);

    switch (key) {
        case KEY_A:
            return alpha_shifted ? 'A' : 'a';
        case KEY_B:
            return alpha_shifted ? 'B' : 'b';
        case KEY_C:
            return alpha_shifted ? 'C' : 'c';
        case KEY_D:
            return alpha_shifted ? 'D' : 'd';
        case KEY_E:
            return alpha_shifted ? 'E' : 'e';
        case KEY_F:
            return alpha_shifted ? 'F' : 'f';
        case KEY_G:
            return alpha_shifted ? 'G' : 'g';
        case KEY_H:
            return alpha_shifted ? 'H' : 'h';
        case KEY_I:
            return alpha_shifted ? 'I' : 'i';
        case KEY_J:
            return alpha_shifted ? 'J' : 'j';
        case KEY_K:
            return alpha_shifted ? 'K' : 'k';
        case KEY_L:
            return alpha_shifted ? 'L' : 'l';
        case KEY_M:
            return alpha_shifted ? 'M' : 'm';
        case KEY_N:
            return alpha_shifted ? 'N' : 'n';
        case KEY_O:
            return alpha_shifted ? 'O' : 'o';
        case KEY_P:
            return alpha_shifted ? 'P' : 'p';
        case KEY_Q:
            return alpha_shifted ? 'Q' : 'q';
        case KEY_R:
            return alpha_shifted ? 'R' : 'r';
        case KEY_S:
            return alpha_shifted ? 'S' : 's';
        case KEY_T:
            return alpha_shifted ? 'T' : 't';
        case KEY_U:
            return alpha_shifted ? 'U' : 'u';
        case KEY_V:
            return alpha_shifted ? 'V' : 'v';
        case KEY_W:
            return alpha_shifted ? 'W' : 'w';
        case KEY_X:
            return alpha_shifted ? 'X' : 'x';
        case KEY_Y:
            return alpha_shifted ? 'Y' : 'y';
        case KEY_Z:
            return alpha_shifted ? 'Z' : 'z';
        case KEY_1:
            return shift_pressed ? '!' : '1';
        case KEY_2:
            return shift_pressed ? '@' : '2';
        case KEY_3:
            return shift_pressed ? '#' : '3';
        case KEY_4:
            return shift_pressed ? '$' : '4';
        case KEY_5:
            return shift_pressed ? '%' : '5';
        case KEY_6:
            return shift_pressed ? '^' : '6';
        case KEY_7:
            return shift_pressed ? '&' : '7';
        case KEY_8:
            return shift_pressed ? '*' : '8';
        case KEY_9:
            return shift_pressed ? '(' : '9';
        case KEY_0:
            return shift_pressed ? ')' : '0';
        case KEY_MINUS:
            return shift_pressed ? '_' : '-';
        case KEY_EQUAL:
            return shift_pressed ? '+' : '=';
        case KEY_LEFTBRACE:
            return shift_pressed ? '{' : '[';
        case KEY_RIGHTBRACE:
            return shift_pressed ? '}' : ']';
        case KEY_SEMICOLON:
            return shift_pressed ? ':' : ';';
        case KEY_APOSTROPHE:
            return shift_pressed ? '"' : '\'';
        case KEY_GRAVE:
            return shift_pressed ? '~' : '`';
        case KEY_BACKSLASH:
            return shift_pressed ? '|' : '\\';
        case KEY_COMMA:
            return shift_pressed ? '<' : ',';
        case KEY_DOT:
            return shift_pressed ? '>' : '.';
        case KEY_SLASH:
            return shift_pressed ? '?' : '/';
        case KEY_SPACE:
            return ' ';
        default:
            return 0;
    }
}

void process_key_event(struct input_event *ev, lv_obj_t *entry) {
    if (ev->type != EV_KEY) return;

    if (ev->code == KEY_LEFTSHIFT || ev->code == KEY_RIGHTSHIFT) {
        shift_pressed = (ev->value > 0);
        return;
    }

    if (ev->code == KEY_CAPSLOCK && ev->value == 1) {
        caps_lock_active = !caps_lock_active;
        return;
    }

    if (ev->value > 0) {
        char key_char = get_shifted_char(ev->code);
        if (key_char) {
            if (osk_max_len > 0) {
                const char *current = lv_textarea_get_text(entry);
                if (current && strlen(current) >= osk_max_len) return;
            }
            lv_textarea_add_char(entry, key_char);
        } else if (ev->code == KEY_BACKSPACE) {
            lv_textarea_del_char(entry);
        }
    }
}

void init_osk(lv_obj_t *ui_pnlEntry, lv_obj_t *ui_txtEntry, bool include_numpad, bool is_password, uint16_t max_len) {
    osk_max_len = max_len;
    shift_once = false;
    last_letter_btn = -1;

    lv_textarea_set_password_mode(ui_txtEntry, is_password);
    if (max_len > 0) lv_textarea_set_max_length(ui_txtEntry, max_len);

    const lv_font_t *font = lv_obj_get_style_text_font(ui_txtEntry, LV_PART_MAIN);
    int32_t border_width = lv_obj_get_style_border_width(ui_txtEntry, LV_PART_MAIN);

    lv_coord_t font_height = lv_font_get_line_height(font);
    lv_obj_set_height(ui_txtEntry, font_height + 12 + border_width * 2);

    lv_obj_set_style_text_color(ui_txtEntry, lv_color_hex(theme.OSK.TEXT), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_txtEntry, theme.OSK.TEXT_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_txtEntry, lv_color_hex(theme.OSK.BACKGROUND), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_txtEntry, theme.OSK.BACKGROUND_ALPHA, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_pad_top(ui_txtEntry, 6, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_txtEntry, 6, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_txtEntry, 6, MU_OBJ_MAIN_DEFAULT);

    const lv_coord_t matrix_h = device.MUX.HEIGHT * 5 / 9;
    const lv_coord_t matrix_half = matrix_h / 2;
    const lv_coord_t hint_gap = 12;

    key_entry = lv_btnmatrix_create(ui_pnlEntry);

    lv_obj_set_width(key_entry, device.MUX.WIDTH * 5 / 6);
    lv_obj_set_height(key_entry, matrix_h);

    lv_btnmatrix_set_one_checked(key_entry, 1);
    lv_btnmatrix_set_map(key_entry, key_lower_map);

    rebuild_layout(key_entry, &key_layout);

    apply_action_row_widths(key_entry);

    osk_apply_selection(key_entry, (uint16_t) key_curr);
    lv_obj_align(key_entry, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(key_entry, osk_handler, LV_EVENT_SCROLL, ui_txtEntry);
    lv_obj_add_event_cb(key_entry, osk_handler, LV_EVENT_CLICKED, ui_txtEntry);

    apply_osk_theme(key_entry);

    if (include_numpad) {
        num_entry = lv_btnmatrix_create(ui_pnlEntry);

        lv_obj_set_width(num_entry, device.MUX.WIDTH * 5 / 6);
        lv_obj_set_height(num_entry, matrix_h);

        lv_btnmatrix_set_one_checked(num_entry, 1);
        lv_btnmatrix_set_map(num_entry, key_number_map);
        osk_apply_selection(num_entry, (uint16_t) key_curr);

        lv_obj_align(num_entry, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_event_cb(num_entry, osk_handler, LV_EVENT_SCROLL, ui_txtEntry);
        lv_obj_add_event_cb(num_entry, osk_handler, LV_EVENT_CLICKED, ui_txtEntry);

        apply_osk_theme(num_entry);
    } else {
        num_entry = NULL;
    }

    nav_hint_container = lv_obj_create(ui_pnlEntry);

    lv_obj_remove_style_all(nav_hint_container);
    lv_obj_set_size(nav_hint_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(nav_hint_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(nav_hint_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav_hint_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_pad_all(nav_hint_container, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(nav_hint_container, OSK_NAV_HINT_PAIR_GAP, MU_OBJ_MAIN_DEFAULT);

    lv_obj_align(nav_hint_container, LV_ALIGN_CENTER, 0, matrix_half + hint_gap);

    nav_hint_a = create_osk_nav_hint_item(nav_hint_container, lang.GENERIC.TYPE);
    nav_hint_b = create_osk_nav_hint_item(nav_hint_container, lang.GENERIC.BACKSPACE);
    nav_hint_x = create_osk_nav_hint_item(nav_hint_container, lang.GENERIC.CLOSE);
    nav_hint_y = create_osk_nav_hint_item(nav_hint_container, lang.GENERIC.SPACE);

    update_nav_hint();
}

void reset_osk(lv_obj_t *osk) {
    if (!osk || !lv_obj_is_valid(osk)) return;

    key_curr = 0;
    shift_once = false;
    last_letter_btn = -1;

    lv_btnmatrix_clear_btn_ctrl_all(osk, LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_set_selected_btn(osk, key_curr);
    lv_btnmatrix_set_btn_ctrl(osk, lv_btnmatrix_get_selected_btn(osk), LV_BTNMATRIX_CTRL_CHECKED);
}

void close_osk(lv_obj_t *osk, lv_group_t *ui, lv_obj_t *entry, lv_obj_t *panel) {
    play_sound(SND_BACK);

    key_show = 0;
    key_map = OSK_MAP_LOWER;
    shift_once = false;

    if (key_entry && lv_obj_is_valid(key_entry)) {
        lv_btnmatrix_set_map(key_entry, key_lower_map);
        apply_action_row_widths(key_entry);
        rebuild_layout(key_entry, &key_layout);
    }

    reset_osk(osk);
    lv_textarea_set_text(entry, "");
    lv_group_set_focus_cb(ui, NULL);
    osk_hide(panel);
}

void osk_show(lv_obj_t *panel) {
    if (!panel || !lv_obj_is_valid(panel)) return;

    lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(panel);
}

void osk_hide(lv_obj_t *panel) {
    if (!panel || !lv_obj_is_valid(panel)) return;

    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *active_osk(void) {
    if (key_show == 2 && num_entry && lv_obj_is_valid(num_entry)) return num_entry;
    return key_entry;
}

static bool is_action_row(const osk_layout_t *L, uint16_t row) {
    if (L->row_count == 0) return false;

    return (row == L->row_count - 1) && (L->row_size[row] <= 3);
}

static uint16_t vert_target(const osk_layout_t *L, uint16_t cur, uint16_t target_row) {
    if (target_row >= L->row_count) return cur;

    uint16_t cur_row = L->row_of[cur];
    uint16_t cur_col = L->col_of[cur];
    uint16_t cur_size = L->row_size[cur_row];
    uint16_t tgt_size = L->row_size[target_row];

    if (cur_size == 0 || tgt_size == 0) return cur;

    bool from_action = is_action_row(L, cur_row);
    bool to_action = is_action_row(L, target_row);

    if (to_action && !from_action) {
        last_letter_btn = (int) cur;

        if (tgt_size == 3) {
            uint16_t scaled = 1;
            if (cur_col == 0) {
                scaled = 0;
            } else if (cur_col + 1 == cur_size) {
                scaled = 2;
            }
            return (uint16_t) (L->row_start[target_row] + scaled);
        }

        if (tgt_size == 2) {
            uint16_t scaled = (cur_col + 1 == cur_size) ? 1 : 0;
            return (uint16_t) (L->row_start[target_row] + scaled);
        }

        return L->row_start[target_row];
    }

    if (from_action && !to_action) {
        if (last_letter_btn >= 0 &&
            (uint16_t) last_letter_btn < L->btn_count &&
            L->row_of[last_letter_btn] == target_row) {
            uint16_t saved = (uint16_t) last_letter_btn;
            last_letter_btn = -1;
            return saved;
        }

        last_letter_btn = -1;

        if (cur_size == 3) {
            uint16_t scaled = (uint16_t) (tgt_size / 2);
            if (cur_col == 0) {
                scaled = 0;
            } else if (cur_col == 2) {
                scaled = (uint16_t) (tgt_size - 1);
            }
            return (uint16_t) (L->row_start[target_row] + scaled);
        }

        if (cur_size == 2) {
            uint16_t scaled = (cur_col == 0) ? 0 : (uint16_t) (tgt_size - 1);
            return (uint16_t) (L->row_start[target_row] + scaled);
        }

        return L->row_start[target_row];
    }

    uint32_t scaled = ((uint32_t) cur_col * (uint32_t) tgt_size + cur_size / 2) / cur_size;
    if (scaled >= tgt_size) scaled = tgt_size - 1;

    return (uint16_t) (L->row_start[target_row] + scaled);
}

void key_up(void) {
    play_sound(SND_NAVIGATE);
    rebuild_layout(active_osk(), &key_layout);

    if (key_layout.btn_count == 0) return;
    if (key_curr < 0 || (uint16_t) key_curr >= key_layout.btn_count) key_curr = 0;

    uint16_t row = key_layout.row_of[key_curr];
    if (row == 0) {
        key_curr = vert_target(&key_layout, (uint16_t) key_curr, (uint16_t) (key_layout.row_count - 1));
    } else {
        key_curr = vert_target(&key_layout, (uint16_t) key_curr, (uint16_t) (row - 1));
    }

    osk_dispatch();
}

void key_down(void) {
    play_sound(SND_NAVIGATE);
    rebuild_layout(active_osk(), &key_layout);

    if (key_layout.btn_count == 0) return;
    if (key_curr < 0 || (uint16_t) key_curr >= key_layout.btn_count) key_curr = 0;

    uint16_t row = key_layout.row_of[key_curr];
    if (row + 1 >= key_layout.row_count) {
        key_curr = vert_target(&key_layout, (uint16_t) key_curr, 0);
    } else {
        key_curr = vert_target(&key_layout, (uint16_t) key_curr, (uint16_t) (row + 1));
    }

    osk_dispatch();
}

void key_left(void) {
    play_sound(SND_NAVIGATE);
    rebuild_layout(active_osk(), &key_layout);

    if (key_layout.btn_count == 0) return;
    if (key_curr < 0 || (uint16_t) key_curr >= key_layout.btn_count) key_curr = 0;

    uint16_t row = key_layout.row_of[key_curr];
    uint16_t row_lo = key_layout.row_start[row];
    uint16_t row_hi = (uint16_t) (row_lo + key_layout.row_size[row] - 1);

    if (is_action_row(&key_layout, row)) last_letter_btn = -1;

    if ((uint16_t) key_curr <= row_lo) {
        key_curr = (int) row_hi;
    } else {
        key_curr = (int) ((uint16_t) key_curr - 1);
    }

    osk_dispatch();
}

void key_right(void) {
    play_sound(SND_NAVIGATE);
    rebuild_layout(active_osk(), &key_layout);

    if (key_layout.btn_count == 0) return;
    if (key_curr < 0 || (uint16_t) key_curr >= key_layout.btn_count) key_curr = 0;

    uint16_t row = key_layout.row_of[key_curr];
    uint16_t row_lo = key_layout.row_start[row];
    uint16_t row_hi = (uint16_t) (row_lo + key_layout.row_size[row] - 1);

    if (is_action_row(&key_layout, row)) last_letter_btn = -1;

    if ((uint16_t) key_curr >= row_hi) {
        key_curr = (int) row_lo;
    } else {
        key_curr = (int) ((uint16_t) key_curr + 1);
    }

    osk_dispatch();
}

static void apply_map_change(int new_map_id) {
    if (!key_entry || !lv_obj_is_valid(key_entry)) return;

    const char **new_map;
    bool one_shot_shift = false;

    switch (new_map_id) {
        case OSK_MAP_UPPER_ONCE:
            new_map = key_upper_map;
            one_shot_shift = true;
            break;
        case OSK_MAP_UPPER_LOCK:
            new_map = key_upper_lock_map;
            break;
        case OSK_MAP_SPECIAL:
            new_map = key_special_map;
            break;
        case OSK_MAP_LOWER:
        default:
            new_map = key_lower_map;
            break;
    }

    key_map = new_map_id;
    shift_once = one_shot_shift;
    last_letter_btn = -1;

    lv_btnmatrix_set_map(key_entry, new_map);
    apply_action_row_widths(key_entry);
    rebuild_layout(key_entry, &key_layout);
    osk_apply_selection(key_entry, (uint16_t) key_curr);
}

void key_swap(void) {
    play_sound(SND_OPTION);
    if (key_show != 1) return;

    switch (key_map) {
        case OSK_MAP_LOWER:
            apply_map_change(OSK_MAP_UPPER_ONCE);
            break;
        case OSK_MAP_UPPER_ONCE:
            apply_map_change(OSK_MAP_UPPER_LOCK);
            break;
        case OSK_MAP_UPPER_LOCK:
            apply_map_change(OSK_MAP_SPECIAL);
            break;
        case OSK_MAP_SPECIAL:
        default:
            apply_map_change(OSK_MAP_LOWER);
            break;
    }
}

void key_swap_back(void) {
    play_sound(SND_OPTION);
    if (key_show != 1) return;

    switch (key_map) {
        case OSK_MAP_LOWER:
            apply_map_change(OSK_MAP_SPECIAL);
            break;
        case OSK_MAP_SPECIAL:
            apply_map_change(OSK_MAP_UPPER_LOCK);
            break;
        case OSK_MAP_UPPER_LOCK:
            apply_map_change(OSK_MAP_UPPER_ONCE);
            break;
        case OSK_MAP_UPPER_ONCE:
        default:
            apply_map_change(OSK_MAP_LOWER);
            break;
    }
}

void key_backspace(lv_obj_t *entry) {
    play_sound(SND_KEYPRESS);
    lv_textarea_del_char(entry);
}

void key_space(lv_obj_t *entry) {
    if (!entry || !lv_obj_is_valid(entry)) return;

    if (osk_max_len > 0) {
        const char *current = lv_textarea_get_text(entry);
        if (current && strlen(current) >= osk_max_len) {
            play_sound(SND_ERROR);
            return;
        }
    }

    play_sound(SND_KEYPRESS);
    lv_textarea_add_char(entry, ' ');
}

void osk_refresh_labels(void) {
    update_nav_hint();
}
