#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../init.h"
#include "common.h"
#include "grid.h"
#include "../log.h"
#include "../language.h"
#include "../theme.h"
#include "../config.h"
#include "../input/list_nav.h"
#include "../core/common.h"
#include "../../module/muxshare.h"

struct grid_info grid_info;

void init_grid_info(int item_count, int column_count) {
    grid_info.item_count = item_count;
    grid_info.column_count = column_count;

    // Calculate items in the last row
    int last_row_item_count = item_count % column_count;
    if (last_row_item_count == 0 && item_count != 0) {
        last_row_item_count = column_count;  // Full last row if remainder is 0
    }
    grid_info.last_row_item_count = last_row_item_count;

    // Calculate the last row index
    grid_info.last_row_index = (item_count - 1) / column_count;
}

int get_grid_row_index(int current_item_index) {
    return current_item_index / grid_info.column_count;
}

int get_grid_column_index(int current_item_index) {
    return current_item_index % grid_info.column_count;
}

int get_grid_row_item_count(int current_item_index) {
    uint8_t row_index = current_item_index / grid_info.column_count;

    if (row_index == grid_info.last_row_index) {
        return grid_info.last_row_item_count;
    } else {
        return grid_info.column_count;
    }
}

static void set_grid_catalogue_and_program_name(int index, char *catalogue_name, char *program) {
    catalogue_name[0] = '\0';
    program[0] = '\0';

    if ((size_t) index >= item_count) return;

    if (strcmp(mux_module, "muxapp") == 0) {
        snprintf(catalogue_name, MAX_BUFFER_SIZE, "Application");
        snprintf(program, MAX_BUFFER_SIZE, "%s",
                 items[index].glyph_icon && items[index].glyph_icon[0] ? items[index].glyph_icon : "app");
        return;
    }

    if (items[index].content_type == ITEM) {
        if (strcmp(mux_module, "muxcollect") == 0 || strcmp(mux_module, "muxhistory") == 0) {
            char *item_dir = strip_dir(items[index].extra_data);

            char file_path[MAX_BUFFER_SIZE];
            snprintf(file_path, sizeof(file_path), "%s", items[index].extra_data);

            char *item_file = get_last_dir(file_path);

            if (item_dir && item_file && item_file[0]) {
                get_catalogue_name(item_dir, item_file, catalogue_name, MAX_BUFFER_SIZE);

                char *item_no_ext = strip_ext(item_file);
                snprintf(program, MAX_BUFFER_SIZE, "%s", item_no_ext ? item_no_ext : item_file);
                free(item_no_ext);
            }

            free(item_dir);
        } else {
            char sys_dir_copy[MAX_BUFFER_SIZE];
            snprintf(sys_dir_copy, sizeof(sys_dir_copy), "%s", sys_dir);

            get_catalogue_name(sys_dir_copy, items[index].name, catalogue_name, MAX_BUFFER_SIZE);

            char *item_no_ext = strip_ext(items[index].name);
            snprintf(program, MAX_BUFFER_SIZE, "%s", item_no_ext ? item_no_ext : items[index].name);
            free(item_no_ext);
        }

        return;
    }

    snprintf(catalogue_name, MAX_BUFFER_SIZE, "%s",
             strcmp(mux_module, "muxplore") == 0 ? "Folder" : "Collection");

    char *item_no_ext = strip_ext(items[index].name);
    snprintf(program, MAX_BUFFER_SIZE, "%s", item_no_ext ? item_no_ext : items[index].name);
    free(item_no_ext);
}

void update_grid_image_paths(int index) {
    if (index < 0 || (size_t) index >= item_count) return;

    char catalogue_name[MAX_BUFFER_SIZE];
    char program[MAX_BUFFER_SIZE];

    set_grid_catalogue_and_program_name(index, catalogue_name, program);

    if (!catalogue_name[0] || !program[0]) {
        free(items[index].grid_image);
        free(items[index].grid_image_focused);

        items[index].grid_image = strdup("");
        items[index].grid_image_focused = strdup("");

        if (!items[index].grid_image || !items[index].grid_image_focused) {
            LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM);
        }

        return;
    }

    char alt_name[MAX_BUFFER_SIZE];
    alt_name[0] = '\0';

    if (strcmp(mux_module, "muxplore") == 0) {
        const char *rom_cat = get_catalogue_name_from_rom_path(sys_dir, items[index].name);
        if (rom_cat && rom_cat[0]) snprintf(alt_name, sizeof(alt_name), "%s", rom_cat);
    }

    char glyph_name_focused[MAX_BUFFER_SIZE];
    snprintf(glyph_name_focused, sizeof(glyph_name_focused), "%s_focused", program);

    char alt_name_focused[MAX_BUFFER_SIZE];
    alt_name_focused[0] = '\0';

    if (alt_name[0]) {
        snprintf(alt_name_focused, sizeof(alt_name_focused), "%s_focused", alt_name);
    }

    char grid_image[MAX_BUFFER_SIZE];
    char grid_image_focused[MAX_BUFFER_SIZE];

    grid_image[0] = '\0';
    grid_image_focused[0] = '\0';

    if (strcmp(mux_module, "muxapp") == 0) {
        get_app_grid_glyph(items[index].extra_data, program, "default",
                           grid_image, sizeof(grid_image));

        if (!grid_image[0] || !file_exist(grid_image)) {
            load_image_catalogue(catalogue_name, program, alt_name, "default",
                                 mux_dim, "grid", grid_image, sizeof(grid_image));
        }

        get_app_grid_glyph(items[index].extra_data, glyph_name_focused, "default_focused",
                           grid_image_focused, sizeof(grid_image_focused));

        if (!grid_image_focused[0] || !file_exist(grid_image_focused)) {
            load_image_catalogue(catalogue_name, glyph_name_focused, alt_name_focused, "default_focused",
                                 mux_dim, "grid", grid_image_focused, sizeof(grid_image_focused));
        }
    } else {
        load_image_catalogue(catalogue_name, program, alt_name, "default",
                             mux_dim, "grid", grid_image, sizeof(grid_image));

        load_image_catalogue(catalogue_name, glyph_name_focused, alt_name_focused, "default_focused",
                             mux_dim, "grid", grid_image_focused, sizeof(grid_image_focused));
    }

    if ((!grid_image_focused[0] || !file_exist(grid_image_focused)) &&
        grid_image[0] && file_exist(grid_image)) {
        snprintf(grid_image_focused, sizeof(grid_image_focused), "%s", grid_image);
    }

    free(items[index].grid_image);
    free(items[index].grid_image_focused);

    items[index].grid_image = strdup(grid_image);
    items[index].grid_image_focused = strdup(grid_image_focused);

    if (!items[index].grid_image || !items[index].grid_image_focused) LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_ALLOCATE_MEM);
}

static void update_grid_image(lv_obj_t *cell, char *image_path) {
    if (!cell) return;

    if (!image_path || !image_path[0] || !file_exist(image_path)) {
        lv_img_set_src(cell, &ui_img_blank);
        return;
    }

    int16_t grid_glyph = config.SETTINGS.THEMEOPT.GLYPH_SIZE_GRID;
    if (grid_glyph == -2) grid_glyph = theme.GLYPH.GRID;

    int grid_hint_w;
    int grid_hint_h;

    if (grid_glyph < 0) {
        grid_hint_w = grid_hint_h = -1;
    } else if (grid_glyph == 0) {
        grid_hint_w = (theme.GRID.CELL.WIDTH * 3) / 4;
        grid_hint_h = (theme.GRID.CELL.HEIGHT * 3) / 4;
    } else {
        grid_hint_w = grid_hint_h = grid_glyph;
    }

    int grid_px = (grid_glyph > 0) ? grid_glyph : 0;

    char grid_image[MAX_BUFFER_SIZE];
    size_t plen = strlen(image_path);

    if (plen > 4 && strcmp(image_path + plen - 4, ".svg") == 0) {
        snprintf(grid_image, sizeof(grid_image), "M:%s?%dx%d", image_path, grid_hint_w, grid_hint_h);
    } else {
        snprintf(grid_image, sizeof(grid_image), "M:%s", image_path);
    }

    lv_img_set_src(cell, grid_image);
    apply_glyph_scale(cell, grid_image, grid_px, grid_px);
}

static void update_grid_item(lv_obj_t *ui_pnlItem, int index) {
    if (!ui_pnlItem) return;

    lv_obj_set_user_data(ui_pnlItem, UFI(index));

    lv_obj_t *cell_image = lv_obj_get_child(ui_pnlItem, 0);
    lv_obj_t *ui_lblItem = lv_obj_get_child(ui_pnlItem, 1);
    lv_obj_t *cell_image_focused = lv_obj_get_child(ui_pnlItem, 2);

    if (index < 0 || (size_t) index >= item_count) {
        lv_obj_add_flag(ui_pnlItem, LV_OBJ_FLAG_HIDDEN);

        if (ui_lblItem) lv_obj_add_flag(ui_lblItem, LV_OBJ_FLAG_HIDDEN);
        if (cell_image) lv_obj_add_flag(cell_image, LV_OBJ_FLAG_HIDDEN);
        if (cell_image_focused) lv_obj_add_flag(cell_image_focused, LV_OBJ_FLAG_HIDDEN);

        return;
    }

    if (ui_lblItem && strcmp(lv_label_get_text(ui_lblItem), items[index].display_name) != 0) lv_label_set_text(ui_lblItem, items[index].display_name);
    if (ui_lblItem && items[index].glyph_icon) lv_obj_set_user_data(ui_lblItem, items[index].glyph_icon);

    if (!items[index].grid_image || !items[index].grid_image[0] ||
        !items[index].grid_image_focused || !items[index].grid_image_focused[0]) {
        update_grid_image_paths(index);
    }

    update_grid_image(cell_image, items[index].grid_image);
    update_grid_image(cell_image_focused, items[index].grid_image_focused);

    lv_obj_clear_flag(ui_pnlItem, LV_OBJ_FLAG_HIDDEN);

    if (ui_lblItem) lv_obj_clear_flag(ui_lblItem, LV_OBJ_FLAG_HIDDEN);
    if (cell_image) lv_obj_clear_flag(cell_image, LV_OBJ_FLAG_HIDDEN);
    if (cell_image_focused) lv_obj_clear_flag(cell_image_focused, LV_OBJ_FLAG_HIDDEN);

    if (current_item_index == index) {
        lv_group_focus_obj(ui_pnlItem);

        if (!is_carousel_grid_mode()) {
            if (ui_lblItem) lv_group_focus_obj(ui_lblItem);
            if (cell_image) lv_group_focus_obj(cell_image);
        }
    }
}

void update_grid_items(int direction) {
    if (is_carousel_grid_mode()) {
        int carousel_item_count = (theme.GRID.COLUMN_COUNT > theme.GRID.ROW_COUNT)
                                  ? theme.GRID.COLUMN_COUNT : theme.GRID.ROW_COUNT;

        for (int i = 0; i < carousel_item_count; i++) {
            int offset = i - (carousel_item_count / 2);

            size_t raw_index = ((size_t) current_item_index + offset + (size_t) item_count) % (size_t) item_count;
            int index = (int) raw_index;

            lv_obj_t *panel_item = lv_obj_get_child(ui_pnlGrid, i);
            if (!panel_item) continue;

            update_grid_item(panel_item, index);
        }
        return;
    }

    int cols = theme.GRID.COLUMN_COUNT;
    int rows = theme.GRID.ROW_COUNT;
    int total_visible = cols * rows;
    if (total_visible <= 0) return;

    int row = get_grid_row_index(current_item_index);

    int max_start_row = ((int) item_count - 1) / cols - rows + 1;
    if (max_start_row < 0) max_start_row = 0;

    int start_row_index;
    if (direction == 0) {
        start_row_index = row - rows / 2;
    } else if (direction < 0) {
        start_row_index = row;
    } else {
        start_row_index = row - rows + 1;
    }

    if (start_row_index < 0) start_row_index = 0;
    if (start_row_index > max_start_row) start_row_index = max_start_row;

    int start_index = start_row_index * cols;

    for (int index = 0; index < total_visible; index++) {
        lv_obj_t *panel_item = lv_obj_get_child(ui_pnlGrid, index);
        if (!panel_item) continue;

        update_grid_item(panel_item, start_index + index);
    }
}

static int get_grid_item_index(int index) {
    lv_obj_t *panel_item = lv_obj_get_child(ui_pnlGrid, index);
    int item_index = IFU(lv_obj_get_user_data(panel_item));
    return item_index;
}

void update_grid(int direction) {
    if (is_carousel_grid_mode()) {
        update_grid_items(1);
        return;
    }

    int row_pitch = theme.GRID.ROW_HEIGHT + theme.GRID.ROW_PADDING;

    if (item_count <= theme.GRID.COLUMN_COUNT * theme.GRID.ROW_COUNT) {
        int virtual_row = current_item_index / theme.GRID.COLUMN_COUNT;
        update_grid_scroll_position(theme.GRID.COLUMN_COUNT, theme.GRID.ROW_COUNT,
                                    row_pitch, virtual_row * theme.GRID.COLUMN_COUNT, ui_pnlGrid);
        return;
    }

    int grid_start_index = get_grid_item_index(0);
    int grid_end_index = get_grid_item_index(theme.GRID.COLUMN_COUNT * theme.GRID.ROW_COUNT - 1);

    if (direction < 0) {
        if (current_item_index == item_count - 1) {
            update_grid_items(1);
        } else {
            if (current_item_index < grid_start_index) {
                update_grid_items(direction);
            }
        }
    } else {
        if (current_item_index == 0) {
            update_grid_items(-1);
        } else {
            if (current_item_index > grid_end_index) {
                update_grid_items(direction);
            }
        }
    }

    int start_item = get_grid_item_index(0);
    int virtual_row = (current_item_index - start_item) / theme.GRID.COLUMN_COUNT;

    update_grid_scroll_position(theme.GRID.COLUMN_COUNT, theme.GRID.ROW_COUNT, row_pitch, virtual_row * theme.GRID.COLUMN_COUNT, ui_pnlGrid);
}

static void gen_grid_item_common(int item_index, int panel_index, int focus_index) {
    if (item_index < 0 || (size_t) item_index >= item_count) return;

    if (!items[item_index].grid_image || !items[item_index].grid_image[0] ||
        !items[item_index].grid_image_focused || !items[item_index].grid_image_focused[0]) {
        update_grid_image_paths(item_index);
    }

    uint8_t col;
    uint8_t row;

    if (is_carousel_grid_mode()) {
        col = (theme.GRID.COLUMN_COUNT == 1) ? 0 : (uint8_t) panel_index;
        row = (theme.GRID.ROW_COUNT == 1) ? 0 : (uint8_t) panel_index;
    } else {
        col = (uint8_t) (item_index % theme.GRID.COLUMN_COUNT);
        row = (uint8_t) (item_index / theme.GRID.COLUMN_COUNT);
    }

    lv_obj_t *cell_panel = lv_obj_create(ui_pnlGrid);
    if (!cell_panel) return;

    lv_obj_set_user_data(cell_panel, UFI(item_index));

    lv_obj_t *cell_image = lv_img_create(cell_panel);
    lv_obj_t *cell_label = lv_label_create(cell_panel);

    if (cell_label && items[item_index].glyph_icon) lv_obj_set_user_data(cell_label, items[item_index].glyph_icon);

    create_grid_item(&theme, cell_panel, cell_label, cell_image, col, row,
                     items[item_index].grid_image,
                     items[item_index].grid_image_focused,
                     items[item_index].display_name);

    if (cell_label) lv_group_add_obj(ui_group, cell_label);
    if (cell_image) lv_group_add_obj(ui_group_glyph, cell_image);

    lv_group_add_obj(ui_group_panel, cell_panel);

    if (is_carousel_grid_mode() && focus_index == panel_index) {
        lv_group_focus_obj(cell_panel);

        if (cell_label) lv_group_focus_obj(cell_label);
        if (cell_image) lv_group_focus_obj(cell_image);
    }
}

void gen_grid_item(int item_index) {
    gen_grid_item_common(item_index, 0, -1);
}

static void gen_carousel_grid_item(int item_index, int panel_index, int focus_index) {
    gen_grid_item_common(item_index, panel_index, focus_index);
}

int disable_grid_file_exists(char *item_curr_dir) {
    char no_grid_path[PATH_MAX];
    snprintf(no_grid_path, sizeof(no_grid_path), "%s/.nogrid", item_curr_dir);

    return file_exist(no_grid_path);
}

void create_carousel_grid(void) {
    if (item_count == 0) return;

    int carousel_item_count = theme.GRID.COLUMN_COUNT > theme.GRID.ROW_COUNT ? theme.GRID.COLUMN_COUNT : theme.GRID.ROW_COUNT;
    if (carousel_item_count <= 0) return;

    int middle_index = carousel_item_count / 2;

    for (int i = 0; i < carousel_item_count; i++) {
        int offset = i - middle_index;
        int item_index = (current_item_index + offset + (int) item_count) % (int) item_count;

        gen_carousel_grid_item(item_index, i, middle_index);
    }
}

int is_carousel_grid_mode(void) {
    int carousel_item_count = (theme.GRID.COLUMN_COUNT > theme.GRID.ROW_COUNT) ? theme.GRID.COLUMN_COUNT
                                                                               : theme.GRID.ROW_COUNT;
    return (grid_mode_enabled && (theme.GRID.ROW_COUNT == 1 || theme.GRID.COLUMN_COUNT == 1) &&
            carousel_item_count > 2 && carousel_item_count % 2 == 1) ? 1 : 0;
}
