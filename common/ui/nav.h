#pragma once

#include "../options.h"
#include "../init.h"

extern char progress_bar_message[MAX_BUFFER_SIZE];
extern volatile int progress_bar_value;
extern lv_timer_t *timer_update_progress;

enum visual_type {
    VIS_CLOCK,
    VIS_BLUETOOTH,
    VIS_NETWORK,
    VIS_BATTERY,
    VIS_HEADERTITLE
};

enum toast_show {
    FOREVER,
    SHORT = 1000,
    MEDIUM = 1750,
    LONG = 2500
};

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

void update_grid_scroll_position(int col_count, int row_count, int row_height,
                                 int current_item_index, lv_obj_t *ui_pnlGrid);

void scroll_object_to_middle(lv_obj_t *container, lv_obj_t *obj);

void update_scroll_position(int mux_item_count, int mux_item_panel, int ui_count, int current_item_index,
                            lv_obj_t *ui_pnlContent);

void add_drop_down_options(lv_obj_t *ui_lblItemDropDown, char *options[], int count);

void map_drop_down_to_index(lv_obj_t *dropdown, int value, const int *options, int num_options, int def_index);

int map_drop_down_to_value(int selected_index, const int *options, int num_options, int def_value);

void show_progress_bar(char *message);

void update_progress_bar(lv_timer_t *timer);

void hide_progress_bar(void);

void set_nav_flags(struct nav_flag *nav_flags, size_t count);

void footer_nav_check_scroll(void);

void hide_info_box(void);

int direct_to_previous(lv_obj_t **ui_objects, size_t ui_count, int *nav_moved);
