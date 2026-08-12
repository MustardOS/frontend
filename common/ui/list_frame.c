#include <stdio.h>
#include "list_frame.h"
#include "../../module/muxshare.h"
#include "../options.h"
#include "common.h"

// Room for every row a paged screen might hold
#define LIST_FRAME_ROWS_MAX 96

static struct theme_config *theme_ref = NULL;

static list_frame frames[LIST_FRAME_MAX];
static int frame_count = 0;
static int current = 0;

static lv_obj_t **row_panels = NULL;
static lv_obj_t **row_labels = NULL;
static lv_obj_t **row_glyphs = NULL;
static lv_obj_t **row_values = NULL;
static int row_total = 0;

static int suppressed[LIST_FRAME_ROWS_MAX];
static int inert[LIST_FRAME_ROWS_MAX];

static lv_obj_t *bar = NULL;
static lv_obj_t *bar_label = NULL;
static lv_obj_t *bar_glyph = NULL;
static lv_obj_t *bar_glyph_right = NULL;
static lv_obj_t *bar_value = NULL;

void list_frame_reset(void) {
    frame_count = 0;
    current = 0;
    row_total = 0;
    bar = NULL;
    bar_label = NULL;
    bar_glyph = NULL;
    bar_glyph_right = NULL;
    bar_value = NULL;
}

int list_frame_active(void) {
    return frame_count > 0;
}

static int frame_has_rows(const int index) {
    const list_frame *f = &frames[index];

    for (int i = f->first; i < f->first + f->count && i < row_total; i++)
        if (!suppressed[i]) return 1;

    return 0;
}

static void show_row(const int i, const int visible) {
    lv_obj_t *const parts[] = {
        row_panels ? row_panels[i] : NULL, row_labels ? row_labels[i] : NULL, row_glyphs ? row_glyphs[i] : NULL,
        row_values ? row_values[i] : NULL
    };

    for (size_t p = 0; p < sizeof(parts) / sizeof(parts[0]); p++) {
        if (!parts[p]) continue;

        if (visible)
            lv_obj_clear_flag(parts[p], MU_OBJ_FLAG_HIDE_FLOAT);
        else
            lv_obj_add_flag(parts[p], MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static lv_obj_t *build_arrow(lv_obj_t *parent, const char *name) {
    lv_obj_t *img = lv_img_create(parent);

    apply_theme_list_glyph(theme_ref, img, "section", name);

    return img;
}

static void bar_focus_cb(lv_event_t *e) {
    if (!bar_glyph_right || !lv_obj_is_valid(bar_glyph_right)) return;

    if (lv_event_get_code(e) == LV_EVENT_FOCUSED)
        lv_obj_add_state(bar_glyph_right, LV_STATE_FOCUSED);
    else
        lv_obj_clear_state(bar_glyph_right, LV_STATE_FOCUSED);
}

static void build_bar(lv_obj_t *parent) {
    bar = lv_obj_create(parent);

    apply_theme_list_panel(bar);

    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    bar_glyph = build_arrow(bar, "left");
    bar_label = lv_label_create(bar);

    apply_theme_list_item(theme_ref, bar_label, "");

    const lv_coord_t inset = theme_ref->list_default.glyph_padding_left + theme_ref->mux.item.height;

    lv_obj_set_width(bar_label, lv_pct(100));
    lv_obj_set_align(bar_label, LV_ALIGN_CENTER);
    lv_obj_set_style_pad_left(bar_label, inset, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(bar_label, inset, MU_OBJ_MAIN_DEFAULT);
    lv_label_set_long_mode(bar_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(bar_label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(bar_label, "");

    bar_glyph_right = build_arrow(bar, "right");
    lv_obj_set_style_align(bar_glyph_right, LV_ALIGN_RIGHT_MID, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_x(bar_glyph_right, -theme_ref->list_default.glyph_padding_left, MU_OBJ_MAIN_DEFAULT);

    lv_obj_add_event_cb(bar_glyph, bar_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(bar_glyph, bar_focus_cb, LV_EVENT_DEFOCUSED, NULL);

    bar_value = lv_dropdown_create(bar);

    lv_dropdown_clear_options(bar_value);
    lv_obj_add_flag(bar_value, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(bar_value, 0, 0);
    lv_obj_set_style_opa(bar_value, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);

    lv_obj_move_to_index(bar, 0);
}

void list_frame_init(
    struct theme_config *t, lv_obj_t *parent, const list_frame *list, const int count, lv_obj_t **panels,
    lv_obj_t **labels, lv_obj_t **glyphs, lv_obj_t **values, const int total_rows
) {
    list_frame_reset();

    if (!t || !parent || !list || count <= 0 || count > LIST_FRAME_MAX) return;
    if (total_rows <= 0 || total_rows > LIST_FRAME_ROWS_MAX) return;

    theme_ref = t;

    for (int i = 0; i < count; i++)
        frames[i] = list[i];

    frame_count = count;

    row_panels = panels;
    row_labels = labels;
    row_glyphs = glyphs;
    row_values = values;
    row_total = total_rows;

    for (int i = 0; i < total_rows; i++) {
        suppressed[i] = panels && panels[i] && lv_obj_has_flag(panels[i], LV_OBJ_FLAG_HIDDEN);
        inert[i] = 0;
    }

    // Rows are created in whatever order their element lists are declared, which need
    // not be the order they are navigated in, so put the panels into row order
    for (int i = 0; i < total_rows; i++)
        if (panels && panels[i]) lv_obj_move_to_index(panels[i], i);

    build_bar(parent);
}

void list_frame_apply(void) {
    if (!list_frame_active()) return;

    const list_frame *f = &frames[current];

    for (int i = 0; i < row_total; i++) {
        const int in_frame = i >= f->first && i < f->first + f->count;
        show_row(i, in_frame && !suppressed[i]);
    }

    reset_ui_groups();

    ui_count_static = 0;

    if (bar) {
        lv_group_add_obj(ui_group, bar_label);
        lv_group_add_obj(ui_group_value, bar_value);
        lv_group_add_obj(ui_group_glyph, bar_glyph);
        lv_group_add_obj(ui_group_panel, bar);

        ui_count_static++;
    }

    for (int i = f->first; i < f->first + f->count && i < row_total; i++) {
        if (suppressed[i] || inert[i]) continue;

        if (row_labels && row_labels[i]) lv_group_add_obj(ui_group, row_labels[i]);
        if (row_values && row_values[i]) lv_group_add_obj(ui_group_value, row_values[i]);
        if (row_glyphs && row_glyphs[i]) lv_group_add_obj(ui_group_glyph, row_glyphs[i]);
        if (row_panels && row_panels[i]) lv_group_add_obj(ui_group_panel, row_panels[i]);

        ui_count_static++;
    }

    if (bar_label) {
        char text[MAX_BUFFER_SIZE];
        snprintf(text, sizeof(text), "%s  %d / %d", f->label ? f->label : "", current + 1, frame_count);
        lv_label_set_text(bar_label, text);
    }

    if (bar) lv_obj_move_to_index(bar, 0);

    current_item_index = 0;
}

int list_frame_focused(void) {
    return list_frame_active() && current_item_index == 0;
}

void list_frame_help(void) {
    if (!list_frame_active()) return;

    show_info_box(frames[current].label ? frames[current].label : "", lang.generic.section_help, 0);
}

static void marker_path(char *out, const size_t len) {
    snprintf(out, len, "%s%s", MUOS_SFI_LOAD, mux_module);
}

static void marker_write(const int row) {
    char path[MAX_BUFFER_SIZE];
    marker_path(path, sizeof(path));

    char value[64];
    snprintf(value, sizeof(value), "%d\n%d", current, row);

    write_text_to_file(path, "w", CHAR, value);
}

int list_frame_row_of(const lv_obj_t *label) {
    if (!label || !row_labels) return -1;

    for (int i = 0; i < row_total; i++)
        if (row_labels[i] == label) return i;

    return -1;
}

void list_frame_set_inert(const int row, const int value) {
    if (row < 0 || row >= LIST_FRAME_ROWS_MAX) return;

    inert[row] = value ? 1 : 0;
}

void list_frame_remember(const lv_obj_t *label) {
    if (!list_frame_active() || !label || !row_labels) return;
    if (!config.settings.advanced.remember_section) return;

    for (int i = 0; i < row_total; i++) {
        if (row_labels[i] != label) continue;

        marker_write(i);

        return;
    }
}

void list_frame_remember_section(void) {
    if (!list_frame_active()) return;
    if (!config.settings.advanced.remember_section) return;

    marker_write(-1);
}

int list_frame_restore(void) {
    if (!list_frame_active()) return 0;

    char path[MAX_BUFFER_SIZE];
    marker_path(path, sizeof(path));

    if (!file_exist(path)) return 0;

    if (!config.settings.advanced.remember_section) {
        remove(path);
        return 0;
    }

    const int frame = read_line_int_from(path, 1);
    const int row = read_line_int_from(path, 2);

    remove(path);

    if (frame < 0 || frame >= frame_count || !frame_has_rows(frame)) return 0;

    current = frame;
    list_frame_apply();

    if (row < frames[frame].first || row >= frames[frame].first + frames[frame].count) return 0;
    if (row >= row_total || suppressed[row]) return 0;

    int steps = 1;
    for (int i = frames[frame].first; i < row; i++)
        if (!suppressed[i]) steps++;

    return steps;
}

int list_frame_current(void) {
    return list_frame_active() ? current : -1;
}

int list_frame_current_row(void) {
    if (!list_frame_active() || current_item_index <= 0) return -1;

    const list_frame *f = &frames[current];

    int seen = 0;
    for (int i = f->first; i < f->first + f->count && i < row_total; i++) {
        if (suppressed[i] || inert[i]) continue;
        if (++seen == current_item_index) return i;
    }

    return -1;
}

int list_frame_go(const int index) {
    if (!list_frame_active() || index < 0 || index >= frame_count) return 0;
    if (index == current || !frame_has_rows(index)) return 0;

    current = index;
    list_frame_apply();

    return 1;
}

int list_frame_move(const int direction) {
    if (!list_frame_active() || direction == 0) return 0;

    const int step = direction < 0 ? -1 : 1;
    int next = current;

    for (int tried = 0; tried < frame_count; tried++) {
        next += step;

        if (next < 0)
            next = frame_count - 1;
        else if (next >= frame_count)
            next = 0;

        if (next == current) break;
        if (!frame_has_rows(next)) continue;

        current = next;
        list_frame_apply();

        return 1;
    }

    return 0;
}
