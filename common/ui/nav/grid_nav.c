#include <stdlib.h>
#include <common/ui/nav/grid_nav.h>
#include <lvgl/lvgl.h>
#include <common/display/theme.h>
#include <common/ui/grid.h>
#include <common/ui/nav/list_nav.h>
#include <common/base/options.h>
#include <common/ui/common.h>

extern int current_item_index;

static void panel_area(lv_obj_t *panel, long *x1, long *y1, long *x2, long *y2) {
    *x1 = lv_obj_get_x(panel);
    *y1 = lv_obj_get_y(panel);
    *x2 = *x1 + lv_obj_get_width(panel);
    *y2 = *y1 + lv_obj_get_height(panel);
}

int grid_spatial_target(const int dx, const int dy) {
    lv_obj_t *from = NULL;
    const uint32_t count = lv_obj_get_child_cnt(ui_pnl_grid);
    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t *panel = lv_obj_get_child(ui_pnl_grid, (int32_t) i);
        if ((int) IFU(lv_obj_get_user_data(panel)) == current_item_index) from = panel;
    }
    if (!from) return -1;

    long fx1, fy1, fx2, fy2;
    panel_area(from, &fx1, &fy1, &fx2, &fy2);
    const long f_along = dx ? (fx1 + fx2) * dx : (fy1 + fy2) * dy;
    const long f_near = dx ? (dx > 0 ? fx2 : -fx1) : (dy > 0 ? fy2 : -fy1);
    const long f_lo = dx ? fy1 : fx1, f_hi = dx ? fy2 : fx2;

    int best = -1, wrap = -1;
    long best_tier = 0, best_score = 0, wrap_along = 0;
    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t *panel = lv_obj_get_child(ui_pnl_grid, (int32_t) i);
        if (panel == from || lv_obj_has_flag(panel, LV_OBJ_FLAG_HIDDEN)) continue;

        const int index = (int) IFU(lv_obj_get_user_data(panel));
        long cx1, cy1, cx2, cy2;
        panel_area(panel, &cx1, &cy1, &cx2, &cy2);
        const long c_along = dx ? (cx1 + cx2) * dx : (cy1 + cy2) * dy;
        const long c_lo = dx ? cy1 : cx1, c_hi = dx ? cy2 : cx2;
        const long overlap = LV_MIN(f_hi, c_hi) - LV_MAX(f_lo, c_lo);

        if (c_along <= f_along + 4) {
            if (dx && overlap > 0 && (wrap < 0 || c_along < wrap_along)) {
                wrap = index;
                wrap_along = c_along;
            }
            continue;
        }

        const long c_near = dx ? (dx > 0 ? cx1 : -cx2) : (dy > 0 ? cy1 : -cy2);
        const long gap = LV_MAX(0, c_near - f_near);
        const long centre = labs((c_lo + c_hi) - (f_lo + f_hi)) / 2;
        const long tier = overlap > 0 ? 0 : 1;
        const long score = tier ? gap + (centre - (f_hi - f_lo) / 2) * 2 : gap * 4 + centre;
        if (best < 0 || tier < best_tier || (tier == best_tier && score < best_score)) {
            best = index;
            best_tier = tier;
            best_score = score;
        }
    }

    if (best >= 0) return best;
    return dx ? wrap : -1;
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
