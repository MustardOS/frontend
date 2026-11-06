#include <stdlib.h>
#include <common/ui/nav/grid_nav.h>
#include <lvgl/lvgl.h>
#include <common/display/theme.h>
#include <common/ui/grid.h>
#include <common/ui/nav/list_nav.h>
#include <common/base/options.h>
#include <common/ui/common.h>

extern int current_item_index;

int grid_spatial_target(const int dx, const int dy) {
    lv_obj_t *from = NULL;
    const uint32_t count = lv_obj_get_child_cnt(ui_pnl_grid);
    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t *panel = lv_obj_get_child(ui_pnl_grid, (int32_t) i);
        if ((int) IFU(lv_obj_get_user_data(panel)) == current_item_index) from = panel;
    }
    if (!from) return -1;

    const lv_coord_t from_x = lv_obj_get_x(from) + lv_obj_get_width(from) / 2;
    const lv_coord_t from_y = lv_obj_get_y(from) + lv_obj_get_height(from) / 2;
    const long row_half = lv_obj_get_height(from) / 2;

    int ahead = -1, near = -1, wrap = -1;
    long ahead_score = 0, near_score = 0, wrap_along = 0;
    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t *panel = lv_obj_get_child(ui_pnl_grid, (int32_t) i);
        if (panel == from) continue;

        const int index = (int) IFU(lv_obj_get_user_data(panel));
        const long vx = lv_obj_get_x(panel) + lv_obj_get_width(panel) / 2 - from_x;
        const long vy = lv_obj_get_y(panel) + lv_obj_get_height(panel) / 2 - from_y;
        const long along = vx * dx + vy * dy;
        const long across = labs(vx * dy - vy * dx);

        if (along <= 0) {
            if (dx && across <= row_half && along < wrap_along) {
                wrap = index;
                wrap_along = along;
            }
            continue;
        }

        const long score = along + 2 * across;
        if (across * 2 <= along && (ahead < 0 || score < ahead_score)) {
            ahead = index;
            ahead_score = score;
        }
        if (near < 0 || score < near_score) {
            near = index;
            near_score = score;
        }
    }

    if (ahead >= 0) return ahead;
    if (dx && wrap >= 0) return wrap;
    return near;
}

int is_horizontal_grid_nav(void) {
    return theme.grid.navigation_type == nav_horizontal || theme.grid.navigation_type == nav_even_rows;
}

int is_horizontal_misc_nav(void) {
    return theme.misc.navigation_type == nav_horizontal || theme.misc.navigation_type == nav_even_rows;
}

int is_split_row_misc_nav(void) {
    return theme.misc.navigation_type == nav_split_rows || theme.misc.navigation_type == nav_split_rows_full;
}

int grid_row_hold_left_ok(void) {
    return grid_mode_enabled && is_horizontal_grid_nav()
           && (get_grid_row_index(current_item_index) > 0 || is_carousel_grid_mode());
}

int grid_row_hold_right_ok(void) {
    return grid_mode_enabled && is_horizontal_grid_nav()
           && (get_grid_row_index(current_item_index) < grid_info.last_row_index || is_carousel_grid_mode());
}

int grid_row_wrap_up_steps(void) {
    if (theme.grid.navigation_type != nav_even_rows || get_grid_column_index(current_item_index)) return -1;

    return get_grid_row_index(current_item_index) == grid_info.last_row_index ? grid_info.last_row_item_count - 1
                                                                              : grid_info.column_count - 1;
}

int grid_row_wrap_down_steps(void) {
    if (theme.grid.navigation_type != nav_even_rows
        || get_grid_column_index(current_item_index) != get_grid_row_item_count(current_item_index) - 1) {
        return -1;
    }

    return get_grid_row_item_count(current_item_index) - 1;
}

int grid_row_left_steps(void) {
    const int column_index = current_item_index % grid_info.column_count;

    if (current_item_index < grid_info.column_count) {
        return LV_MAX(column_index + 1, grid_info.last_row_item_count);
    }

    return grid_info.column_count;
}

int grid_row_right_steps(const int item_count) {
    const uint8_t row_index = current_item_index / grid_info.column_count;

    if (row_index == grid_info.last_row_index - 1) {
        const int new_item_index = LV_MIN(current_item_index + grid_info.column_count, item_count - 1);
        return new_item_index - current_item_index;
    }

    if (row_index == grid_info.last_row_index) return grid_info.last_row_item_count;

    return grid_info.column_count;
}

int grid_row_hold_up_ok(void) {
    if (theme.grid.navigation_type == nav_even_rows) return get_grid_column_index(current_item_index) > 0;
    if (theme.grid.navigation_type < nav_even_rows) return current_item_index > 0;

    return 0;
}

int grid_row_hold_down_ok(const int item_count) {
    if (theme.grid.navigation_type == nav_even_rows) {
        return get_grid_column_index(current_item_index) < get_grid_row_item_count(current_item_index) - 1;
    }
    if (theme.grid.navigation_type < nav_even_rows) return current_item_index < item_count - 1;

    return 0;
}

int misc_even_row_wrap_up_steps(const int item_count) {
    if (theme.misc.navigation_type != nav_even_rows) return -1;

    const int row_size = item_count / 2;
    if (current_item_index % row_size != 0) return -1;

    return row_size - 1;
}

int misc_even_row_wrap_down_steps(const int item_count) {
    if (theme.misc.navigation_type != nav_even_rows) return -1;

    const int row_size = item_count / 2;
    if ((current_item_index + 1) % row_size != 0 && current_item_index != item_count - 1) return -1;

    return row_size - 1;
}

int misc_even_row_hold_up_ok(const int item_count) {
    if (theme.misc.navigation_type != nav_even_rows) return 0;

    const int row_size = item_count / 2;
    return current_item_index % row_size != 0;
}

int misc_even_row_hold_down_ok(const int item_count) {
    if (theme.misc.navigation_type != nav_even_rows) return 0;

    const int row_size = item_count / 2;
    return (current_item_index + 1) % row_size != 0 && current_item_index != item_count - 1;
}
