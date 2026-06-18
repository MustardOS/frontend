#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../init.h"
#include "common.h"
#include "nav.h"
#include "grid.h"
#include "../audio.h"
#include "../fileio.h"
#include "../log.h"
#include "../language.h"
#include "../theme.h"
#include "../config.h"
#include "../init.h"
#include "../kiosk.h"

char progress_bar_message[MAX_BUFFER_SIZE];
volatile int progress_bar_value = 0;
lv_timer_t *timer_update_progress;

void show_info_box(const char *title, const char *content, int is_content) {
    if (msgbox_active == 0) {
        lv_obj_clear_flag(ui_pnlHelp, LV_OBJ_FLAG_HIDDEN);

        if (is_content) {
            lv_obj_add_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
        }

        msgbox_active = 1;
        msgbox_element = ui_pnlHelp;

        lv_label_set_text(ui_lblHelpHeader, title);
        lv_label_set_text(ui_lblHelpContent, content);

        if (is_content) lv_label_set_text(ui_lblHelpPreviewHeader, title);

        lv_obj_t *ui_pnlItem = lv_obj_get_parent(ui_lblHelpContent);
        lv_obj_scroll_to_y(ui_pnlItem, 0, LV_ANIM_OFF);
    }
}

void nav_move(lv_group_t *group, int direction) {
    (direction < 0 ? nav_prev : nav_next)(group, 1);
}

void nav_prev(lv_group_t *group, int count) {
    for (int i = 0; i < count; i++) lv_group_focus_prev(group);
}

void nav_next(lv_group_t *group, int count) {
    for (int i = 0; i < count; i++) lv_group_focus_next(group);
}

void move_option(lv_obj_t *element, int count) {
    if (!count) return;

    uint16_t total = lv_dropdown_get_option_cnt(element);
    if (total <= 1) return;

    play_sound(SND_OPTION);

    uint16_t current = lv_dropdown_get_selected(element);
    int next = (int) current + count;

    if (next < 0) next = total + (next % total);
    next %= total;

    lv_dropdown_set_selected(element, (uint16_t) next);
}

void watermark(lv_obj_t *screen) {
    if (!TEST_IMAGE) return;

    static const uint8_t enc[] = {
            0xF1, 0xED, 0xEC, 0xF6, 0x85, 0xEC, 0xF6, 0x85, 0xE4, 0x85, 0xF1, 0xE0,
            0xF6, 0xF1, 0x85, 0xEC, 0xE8, 0xE4, 0xE2, 0xE0, 0x84, 0x85, 0x88, 0x85,
            0xF5, 0xE9, 0xE0, 0xE4, 0xF6, 0xE0, 0x85, 0xF7, 0xE0, 0xF5, 0xEA, 0xF7,
            0xF1, 0x85, 0xE4, 0xEB, 0xFC, 0x85, 0xEC, 0xF6, 0xF6, 0xF0, 0xE0, 0xF6,
            0x85, 0xF7, 0xE0, 0xF6, 0xF5, 0xEA, 0xEB, 0xF6, 0xEC, 0xE7, 0xE9, 0xFC
    };

    char msg[sizeof(enc) + 1];
    for (size_t i = 0; i < sizeof(enc); i++) msg[i] = (char) (enc[i] ^ 0xA5);
    msg[sizeof(enc)] = '\0';

    lv_obj_t *ui_conTest = lv_obj_create(screen);
    lv_obj_remove_style_all(ui_conTest);
    lv_obj_set_size(ui_conTest, LV_PCT(100), LV_PCT(100));
    lv_obj_set_align(ui_conTest, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_conTest, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(ui_conTest, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_conTest, &lv_font_unscii_8, MU_OBJ_MAIN_DEFAULT);

    lv_obj_t *ui_lblTestTop = lv_label_create(ui_conTest);
    lv_obj_set_size(ui_lblTestTop, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblTestTop, LV_ALIGN_TOP_MID);
    lv_obj_set_y(ui_lblTestTop, 4);
    lv_label_set_text(ui_lblTestTop, msg);
    lv_obj_add_flag(ui_lblTestTop, LV_OBJ_FLAG_FLOATING);

    lv_obj_t *ui_lblTestBottom = lv_label_create(ui_conTest);
    lv_obj_set_size(ui_lblTestBottom, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblTestBottom, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(ui_lblTestBottom, -4);
    lv_label_set_text(ui_lblTestBottom, msg);
    lv_obj_add_flag(ui_lblTestBottom, LV_OBJ_FLAG_FLOATING);

    lv_obj_move_foreground(ui_conTest);
}

void process_visual_element(enum visual_type visual, lv_obj_t *element) {
    switch (visual) {
        case VIS_CLOCK:
            if (!config.VISUAL.CLOCK) lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
            break;
        case VIS_BLUETOOTH:
            if (!config.VISUAL.BLUETOOTH) lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
            break;
        case VIS_NETWORK:
            if (!config.VISUAL.NETWORK) lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
            break;
        case VIS_BATTERY:
            switch (config.VISUAL.BATTERY) {
                case 1: // Text Only
                    lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lblBatteryPercent, LV_OBJ_FLAG_HIDDEN);
                    break;
                case 2: // Text + Icon
                    lv_obj_clear_flag(element, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lblBatteryPercent, LV_OBJ_FLAG_HIDDEN);
                    break;
                default: // Icon Only (0)
                    lv_obj_clear_flag(element, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_lblBatteryPercent, LV_OBJ_FLAG_HIDDEN);
                    break;
            }
            break;
        case VIS_HEADERTITLE:
            if (!config.VISUAL.HEADERTITLE) lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
            break;
    }
}

void update_grid_scroll_position(int col_count, int row_count, int row_height,
                                 int current_item_index, lv_obj_t *ui_pnlGrid) {
    uint8_t cell_row_index = get_grid_row_index(current_item_index);
    lv_coord_t scroll_y = lv_obj_get_scroll_y(ui_pnlGrid);
    int first_visible_row = scroll_y / row_height;
    int last_visible_row = first_visible_row + row_count - 1;

    // Check if the current cell is within the visible range
    if (cell_row_index >= first_visible_row && cell_row_index <= last_visible_row) {
        // The cell is already within the visible range; no scroll needed
        return;
    }

    int y_offset = 0;

    // If the cell is below the visible range, scroll to bring it into view at the bottom
    if (cell_row_index > last_visible_row) {
        y_offset = (cell_row_index - row_count + 1) * row_height;
    }

    // If the cell is above the visible range, scroll to bring it into view at the top
    if (cell_row_index < first_visible_row) {
        y_offset = cell_row_index * row_height;
    }

    lv_obj_scroll_to_y(ui_pnlGrid, y_offset, LV_ANIM_OFF);
}

void scroll_object_to_middle(lv_obj_t *container, lv_obj_t *obj) {
    lv_coord_t scroll_y = lv_obj_get_y(obj) - (lv_obj_get_height(container) / 2) + (lv_obj_get_height(obj) / 2);
    lv_obj_scroll_to(container, lv_obj_get_scroll_x(container), scroll_y, LV_ANIM_OFF);
}

void update_scroll_position(int mux_item_count, int mux_item_panel, int ui_count,
                            int current_item_index, lv_obj_t *ui_pnlContent) {
    // how many items should be above the currently selected item when scrolling
    double item_distribution = (mux_item_count - 1) / (double) 2;
    // how many items are off screen
    double scroll_multiplier = (current_item_index > item_distribution) ? (current_item_index - item_distribution)
                                                                        : 0;
    // max scroll value
    int isAtBottom = (current_item_index >= ui_count - item_distribution - 1);
    if (isAtBottom) scroll_multiplier = ui_count - mux_item_count;

    if (mux_item_count % 2 == 0 && current_item_index > item_distribution && !isAtBottom) {
        lv_obj_set_scroll_snap_y(ui_pnlContent, LV_SCROLL_SNAP_CENTER);
    } else {
        lv_obj_set_scroll_snap_y(ui_pnlContent, LV_SCROLL_SNAP_START);
    }

    int content_panel_y = (int) round(scroll_multiplier * mux_item_panel);
    lv_obj_scroll_to_y(ui_pnlContent, content_panel_y, LV_ANIM_OFF);
    lv_obj_update_snap(ui_pnlContent, LV_ANIM_OFF);
}

void add_drop_down_options(lv_obj_t *ui_lblItemDropDown, char *options[], int count) {
    lv_dropdown_clear_options(ui_lblItemDropDown);

    for (unsigned int i = 0; i < count; i++) {
        lv_dropdown_add_option(ui_lblItemDropDown, options[i], LV_DROPDOWN_POS_LAST);
    }
}

void map_drop_down_to_index(lv_obj_t *dropdown, int value, const int *options, int num_options, int def_index) {
    for (int i = 0; i < num_options; i++) {
        if (value == options[i]) {
            lv_dropdown_set_selected(dropdown, i);
            return;
        }
    }

    lv_dropdown_set_selected(dropdown, def_index);
}

int map_drop_down_to_value(int selected_index, const int *options, int num_options, int def_value) {
    if (selected_index >= 0 && selected_index < num_options) {
        return options[selected_index];
    }

    return def_value;
}

void show_progress_bar(char *message) {
    progress_bar_value = 0;
    snprintf(progress_bar_message, sizeof(progress_bar_message), "%s", message);
    timer_update_progress = lv_timer_create(update_progress_bar, TIMER_REFRESH, NULL);
}

void update_progress_bar(lv_timer_t *timer) {
    (void) timer;
    static int last_value = -1;
    if (last_value == progress_bar_value) return;
    lv_bar_set_value(ui_barProgress, progress_bar_value, LV_ANIM_OFF);

    char buf[256];
    snprintf(buf, sizeof(buf), "%s: %d%%", progress_bar_message, progress_bar_value);
    lv_label_set_text(ui_lblProgress, buf);
    if (lv_obj_has_flag(ui_pnlProgress, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(ui_pnlProgress, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ui_pnlProgress);
    }
}

void hide_progress_bar(void) {
    if (timer_update_progress) {
        lv_timer_del(timer_update_progress);
        timer_update_progress = NULL;
    }

    lv_obj_add_flag(ui_pnlProgress, LV_OBJ_FLAG_HIDDEN);
}

void set_nav_flags(struct nav_flag *nav_flags, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (nav_flags[i].visible) {
            lv_obj_clear_flag(nav_flags[i].element, MU_OBJ_FLAG_HIDE_FLOAT);
        } else {
            lv_obj_add_flag(nav_flags[i].element, MU_OBJ_FLAG_HIDE_FLOAT);
        }
    }
}

int direct_to_previous(lv_obj_t **ui_objects, size_t ui_count, int *nav_moved) {
    if (!file_exist(MUOS_PDI_LOAD)) return 0;

    char *prev = read_all_char_from(MUOS_PDI_LOAD);
    if (!prev) return 0;

    int text_hit = 0;
    for (size_t i = 0; i < ui_count; i++) {
        int item_hidden = lv_obj_has_flag(ui_objects[i], LV_OBJ_FLAG_HIDDEN);
        if (!item_hidden && strcasecmp(lv_obj_get_user_data(ui_objects[i]), prev) == 0) break;
        if (!item_hidden) text_hit++;
    }

    int nav_next_return = 0;
    if (text_hit > 0) {
        *nav_moved = 1;
        nav_next_return = text_hit;
    }

    free(prev);
    remove(MUOS_PDI_LOAD);

    return nav_next_return;
}
