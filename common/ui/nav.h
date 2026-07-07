#pragma once

#include "../options.h"
#include "../init.h"

extern char progress_bar_message[MAX_BUFFER_SIZE];
extern volatile int progress_bar_value;
extern lv_timer_t *timer_update_progress;

enum visual_type { vis_clock, vis_bluetooth, vis_network, vis_battery, vis_headertitle };

enum toast_wait { tst_wait_f, tst_wait_s = 1000, tst_wait_m = 1750, tst_wait_l = 2500 };

struct nav_flag {
    lv_obj_t *element;
    int visible;
};

void show_info_box(const char *title, const char *content, int is_content);

void nav_move(lv_group_t *group, int direction);

void nav_prev(lv_group_t *group, int count);

void nav_next(lv_group_t *group, int count);

void move_option(lv_obj_t *element, int count);

void watermark(lv_obj_t *screen);

void process_visual_element(enum visual_type visual, lv_obj_t *element);

void update_grid_scroll_position(int row_count, int row_height, int current_item_index, lv_obj_t *ui_pnl_grid);

void scroll_object_to_middle(lv_obj_t *container, const lv_obj_t *obj);

void update_scroll_position(
    int mux_item_count, int mux_item_panel, int ui_count_static, int current_item_index, lv_obj_t *ui_pnl_content
);

void update_windowed_list(
    const lv_obj_t *ui_pnl_content, int direction, int current_item_index, int total_count, int visible_count,
    void (*update_item_cb)(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, int index),
    void (*update_items_cb)(int start_index)
);

void list_win_focus_group(int index);

int list_win_focus_index(void);

void list_win_move_index(int direction);

void list_win_update_items(
    int start_index, void (*update_item_cb)(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, int index)
);

void list_win_focus_initial(void (*update_item_cb)(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, int index));

void list_win_nav_move(
    int steps, int direction, void (*update_item_cb)(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, int index)
);

void add_drop_down_options(lv_obj_t *ui_lbl_item_drop_down, char *options[], int count);

void map_drop_down_to_index(lv_obj_t *dropdown, int value, const int *options, int num_options, int def_index);

int map_drop_down_to_value(int selected_index, const int *options, int num_options, int def_value);

void show_progress_bar(char *message);

void update_progress_bar(lv_timer_t *timer);

void hide_progress_bar(void);

void show_bounce_progress_bar(const char *message, int timeout_seconds);

void hide_bounce_progress_bar(void);

void set_nav_flags(const struct nav_flag *nav_flags, size_t count);

void footer_nav_check_scroll(void);

void hide_info_box(void);

int direct_to_previous(lv_obj_t **ui_objects, size_t ui_count_static, int *nav_moved);

void nav_suppress_next_shake(void);

void nav_unsuppress_shake(void);

enum nav_direction { nav_dir_up, nav_dir_down, nav_dir_left, nav_dir_right };

void nav_set_last_dir(enum nav_direction dir);

void nav_focus_shake_cb(const lv_group_t *group);

void nav_play_shake(lv_obj_t *obj, enum nav_direction hint);

void nav_watch_list_overflow(lv_obj_t *panel);
