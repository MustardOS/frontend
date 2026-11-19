#include <stdlib.h>
#include <string.h>
#include <common/runtime/init.h>
#include <common/ui/common.h>
#include <common/ui/grid.h>
#include <common/ui/glyph.h>
#include <common/ui/font.h>
#include <common/display/theme.h>
#include <common/config/config.h>
#include <common/storage/fileio.h>
#include <common/ui/nav/content_layout.h>
#include <common/ui/nav/content_strip.h>
#include <module/muxshare.h>

#define STRIP_PAD 4

static content_item *strip_items;
static size_t strip_count;
static int *strip_saved;
static int *list_saved;
static size_t list_saved_count;

static int strip_index;
static int strip_on;
static int strip_carousel;
static int strip_column;
static int strip_visible;
static int strip_first;
static int strip_height;

static lv_obj_t *strip_panel;
static lv_coord_t *strip_columns;
static lv_coord_t strip_rows[2];

static int base_saved;
static int16_t base_count;
static int16_t base_height;
static lv_coord_t base_y;

static void strip_deleted(lv_event_t *e) {
    LV_UNUSED(e);
    strip_panel = NULL;
}

void content_strip_begin(void) {
    strip_panel = NULL;
    base_saved = 0;
    content_strip_clear();

    base_count = theme.mux.item.count;
    base_height = theme.misc.content.height;
    base_y = lv_obj_get_style_y(ui_pnl_content, LV_PART_MAIN);
    base_saved = 1;
}

void content_strip_clear(void) {
    if (strip_panel) lv_obj_del(strip_panel);
    strip_panel = NULL;

    free_items(&strip_items, &strip_count);
    free(strip_saved);
    free(list_saved);
    free(strip_columns);
    strip_saved = NULL;
    list_saved = NULL;
    strip_columns = NULL;
    list_saved_count = 0;
    strip_index = 0;
    strip_on = 0;
    strip_first = 0;
    strip_height = 0;

    if (!base_saved) return;

    theme.mux.item.count = base_count;
    theme.misc.content.height = base_height;
    lv_obj_set_y(ui_pnl_content, base_y);
    lv_obj_set_height(ui_pnl_content, base_height);
}

void content_strip_end(void) {
    content_strip_clear();
    base_saved = 0;
}

int content_strip_shown(void) {
    return strip_panel != NULL;
}

int content_strip_active(void) {
    return strip_panel && strip_on;
}

int content_strip_height(void) {
    return strip_panel ? strip_height : 0;
}

int content_strip_position(int *count) {
    if (count) *count = (int) strip_count;
    return strip_index;
}

const content_item *content_strip_selected(void) {
    return strip_panel ? &strip_items[strip_index] : NULL;
}

int content_strip_saved_index(const int list_index) {
    if (content_strip_active()) return strip_saved[strip_index];
    if (list_saved && list_index >= 0 && (size_t) list_index < list_saved_count) return list_saved[list_index];

    return list_index;
}

static lv_obj_t *strip_cell(const int index) {
    return strip_panel ? lv_obj_get_child(strip_panel, index) : NULL;
}

static void cell_state(const int index, const int focused) {
    lv_obj_t *cell = strip_cell(index);
    if (!cell) return;

    lv_obj_t *parts[] = {cell, lv_obj_get_child(cell, 0), lv_obj_get_child(cell, 1)};
    for (size_t i = 0; i < A_SIZE(parts); i++) {
        if (!parts[i]) continue;
        if (focused) {
            lv_obj_add_state(parts[i], LV_STATE_FOCUSED);
        } else {
            lv_obj_clear_state(parts[i], LV_STATE_FOCUSED);
        }
    }

    lv_event_send(cell, focused ? LV_EVENT_FOCUSED : LV_EVENT_DEFOCUSED, NULL);
}

static void list_state(const int focused) {
    lv_obj_t *parts[] = {
        lv_group_get_focused(ui_group), lv_group_get_focused(ui_group_glyph), lv_group_get_focused(ui_group_panel)
    };

    for (size_t i = 0; i < A_SIZE(parts); i++) {
        if (!parts[i]) continue;
        if (focused) {
            lv_obj_add_state(parts[i], LV_STATE_FOCUSED);
        } else {
            lv_obj_clear_state(parts[i], LV_STATE_FOCUSED);
        }
    }
}

static void strip_fade(void) {
    if (!strip_carousel) return;

    for (size_t i = 0; i < strip_count; i++) {
        const int distance = abs((int) i - strip_index);
        const int opacity = LV_MAX(255 - distance * 80, 60);
        lv_obj_set_style_opa(strip_cell((int) i), (lv_opa_t) opacity, MU_OBJ_MAIN_DEFAULT);
    }
}

static void strip_scroll(const lv_anim_enable_t anim) {
    lv_obj_t *cell = strip_cell(strip_index);
    if (!cell) return;

    if (!strip_carousel) {
        if (strip_index < strip_first) strip_first = strip_index;
        if (strip_index >= strip_first + strip_visible) strip_first = strip_index - strip_visible + 1;
    }

    lv_obj_scroll_to_x(strip_panel, (strip_carousel ? strip_index : strip_first) * strip_column, anim);

    strip_fade();
}

void content_strip_sync(void) {
    if (strip_panel) list_state(!strip_on);
}

void content_strip_focus(const int active) {
    if (!strip_panel || strip_on == active) return;

    strip_on = active;
    cell_state(strip_index, active);
    list_state(!active);

    lv_obj_t *label = lv_group_get_focused(ui_group);
    if (active) {
        apply_text_long_dot(&theme, label);
    } else {
        set_label_long_mode(&theme, label, config.visual.name_scroll);
    }
    nav_moved = 1;
}

int content_strip_move(const int direction) {
    if (!content_strip_active()) return 0;

    const int target = strip_index + direction;
    if (target < 0 || (size_t) target >= strip_count) return 0;

    cell_state(strip_index, 0);
    strip_index = target;
    cell_state(strip_index, 1);
    strip_scroll(LV_ANIM_ON);

    play_sound(snd_navigate);
    nav_moved = 1;
    return 1;
}

int content_strip_nav(const int vertical, const int direction) {
    if (msgbox_active || !strip_panel) return 0;

    if (strip_on) {
        if (!vertical) {
            content_strip_move(direction);
        } else if (direction > 0 && ui_count_static) {
            play_sound(snd_navigate);
            content_strip_focus(0);
        }
        return 1;
    }

    if (vertical && direction < 0 && current_item_index == 0) {
        play_sound(snd_navigate);
        content_strip_focus(1);
        return 1;
    }

    return 0;
}

static void strip_build(const char *module, const content_layout_config *layout) {
    const int width = device.mux.width;
    const int columns = strip_carousel ? LV_CLAMP(3, layout->strip_columns | 1, 7) : layout->strip_columns;

    strip_column = width / columns;
    const int limit = base_height * 45 / 100 - STRIP_PAD * 2;
    const int cell = LV_MIN(strip_column * layout->strip_size / 100, limit);
    const int height = cell + STRIP_PAD * 2;
    strip_visible = columns;
    strip_height = height + theme.mux.item.spacing;

    struct theme_config cell_theme = theme;
    cell_theme.grid.cell.width = (int16_t) cell;
    cell_theme.grid.cell.height = (int16_t) cell;
    cell_theme.grid.cell.radius = (int16_t) (cell / 10);
    cell_theme.grid.cell.column_align = LV_GRID_ALIGN_CENTER;
    cell_theme.grid.cell.row_align = LV_GRID_ALIGN_CENTER;
    cell_theme.grid.cell.text_padding_bottom = (int16_t) (cell / 16);
    cell_theme.grid.cell.text_padding_side = (int16_t) (cell / 16);

    const int named = cell_theme.grid.cell_default.text_alpha > 0;
    cell_theme.grid.cell.image_padding_top = (int16_t) (named ? cell / 10 : 0);
    cell_theme.glyph.grid = (int16_t) (named ? cell / 2 : cell * 3 / 5);

    strip_columns = malloc((strip_count + 1) * sizeof(*strip_columns));
    if (!strip_columns) return;

    for (size_t i = 0; i < strip_count; i++)
        strip_columns[i] = (lv_coord_t) strip_column;
    strip_columns[strip_count] = LV_GRID_TEMPLATE_LAST;
    strip_rows[0] = (lv_coord_t) height;
    strip_rows[1] = LV_GRID_TEMPLATE_LAST;

    strip_panel = lv_obj_create(ui_screen);
    lv_obj_remove_style_all(strip_panel);
    lv_obj_set_size(strip_panel, width, height);
    lv_obj_set_pos(strip_panel, 0, base_y);
    lv_obj_set_layout(strip_panel, LV_LAYOUT_GRID);
    lv_obj_set_style_grid_column_dsc_array(strip_panel, strip_columns, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_grid_row_dsc_array(strip_panel, strip_rows, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_scroll_dir(strip_panel, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(strip_panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(strip_panel, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_add_event_cb(strip_panel, strip_deleted, LV_EVENT_DELETE, NULL);

    if (strip_carousel) {
        const lv_coord_t side = (lv_coord_t) ((width - strip_column) / 2);
        lv_obj_set_style_pad_left(strip_panel, side, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_right(strip_panel, side, MU_OBJ_MAIN_DEFAULT);
    } else {
        lv_obj_set_style_pad_left(
            strip_panel, (lv_coord_t) ((width - strip_column * columns) / 2), MU_OBJ_MAIN_DEFAULT
        );
    }

    load_font_section(FONT_PANEL_DIR, strip_panel);

    char glyph[MAX_BUFFER_SIZE];
    const char *fallback = get_glyph_file(module, "folder", glyph, sizeof(glyph)) ? glyph : "";

    for (size_t i = 0; i < strip_count; i++) {
        const content_item *item = &strip_items[i];
        const int art = item->grid_image && item->grid_image[0] && file_exist(item->grid_image);
        const int art_focused =
            item->grid_image_focused && item->grid_image_focused[0] && file_exist(item->grid_image_focused);

        lv_obj_t *cell_panel = lv_obj_create(strip_panel);
        lv_obj_t *cell_image = lv_img_create(cell_panel);
        lv_obj_t *cell_label = lv_label_create(cell_panel);

        create_grid_item(
            &cell_theme, cell_panel, cell_label, cell_image, (int16_t) i, 0, art ? item->grid_image : fallback,
            art_focused ? item->grid_image_focused : fallback, item->display_name
        );
    }

    for (size_t i = 0; i < strip_count; i++)
        cell_state((int) i, strip_on && (int) i == strip_index);

    const int list_y = base_y + height + theme.mux.item.spacing;
    const int list_height = base_height - height - theme.mux.item.spacing;
    const int panel = theme.mux.item.panel > 0 ? theme.mux.item.panel : 1;

    theme.mux.item.count = (int16_t) LV_MAX(1, list_height / panel);
    theme.misc.content.height = (int16_t) (theme.mux.item.count * panel);
    lv_obj_set_y(ui_pnl_content, (lv_coord_t) list_y);
    lv_obj_set_height(ui_pnl_content, theme.misc.content.height);

    lv_obj_update_layout(strip_panel);
    strip_scroll(LV_ANIM_OFF);
}

int content_strip_split(const char *module, int *index) {
    content_strip_clear();

    if (!content_layout_mixed(&config) || !base_saved) return 0;

    size_t folders = 0;
    for (size_t i = 0; i < item_count; i++) {
        if (items[i].content_type == content_type_folder) folders++;
    }
    if (!folders || folders == item_count) return 0;

    for (size_t i = 0; i < item_count; i++) {
        if (items[i].content_type == content_type_folder) update_grid_image_paths((int) i);
    }

    strip_items = malloc(folders * sizeof(*strip_items));
    strip_saved = malloc(folders * sizeof(*strip_saved));
    list_saved = malloc((item_count - folders) * sizeof(*list_saved));
    if (!strip_items || !strip_saved || !list_saved) {
        free(strip_items);
        strip_items = NULL;
        content_strip_clear();
        return 0;
    }

    const int wanted = index ? *index : -1;
    int remapped = 0;
    size_t kept = 0;

    for (size_t i = 0; i < item_count; i++) {
        if (items[i].content_type == content_type_folder) {
            if ((int) i == wanted) {
                strip_index = (int) strip_count;
                strip_on = 1;
            }
            strip_saved[strip_count] = (int) i;
            strip_items[strip_count++] = items[i];
            continue;
        }

        if ((int) i == wanted) remapped = (int) kept;
        list_saved[kept] = (int) i;
        items[kept++] = items[i];
    }

    item_count = kept;
    list_saved_count = kept;
    if (index && wanted >= 0) *index = remapped;

    const content_layout_config *layout = content_layout_current(&config);
    strip_carousel = layout->style == content_style_carousel_list;
    strip_build(module, layout);

    return 1;
}
