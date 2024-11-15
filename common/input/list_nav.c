#include "list_nav.h"

#include <sys/param.h>

#include "../common.h"
#include "../theme.h"

void list_nav_prev(int steps);

void list_nav_next(int steps);

extern int ui_count;
extern int current_item_index;
int grid_mode_enabled = 0;

void handle_list_nav_up(void) {
    if (msgbox_active || !ui_count) return;

    // Grid mode.  Wrap on Row.
    if (grid_mode_enabled &&
        theme.GRID.NAVIGATION_TYPE == 4 && get_grid_column_index(current_item_index) == 0) {
        list_nav_next(get_grid_row_index(current_item_index) == grid_info.last_row_index ?
                      grid_info.last_row_item_count - 1 : grid_info.column_count - 1);
        // Regular Navigation
    } else {
        list_nav_prev(1);
    }
}

void handle_list_nav_down(void) {
    if (msgbox_active || !ui_count) return;

    // Grid Navigation.  Wrap on Row.
    if (grid_mode_enabled && theme.GRID.NAVIGATION_TYPE == 4 &&
        get_grid_column_index(current_item_index) == get_grid_row_item_count(current_item_index) - 1) {
        list_nav_prev(get_grid_row_item_count(current_item_index) - 1);
        // Regular Navigation
    } else {
        list_nav_next(1);
    }
}

void handle_list_nav_up_hold(void) {
    if (msgbox_active || !ui_count) return;

    // Don't wrap around when scrolling on hold.
    if ((grid_mode_enabled && theme.GRID.NAVIGATION_TYPE == 4 && get_grid_column_index(current_item_index) > 0) ||
        (grid_mode_enabled && theme.GRID.NAVIGATION_TYPE < 4 && current_item_index > 0) ||
        (!grid_mode_enabled && current_item_index > 0)) {
        handle_list_nav_up();
    }
}

void handle_list_nav_down_hold(void) {
    if (msgbox_active || !ui_count) return;

    // Don't wrap around when scrolling on hold.
    if ((grid_mode_enabled && theme.GRID.NAVIGATION_TYPE == 4 &&
         get_grid_column_index(current_item_index) < get_grid_row_item_count(current_item_index) - 1) ||
        (grid_mode_enabled && theme.GRID.NAVIGATION_TYPE < 4 && current_item_index < ui_count - 1) ||
        (!grid_mode_enabled && current_item_index < ui_count - 1)) {
        handle_list_nav_down();
    }
}

void handle_list_nav_left() {
    if (msgbox_active) return;

    // Horizontal Navigation with 2 rows of 4 items
    if (grid_mode_enabled &&
        (theme.GRID.NAVIGATION_TYPE == 2 || theme.GRID.NAVIGATION_TYPE == 4)) {
        int column_index = current_item_index % grid_info.column_count;

        if (current_item_index < grid_info.column_count) {
            list_nav_prev(LV_MAX(column_index + 1, grid_info.last_row_item_count));
        } else {
            list_nav_prev(grid_info.column_count);
        }
    }
}

void handle_list_nav_right() {
    if (msgbox_active) return;

    // Horizontal Navigation with 2 rows of 4 items
    if (grid_mode_enabled &&
        (theme.GRID.NAVIGATION_TYPE == 2 || theme.GRID.NAVIGATION_TYPE == 4)) {
        uint8_t row_index = current_item_index / grid_info.column_count;

        //when on 2nd to last row do not go past last item
        if (row_index == grid_info.last_row_index - 1) {
            int new_item_index = LV_MIN(current_item_index + grid_info.column_count, ui_count - 1);
            list_nav_next(new_item_index - current_item_index);
            //when on the last row only move based on amount of items in that row
        } else if (row_index == grid_info.last_row_index) {
            list_nav_next(grid_info.last_row_item_count);
        } else {
            list_nav_next(grid_info.column_count);
        }
    }
}

void handle_list_nav_left_hold(void) {
    if (msgbox_active) return;

    // Don't wrap around when scrolling on hold.
    if (grid_mode_enabled && (theme.GRID.NAVIGATION_TYPE == 2 || theme.GRID.NAVIGATION_TYPE == 4) &&
        get_grid_row_index(current_item_index) > 0) {
        handle_list_nav_left();
    }
}

void handle_list_nav_right_hold(void) {
    if (msgbox_active) return;

    // Don't wrap around when scrolling on hold.
    if (grid_mode_enabled && (theme.GRID.NAVIGATION_TYPE == 2 || theme.GRID.NAVIGATION_TYPE == 4) &&
        get_grid_row_index(current_item_index) < grid_info.last_row_index) {
        handle_list_nav_right();
    }
}

void handle_list_nav_page_up(void) {
    if (msgbox_active || !ui_count) return;

    // Don't wrap around when scrolling by page.
    int steps;
    if (grid_mode_enabled) {
        steps = LV_MIN(theme.GRID.ROW_COUNT * theme.GRID.COLUMN_COUNT, current_item_index);
    } else {
        steps = LV_MIN(theme.MUX.ITEM.COUNT, current_item_index);
    }
    if (steps > 0) {
        list_nav_prev(steps);
    }
}

void handle_list_nav_page_down(void) {
    if (msgbox_active || !ui_count) return;

    // Don't wrap around when scrolling by page.
    int steps;
    if (grid_mode_enabled) {
        steps = LV_MIN(theme.GRID.ROW_COUNT * theme.GRID.COLUMN_COUNT, ui_count - current_item_index - 1);
    } else {
        steps = LV_MIN(theme.MUX.ITEM.COUNT, ui_count - current_item_index - 1);
    }
    if (steps > 0) {
        list_nav_next(steps);
    }
}
