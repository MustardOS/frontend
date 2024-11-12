#include "list_nav.h"

#include <sys/param.h>

#include "../common.h"
#include "../theme.h"

void list_nav_prev(int steps);

void list_nav_next(int steps);

extern int ui_count;
extern int current_item_index;

void handle_list_nav_up(void) {
    if (msgbox_active || !ui_count) return;

    list_nav_prev(1);
}

void handle_list_nav_down(void) {
    if (msgbox_active || !ui_count) return;

    list_nav_next(1);
}

void handle_list_nav_up_hold(void) {
    if (msgbox_active || !ui_count) return;

    // Don't wrap around when scrolling on hold.
    if (current_item_index > 0) {
        list_nav_prev(1);
    }
}

void handle_list_nav_down_hold(void) {
    if (msgbox_active || !ui_count) return;

    // Don't wrap around when scrolling on hold.
    if (current_item_index < ui_count - 1) {
        list_nav_next(1);
    }
}

void handle_list_nav_page_up(void) {
    if (msgbox_active || !ui_count) return;

    // Don't wrap around when scrolling by page.
    int steps = MIN(theme.MUX.ITEM.COUNT, current_item_index);
    if (steps > 0) {
        list_nav_prev(steps);
    }
}

void handle_list_nav_page_down(void) {
    if (msgbox_active || !ui_count) return;

    // Don't wrap around when scrolling by page.
    int steps = MIN(theme.MUX.ITEM.COUNT, ui_count - current_item_index - 1);
    if (steps > 0) {
        list_nav_next(steps);
    }
}
