#pragma once

struct grid_info {
    int item_count;
    int last_row_item_count;
    int column_count;
    int last_row_index;
};

extern struct grid_info grid_info;

void init_grid_info(int item_count, int column_count);

int get_grid_row_index(int current_item_index);

int get_grid_column_index(int current_item_index);

int get_grid_row_item_count(int current_item_index);

void update_grid_image_paths(int index);

void update_grid_items(int direction);

void update_grid(int direction);

void gen_grid_item(int item_index);

int disable_grid_file_exists(char *item_curr_dir);

void create_carousel_grid(void);

int is_carousel_grid_mode(void);
