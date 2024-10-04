#include "list_nav.h"

#include "../common.h"
#include "../theme.h"

void list_nav_prev(int steps);
void list_nav_next(int steps);

void list_nav_first(void);
void list_nav_last(void);

extern int ui_count;
extern int current_item_index;

void handle_list_nav_up(void) {
    if (msgbox_active || !ui_count) {
        return;
    }

    if (current_item_index > 0) {
        list_nav_prev(1);
    } else {
        list_nav_last();
    }
}

void handle_list_nav_down(void) {
    if (msgbox_active || !ui_count) {
        return;
    }

    if (current_item_index < ui_count - 1) {
        list_nav_next(1);
    } else {
        list_nav_first();
    }
}

void handle_list_nav_up_hold(void) {
    if (msgbox_active || !ui_count) {
        return;
    }

    if (current_item_index > 0) {
        list_nav_prev(1);
    }
}

void handle_list_nav_down_hold(void) {
    if (msgbox_active || !ui_count) {
        return;
    }

    if (current_item_index < ui_count - 1) {
        list_nav_next(1);
    }
}

void handle_list_nav_page_up(void) {
    if (msgbox_active || !ui_count) {
        return;
    }

    if (current_item_index > 0) {
        list_nav_prev(theme.MUX.ITEM.COUNT);
    }
}

void handle_list_nav_page_down(void) {
    if (msgbox_active || !ui_count) {
        return;
    }

    if (current_item_index < ui_count - 1) {
        list_nav_next(theme.MUX.ITEM.COUNT);
    }
}
