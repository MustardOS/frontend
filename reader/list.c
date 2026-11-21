#include <stdio.h>
#include <string.h>
#include <common/display/theme.h>
#include <common/runtime/init.h>
#include <common/ui/common.h>
#include <module/muxshare.h>
#include "list.h"

#define LIST_ROW_MAX 24

typedef struct {
    lv_obj_t *panel;
    lv_obj_t *label;
    lv_obj_t *glyph;
    lv_obj_t *value;
} list_row;

static list_row rows[LIST_ROW_MAX];
static int row_count = 0;
static lv_obj_t *dim = NULL;
static int visible = 0;
static int total = 0;
static int selected = 0;
static int window_start = 0;
static list_text_fn text_fn = NULL;

static int window_rows(void) {
    int count = theme.mux.item.count > 0 ? theme.mux.item.count : 8;
    if (count > LIST_ROW_MAX) count = LIST_ROW_MAX;
    return count;
}

void list_init(void) {
    dim = lv_obj_create(ui_screen);
    lv_obj_remove_style_all(dim);
    lv_obj_set_size(dim, device.mux.width, device.mux.height);
    lv_obj_set_style_bg_color(dim, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dim, LV_OPA_90, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(dim, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(dim, LV_OBJ_FLAG_HIDDEN);

    row_count = window_rows();
    for (int i = 0; i < row_count; i++) {
        rows[i].panel = lv_obj_create(ui_pnl_content);
        lv_obj_set_width(rows[i].panel, theme.misc.content.width);
        apply_theme_list_panel(rows[i].panel);

        rows[i].label = lv_label_create(rows[i].panel);
        apply_theme_option_item_label(&theme, rows[i].label, "", 1);

        rows[i].glyph = lv_img_create(rows[i].panel);
        apply_theme_list_glyph(&theme, rows[i].glyph, mux_module, "");

        rows[i].value = lv_label_create(rows[i].panel);
        apply_theme_list_value(&theme, rows[i].value, "");

        lv_obj_add_flag(rows[i].panel, LV_OBJ_FLAG_HIDDEN);
    }

    header_and_footer_setup();
    lv_obj_add_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
}

static void set_focus(const list_row *row, const int focused) {
    lv_obj_t *objects[] = {row->panel, row->label, row->glyph, row->value};
    for (size_t i = 0; i < A_SIZE(objects); i++) {
        if (focused)
            lv_obj_add_state(objects[i], LV_STATE_FOCUSED);
        else
            lv_obj_clear_state(objects[i], LV_STATE_FOCUSED);
    }
}

static void place_window(void) {
    if (total <= row_count) {
        window_start = 0;
        return;
    }

    if (selected < window_start) window_start = selected;
    if (selected >= window_start + row_count) window_start = selected - row_count + 1;
    if (window_start > total - row_count) window_start = total - row_count;
    if (window_start < 0) window_start = 0;
}

void list_refresh(void) {
    if (!visible || !text_fn) return;

    place_window();

    for (int i = 0; i < row_count; i++) {
        const int index = window_start + i;
        if (index >= total) {
            lv_obj_add_flag(rows[i].panel, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        char label[MAX_BUFFER_SIZE] = "";
        char value[MAX_BUFFER_SIZE] = "";
        const char *glyph = "";
        text_fn(index, label, sizeof(label), value, sizeof(value), &glyph);

        lv_label_set_text(rows[i].label, label);
        lv_label_set_text(rows[i].value, value);
        if (value[0])
            lv_obj_clear_flag(rows[i].value, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(rows[i].value, LV_OBJ_FLAG_HIDDEN);

        apply_theme_list_glyph(&theme, rows[i].glyph, mux_module, glyph ? glyph : "");

        set_focus(&rows[i], index == selected);
        lv_obj_clear_flag(rows[i].panel, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_invalidate(ui_screen);
}

void list_show(const char *title, const int count, const int initial, const list_text_fn text) {
    total = count;
    selected = initial >= 0 && initial < count ? initial : 0;
    text_fn = text;
    window_start = 0;

    lv_label_set_text(ui_lbl_title, title);

    if (!visible) {
        visible = 1;
        lv_obj_clear_flag(dim, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(dim);
        lv_obj_move_foreground(ui_pnl_header);
        lv_obj_move_foreground(ui_pnl_content);
        lv_obj_move_foreground(ui_pnl_footer);
    }

    list_refresh();
}

void list_hide(void) {
    if (!visible) return;
    visible = 0;

    lv_obj_add_flag(dim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(ui_screen);
}

int list_is_visible(void) {
    return visible;
}

int list_selected(void) {
    return selected;
}

void list_move(const int delta, const int wrap) {
    if (!visible || total <= 0 || delta == 0) return;

    int next = selected + delta;
    if (wrap) {
        next = ((next % total) + total) % total;
    } else {
        if (next < 0) next = 0;
        if (next >= total) next = total - 1;
    }

    selected = next;
    list_refresh();
}
