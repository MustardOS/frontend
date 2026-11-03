#pragma once

enum nav_type {
    nav_standard = 0,        // Single row/column.  No row wrap
    nav_swap_axis = 1,       // Same as nav_standard.  With DPAD axis swapped
    nav_horizontal = 2,      // Single horizontal row.  Left/right jump by half the item count
    nav_split_rows = 3,      // Uneven row split.  Left/right jump rows, no up/down wrap
    nav_even_rows = 4,       // Two even rows.  Full up/down wrap plus left/right row jump
    nav_split_rows_full = 5, // Same as nav_split_rows.  Wraps on row for up/down
};

int is_horizontal_grid_nav(void);

int is_horizontal_misc_nav(void);

int is_split_row_misc_nav(void);

int grid_row_wrap_up_steps(void);

int grid_row_wrap_down_steps(void);

int grid_row_left_steps(void);

int grid_row_right_steps(int item_count);

int grid_row_hold_up_ok(void);

int grid_row_hold_down_ok(int item_count);

int misc_even_row_wrap_up_steps(int item_count);

int misc_even_row_wrap_down_steps(int item_count);

int misc_even_row_hold_up_ok(int item_count);

int misc_even_row_hold_down_ok(int item_count);

int grid_row_hold_left_ok(void);

int grid_row_hold_right_ok(void);
