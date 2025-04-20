#pragma once

extern int grid_mode_enabled;

void handle_list_nav_prev(int steps);
void handle_list_nav_next(int steps);

void list_nav_set_callbacks(void (*prev)(int), void (*next)(int));

void handle_list_nav_up(void);

void handle_list_nav_down(void);

void handle_list_nav_up_hold(void);

void handle_list_nav_down_hold(void);

void handle_list_nav_left();

void handle_list_nav_right();

void handle_list_nav_left_hold(void);

void handle_list_nav_right_hold(void);

void handle_list_nav_page_up(void);

void handle_list_nav_page_down(void);
