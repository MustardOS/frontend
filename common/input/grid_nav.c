#include "grid_nav.h"
#include "../../lvgl/lvgl.h"
#include "../theme.h"
#include "../ui/grid.h"
#include "list_nav.h"

extern int current_item_index;

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
