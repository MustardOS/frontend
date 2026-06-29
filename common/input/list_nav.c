#include "list_nav.h"
#include "../init.h"
#include "../ui/common.h"
#include "../ui/grid.h"
#include "../ui/nav.h"
#include "../theme.h"

static void (*list_nav_prev_cb)(int) = NULL;

static void (*list_nav_next_cb)(int) = NULL;

extern int ui_count_static;
extern int current_item_index;

int grid_mode_enabled = 0;

void list_nav_set_callbacks(void (*prev)(int), void (*next)(int)) {
    list_nav_prev_cb = prev;
    list_nav_next_cb = next;
}

static int list_nav_wrap_index(const int index) {
    if (ui_count_static <= 0) return 0;

    int wrapped = index % ui_count_static;
    if (wrapped < 0) wrapped += ui_count_static;

    return wrapped;
}

static enum nav_direction list_nav_carousel_direction(const int forward, const enum nav_direction fallback) {
    if (!is_carousel_grid_mode()) return fallback;

    if (theme.grid.row_count == 1) {
        return forward ? nav_dir_right : nav_dir_left;
    }

    if (theme.grid.column_count == 1) {
        return forward ? nav_dir_down : nav_dir_up;
    }

    return fallback;
}

static enum nav_direction
list_nav_grid_direction(const int steps, const int forward, const enum nav_direction fallback) {
    const enum nav_direction direction = list_nav_carousel_direction(forward, fallback);

    if (is_carousel_grid_mode()) return direction;
    if (!grid_mode_enabled || grid_info.column_count <= 0 || ui_count_static < 2) return direction;

    const int from_index = current_item_index;
    const int to_index = list_nav_wrap_index(current_item_index + (forward ? steps : -steps));

    if (from_index == to_index) return direction;

    const int from_row = get_grid_row_index(from_index);
    const int to_row = get_grid_row_index(to_index);
    const int from_col = get_grid_column_index(from_index);
    const int to_col = get_grid_column_index(to_index);

    if (to_row < from_row) return nav_dir_up;
    if (to_row > from_row) return nav_dir_down;
    if (to_col < from_col) return nav_dir_left;
    if (to_col > from_col) return nav_dir_right;

    return direction;
}

static void handle_list_nav_prev_with_dir(const int steps, const enum nav_direction fallback) {
    if (steps <= 0) return;

    nav_set_last_dir(list_nav_grid_direction(steps, 0, fallback));
    nav_unsuppress_shake();

    if (list_nav_prev_cb) list_nav_prev_cb(steps);
}

static void handle_list_nav_next_with_dir(const int steps, const enum nav_direction fallback) {
    if (steps <= 0) return;

    nav_set_last_dir(list_nav_grid_direction(steps, 1, fallback));
    nav_unsuppress_shake();

    if (list_nav_next_cb) list_nav_next_cb(steps);
}

void handle_list_nav_prev(const int steps) {
    handle_list_nav_prev_with_dir(steps, nav_dir_up);
}

void handle_list_nav_next(const int steps) {
    handle_list_nav_next_with_dir(steps, nav_dir_down);
}

void handle_list_nav_up(void) {
    if (msgbox_active) {
        if (!swap_axis) scroll_help_content(1, 0);
        return;
    }

    if (ui_count_static < 2 || block_input) return;

    if (grid_mode_enabled && theme.grid.navigation_type == 4 && get_grid_column_index(current_item_index) == 0) {
        handle_list_nav_next_with_dir(
            get_grid_row_index(current_item_index) == grid_info.last_row_index ? grid_info.last_row_item_count - 1
                                                                               : grid_info.column_count - 1,
            nav_dir_up
        );
    } else {
        handle_list_nav_prev_with_dir(1, nav_dir_up);
    }
}

void handle_list_nav_down(void) {
    if (msgbox_active) {
        if (!swap_axis) scroll_help_content(-1, 0);
        return;
    }

    if (ui_count_static < 2 || block_input) return;

    if (grid_mode_enabled && theme.grid.navigation_type == 4
        && get_grid_column_index(current_item_index) == get_grid_row_item_count(current_item_index) - 1) {
        handle_list_nav_prev_with_dir(get_grid_row_item_count(current_item_index) - 1, nav_dir_down);
    } else {
        handle_list_nav_next_with_dir(1, nav_dir_down);
    }
}

void handle_list_nav_up_hold(void) {
    if (msgbox_active) {
        if (!swap_axis) scroll_help_content(1, 0);
        return;
    }

    if (ui_count_static < 2 || block_input) return;

    if ((grid_mode_enabled && theme.grid.navigation_type == 4 && get_grid_column_index(current_item_index) > 0)
        || (grid_mode_enabled && theme.grid.navigation_type < 4 && current_item_index > 0)
        || (!grid_mode_enabled && current_item_index > 0) || is_carousel_grid_mode()) {
        handle_list_nav_up();
    }
}

void handle_list_nav_down_hold(void) {
    if (msgbox_active) {
        if (!swap_axis) scroll_help_content(-1, 0);
        return;
    }

    if (ui_count_static < 2 || block_input) return;

    if ((grid_mode_enabled && theme.grid.navigation_type == 4
         && get_grid_column_index(current_item_index) < get_grid_row_item_count(current_item_index) - 1)
        || (grid_mode_enabled && theme.grid.navigation_type < 4 && current_item_index < ui_count_static - 1)
        || (!grid_mode_enabled && current_item_index < ui_count_static - 1) || is_carousel_grid_mode()) {
        handle_list_nav_down();
    }
}

void handle_list_nav_left(void) {
    if (msgbox_active) {
        if (swap_axis) scroll_help_content(1, 0);
        return;
    }
    if (block_input) return;

    nav_set_last_dir(nav_dir_left);
    if (is_carousel_grid_mode() && theme.grid.row_count == 1) {
        handle_list_nav_prev(1);
        return;
    }

    // Horizontal Navigation with 2 rows of 4 items
    if (grid_mode_enabled && (theme.grid.navigation_type == 2 || theme.grid.navigation_type == 4)) {
        const int column_index = current_item_index % grid_info.column_count;

        if (current_item_index < grid_info.column_count) {
            handle_list_nav_prev(LV_MAX(column_index + 1, grid_info.last_row_item_count));
        } else {
            handle_list_nav_prev(grid_info.column_count);
        }
    }
}

void handle_list_nav_right(void) {
    if (msgbox_active) {
        if (swap_axis) scroll_help_content(-1, 0);
        return;
    }
    if (block_input) return;

    nav_set_last_dir(nav_dir_right);
    if (is_carousel_grid_mode() && theme.grid.row_count == 1) {
        handle_list_nav_next(1);
        return;
    }

    // Horizontal Navigation with 2 rows of 4 items
    if (grid_mode_enabled && (theme.grid.navigation_type == 2 || theme.grid.navigation_type == 4)) {
        const uint8_t row_index = current_item_index / grid_info.column_count;

        // when on 2nd to last row do not go past last item
        if (row_index == grid_info.last_row_index - 1) {
            const int new_item_index = LV_MIN(current_item_index + grid_info.column_count, ui_count_static - 1);
            handle_list_nav_next(new_item_index - current_item_index);
            // when on the last row only move based on amount of items in that row
        } else if (row_index == grid_info.last_row_index) {
            handle_list_nav_next(grid_info.last_row_item_count);
        } else {
            handle_list_nav_next(grid_info.column_count);
        }
    }
}

void handle_list_nav_left_hold(void) {
    if (msgbox_active) {
        if (swap_axis) scroll_help_content(1, 0);
        return;
    }

    if (block_input) return;

    if ((grid_mode_enabled && (theme.grid.navigation_type == 2 || theme.grid.navigation_type == 4)
         && get_grid_row_index(current_item_index) > 0)
        || (is_carousel_grid_mode() && theme.grid.row_count == 1)) {
        handle_list_nav_left();
    }
}

void handle_list_nav_right_hold(void) {
    if (msgbox_active) {
        if (swap_axis) scroll_help_content(-1, 0);
        return;
    }

    if (block_input) return;

    if ((grid_mode_enabled && (theme.grid.navigation_type == 2 || theme.grid.navigation_type == 4)
         && get_grid_row_index(current_item_index) < grid_info.last_row_index)
        || (is_carousel_grid_mode() && theme.grid.row_count == 1)) {
        handle_list_nav_right();
    }
}

void handle_list_nav_page_up(void) {
    if (msgbox_active) {
        scroll_help_content(1, 1);
        return;
    }

    if (ui_count_static < 2 || block_input || page_nav_blocked) return;

    int steps;
    if (grid_mode_enabled) {
        steps = LV_MIN(theme.grid.row_count * theme.grid.column_count, current_item_index);
    } else {
        steps = LV_MIN(theme.mux.item.count, current_item_index);
    }

    if (steps > 0) {
        handle_list_nav_prev_with_dir(steps, nav_dir_up);
    }
}

void handle_list_nav_page_down(void) {
    if (msgbox_active) {
        scroll_help_content(-1, 1);
        return;
    }

    if (ui_count_static < 2 || block_input || page_nav_blocked) return;

    int steps;
    if (grid_mode_enabled) {
        steps = LV_MIN(theme.grid.row_count * theme.grid.column_count, ui_count_static - current_item_index - 1);
    } else {
        steps = LV_MIN(theme.mux.item.count, ui_count_static - current_item_index - 1);
    }

    if (steps > 0) {
        handle_list_nav_next_with_dir(steps, nav_dir_down);
    }
}
