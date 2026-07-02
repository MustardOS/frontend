#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../init.h"
#include "common.h"
#include "nav.h"
#include "grid.h"
#include "transition.h"
#include "../audio.h"
#include "../fileio.h"
#include "../config.h"

char progress_bar_message[MAX_BUFFER_SIZE];
volatile int progress_bar_value = 0;
lv_timer_t *timer_update_progress;

static lv_timer_t *timer_bounce_progress = NULL;
static int bounce_pos = 0;
static int bounce_direction = 1;

#define FOOTER_SCROLL_PAUSE_MS   1500
#define FOOTER_SCROLL_PX_PER_SEC 60
#define FOOTER_SCROLL_PAD_RIGHT  16

#define BOUNCE_SEGMENT_WIDTH 15
#define BOUNCE_STEP          2

static void footer_scroll_anim_cb(void *obj, const int32_t x) {
    lv_obj_t *panel = obj;
    const lv_coord_t delta = lv_obj_get_scroll_x(panel) - (lv_coord_t) x;
    if (delta != 0) lv_obj_scroll_by(panel, delta, 0, LV_ANIM_OFF);
}

static void help_panel_y_cb(void *obj, const int32_t y) {
    lv_obj_set_style_translate_y(obj, y, MU_OBJ_MAIN_DEFAULT);
}

static void help_panel_x_cb(void *obj, const int32_t x) {
    lv_obj_set_style_translate_x(obj, x, MU_OBJ_MAIN_DEFAULT);
}

static void help_panel_opa_cb(void *obj, const int32_t opa) {
    lv_obj_set_style_opa(obj, (lv_opa_t) opa, MU_OBJ_MAIN_DEFAULT);
}

static void help_dim_opa_cb(void *obj, const int32_t opa) {
    lv_obj_set_style_bg_opa(obj, (lv_opa_t) opa, MU_OBJ_MAIN_DEFAULT);
}

static void help_hide_ready_cb(lv_anim_t *a) {
    (void) a;
    lv_obj_add_flag(ui_pnl_help, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(ui_pnl_help, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_help, 155, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_translate_x(ui_pnl_help_message, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_translate_y(ui_pnl_help_message, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_opa(ui_pnl_help_message, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
}

void footer_nav_check_scroll(void) {
    if (!ui_pnl_footer) return;

    lv_anim_del(ui_pnl_footer, footer_scroll_anim_cb);

    const lv_coord_t cur = lv_obj_get_scroll_x(ui_pnl_footer);
    if (cur != 0) lv_obj_scroll_by(ui_pnl_footer, cur, 0, LV_ANIM_OFF);

    lv_obj_update_layout(ui_pnl_footer);

    lv_coord_t content_w = 0;
    const uint32_t n = lv_obj_get_child_cnt(ui_pnl_footer);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *c = lv_obj_get_child(ui_pnl_footer, (int32_t) i);
        if (!c) continue;
        if (lv_obj_has_flag_any(c, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING)) continue;
        content_w += lv_obj_get_width(c);
    }

    const lv_coord_t pad_left = lv_obj_get_style_pad_left(ui_pnl_footer, LV_PART_MAIN);
    const lv_coord_t pad_right = lv_obj_get_style_pad_right(ui_pnl_footer, LV_PART_MAIN);
    const lv_coord_t inner_w = lv_obj_get_width(ui_pnl_footer) - pad_left - pad_right;
    const lv_coord_t overflow = content_w - inner_w + FOOTER_SCROLL_PAD_RIGHT;

    if (overflow <= 4) return;

    uint32_t scroll_ms = (uint32_t) (overflow * 1000 / FOOTER_SCROLL_PX_PER_SEC);
    if (scroll_ms < 400) scroll_ms = 400;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ui_pnl_footer);
    lv_anim_set_exec_cb(&a, footer_scroll_anim_cb);
    lv_anim_set_values(&a, 0, overflow);
    lv_anim_set_time(&a, scroll_ms);
    lv_anim_set_delay(&a, FOOTER_SCROLL_PAUSE_MS);
    lv_anim_set_playback_time(&a, scroll_ms);
    lv_anim_set_playback_delay(&a, FOOTER_SCROLL_PAUSE_MS);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_repeat_delay(&a, FOOTER_SCROLL_PAUSE_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

void show_info_box(const char *title, const char *content, const int is_content) {
    if (msgbox_active == 0) {
        lv_anim_del(ui_pnl_help, help_dim_opa_cb);
        lv_anim_del(ui_pnl_help, help_panel_opa_cb);
        lv_anim_del(ui_pnl_help_message, help_panel_y_cb);
        lv_anim_del(ui_pnl_help_message, help_panel_x_cb);
        lv_anim_del(ui_pnl_help_message, help_panel_opa_cb);

        lv_obj_set_style_opa(ui_pnl_help, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_translate_x(ui_pnl_help_message, 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_translate_y(ui_pnl_help_message, 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_opa(ui_pnl_help_message, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);

        lv_obj_clear_flag(ui_pnl_help, LV_OBJ_FLAG_HIDDEN);

        if (is_content) {
            lv_obj_add_flag(ui_pnl_help_preview, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_pnl_help_message, LV_OBJ_FLAG_HIDDEN);
        }

        msgbox_active = 1;
        msgbox_element = ui_pnl_help;

        lv_label_set_text(ui_lbl_help_header, title);
        lv_label_set_text(ui_lbl_help_content, content);

        if (is_content) lv_label_set_text(ui_lbl_help_preview_header, title);

        lv_obj_t *ui_pnl_item = lv_obj_get_parent(ui_lbl_help_content);
        lv_obj_scroll_to_y(ui_pnl_item, 0, LV_ANIM_OFF);

        const int trans = config.visual.dialogue_transition;

        lv_anim_path_cb_t path;
        uint32_t duration;
        switch (trans) {
            case TSN_BOUNCE_RIGHT:
            case TSN_BOUNCE_LEFT:
            case TSN_BOUNCE_UP:
            case TSN_BOUNCE_DOWN:
                path = lv_anim_path_bounce;
                duration = 450;
                break;
            case TSN_SHOOT_RIGHT:
            case TSN_SHOOT_LEFT:
            case TSN_SHOOT_UP:
            case TSN_SHOOT_DOWN:
                path = lv_anim_path_overshoot;
                duration = 350;
                break;
            default:
                path = lv_anim_path_ease_out;
                duration = 250;
                break;
        }

        lv_anim_t ad;
        lv_anim_init(&ad);
        lv_anim_set_var(&ad, ui_pnl_help);
        lv_anim_set_exec_cb(&ad, help_dim_opa_cb);
        lv_anim_set_values(&ad, LV_OPA_TRANSP, 155);
        lv_anim_set_time(&ad, 200);
        lv_anim_set_path_cb(&ad, lv_anim_path_ease_out);
        lv_anim_start(&ad);

        if (trans == TSN_DISABLED) return;

        lv_anim_t ap;
        lv_anim_init(&ap);
        lv_anim_set_var(&ap, ui_pnl_help_message);
        lv_anim_set_time(&ap, duration);
        lv_anim_set_path_cb(&ap, path);

        const lv_coord_t w = LV_HOR_RES;
        const lv_coord_t h = LV_VER_RES;

        switch (trans) {
            case TSN_FADE_IN:
                lv_obj_set_style_opa(ui_pnl_help_message, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
                lv_anim_set_exec_cb(&ap, help_panel_opa_cb);
                lv_anim_set_values(&ap, LV_OPA_TRANSP, LV_OPA_COVER);
                break;
            case TSN_SLIDE_RIGHT:
            case TSN_BOUNCE_RIGHT:
            case TSN_SHOOT_RIGHT:
                lv_obj_set_style_translate_x(ui_pnl_help_message, w, MU_OBJ_MAIN_DEFAULT);
                lv_anim_set_exec_cb(&ap, help_panel_x_cb);
                lv_anim_set_values(&ap, w, 0);
                break;
            case TSN_SLIDE_LEFT:
            case TSN_BOUNCE_LEFT:
            case TSN_SHOOT_LEFT:
                lv_obj_set_style_translate_x(ui_pnl_help_message, -w, MU_OBJ_MAIN_DEFAULT);
                lv_anim_set_exec_cb(&ap, help_panel_x_cb);
                lv_anim_set_values(&ap, -w, 0);
                break;
            case TSN_SLIDE_DOWN:
            case TSN_BOUNCE_DOWN:
            case TSN_SHOOT_DOWN:
                lv_obj_set_style_translate_y(ui_pnl_help_message, -h, MU_OBJ_MAIN_DEFAULT);
                lv_anim_set_exec_cb(&ap, help_panel_y_cb);
                lv_anim_set_values(&ap, -h, 0);
                break;
            default:
                lv_obj_set_style_translate_y(ui_pnl_help_message, h, MU_OBJ_MAIN_DEFAULT);
                lv_anim_set_exec_cb(&ap, help_panel_y_cb);
                lv_anim_set_values(&ap, h, 0);
                break;
        }

        lv_anim_start(&ap);
    }
}

void hide_info_box(void) {
    if (!ui_pnl_help) return;

    lv_anim_del(ui_pnl_help, help_dim_opa_cb);
    lv_anim_del(ui_pnl_help_message, help_panel_y_cb);
    lv_anim_del(ui_pnl_help_message, help_panel_x_cb);
    lv_anim_del(ui_pnl_help_message, help_panel_opa_cb);

    lv_anim_t ap;
    lv_anim_init(&ap);
    lv_anim_set_var(&ap, ui_pnl_help_message);
    lv_anim_set_exec_cb(&ap, help_panel_opa_cb);
    lv_anim_set_values(&ap, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&ap, 150);
    lv_anim_set_path_cb(&ap, lv_anim_path_linear);
    lv_anim_start(&ap);

    lv_anim_t ad;
    lv_anim_init(&ad);
    lv_anim_set_var(&ad, ui_pnl_help);
    lv_anim_set_exec_cb(&ad, help_dim_opa_cb);
    lv_anim_set_values(&ad, 155, LV_OPA_TRANSP);
    lv_anim_set_time(&ad, 150);
    lv_anim_set_path_cb(&ad, lv_anim_path_linear);
    lv_anim_set_ready_cb(&ad, help_hide_ready_cb);
    lv_anim_start(&ad);
}

void nav_move(lv_group_t *group, const int direction) {
    (direction < 0 ? nav_prev : nav_next)(group, 1);
}

void nav_prev(lv_group_t *group, const int count) {
    for (int i = 0; i < count; i++)
        lv_group_focus_prev(group);
}

void nav_next(lv_group_t *group, const int count) {
    for (int i = 0; i < count; i++)
        lv_group_focus_next(group);
}

void move_option(lv_obj_t *element, const int count) {
    if (!count) return;

    const uint16_t total = lv_dropdown_get_option_cnt(element);
    if (total <= 1) return;

    play_sound(snd_option);

    const uint16_t current = lv_dropdown_get_selected(element);
    int next = (int) current + count;

    if (next < 0) next = total + next % total;
    next %= total;

    lv_dropdown_set_selected(element, (uint16_t) next);
}

void watermark(lv_obj_t *screen) {
    if (!TEST_IMAGE) return;

    static const uint8_t enc[] = {0xF1, 0xED, 0xEC, 0xF6, 0x85, 0xEC, 0xF6, 0x85, 0xE4, 0x85, 0xF1, 0xE0,
                                  0xF6, 0xF1, 0x85, 0xEC, 0xE8, 0xE4, 0xE2, 0xE0, 0x84, 0x85, 0x88, 0x85,
                                  0xF5, 0xE9, 0xE0, 0xE4, 0xF6, 0xE0, 0x85, 0xF7, 0xE0, 0xF5, 0xEA, 0xF7,
                                  0xF1, 0x85, 0xE4, 0xEB, 0xFC, 0x85, 0xEC, 0xF6, 0xF6, 0xF0, 0xE0, 0xF6,
                                  0x85, 0xF7, 0xE0, 0xF6, 0xF5, 0xEA, 0xEB, 0xF6, 0xEC, 0xE7, 0xE9, 0xFC};

    char msg[sizeof(enc) + 1];
    for (size_t i = 0; i < sizeof(enc); i++)
        msg[i] = (char) (enc[i] ^ 0xA5);
    msg[sizeof(enc)] = '\0';

    lv_obj_t *ui_con_test = lv_obj_create(screen);
    lv_obj_remove_style_all(ui_con_test);
    lv_obj_set_size(ui_con_test, LV_PCT(100), LV_PCT(100));
    lv_obj_set_align(ui_con_test, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_con_test, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(ui_con_test, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_con_test, &lv_font_unscii_8, MU_OBJ_MAIN_DEFAULT);

    lv_obj_t *ui_lbl_test_top = lv_label_create(ui_con_test);
    lv_obj_set_size(ui_lbl_test_top, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lbl_test_top, LV_ALIGN_TOP_MID);
    lv_obj_set_y(ui_lbl_test_top, 4);
    lv_label_set_text(ui_lbl_test_top, msg);
    lv_obj_add_flag(ui_lbl_test_top, LV_OBJ_FLAG_FLOATING);

    lv_obj_t *ui_lbl_test_bottom = lv_label_create(ui_con_test);
    lv_obj_set_size(ui_lbl_test_bottom, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lbl_test_bottom, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(ui_lbl_test_bottom, -4);
    lv_label_set_text(ui_lbl_test_bottom, msg);
    lv_obj_add_flag(ui_lbl_test_bottom, LV_OBJ_FLAG_FLOATING);

    lv_obj_move_foreground(ui_con_test);
}

void process_visual_element(const enum visual_type visual, lv_obj_t *element) {
    switch (visual) {
        case vis_clock:
            if (!config.visual.clock) lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
            break;
        case vis_bluetooth:
            if (!config.visual.bluetooth) lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
            break;
        case vis_network:
            if (!config.visual.network) lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
            break;
        case vis_battery:
            switch (config.visual.battery) {
                case 1: // Text Only
                    lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lbl_battery_percent, LV_OBJ_FLAG_HIDDEN);
                    break;
                case 2: // Text + Icon
                    lv_obj_clear_flag(element, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lbl_battery_percent, LV_OBJ_FLAG_HIDDEN);
                    break;
                default: // Icon Only (0)
                    lv_obj_clear_flag(element, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_lbl_battery_percent, LV_OBJ_FLAG_HIDDEN);
                    break;
            }
            break;
        case vis_headertitle:
            if (!config.visual.header_title) lv_obj_add_flag(element, LV_OBJ_FLAG_HIDDEN);
            break;
    }
}

void update_grid_scroll_position(
    const int row_count, const int row_height, const int current_item_index, lv_obj_t *ui_pnl_grid
) {
    const uint8_t cell_row_index = get_grid_row_index(current_item_index);
    const lv_coord_t scroll_y = lv_obj_get_scroll_y(ui_pnl_grid);
    const int first_visible_row = scroll_y / row_height;
    const int last_visible_row = first_visible_row + row_count - 1;

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

    lv_obj_scroll_to_y(ui_pnl_grid, y_offset, LV_ANIM_OFF);
}

void scroll_object_to_middle(lv_obj_t *container, const lv_obj_t *obj) {
    const lv_coord_t scroll_y = lv_obj_get_y(obj) - lv_obj_get_height(container) / 2 + lv_obj_get_height(obj) / 2;
    lv_obj_scroll_to(container, lv_obj_get_scroll_x(container), scroll_y, LV_ANIM_OFF);
}

void update_scroll_position(
    const int mux_item_count, const int mux_item_panel, const int ui_count_static, const int current_item_index,
    lv_obj_t *ui_pnl_content
) {
    // how many items should be above the currently selected item when scrolling
    const double item_distribution = (mux_item_count - 1) / (double) 2;
    // how many items are off screen
    double scroll_multiplier = current_item_index > item_distribution ? current_item_index - item_distribution : 0;
    // max scroll value
    const int is_at_bottom = current_item_index >= ui_count_static - item_distribution - 1;
    if (is_at_bottom) scroll_multiplier = ui_count_static - mux_item_count;

    if (mux_item_count % 2 == 0 && current_item_index > item_distribution && !is_at_bottom) {
        lv_obj_set_scroll_snap_y(ui_pnl_content, LV_SCROLL_SNAP_CENTER);
    } else {
        lv_obj_set_scroll_snap_y(ui_pnl_content, LV_SCROLL_SNAP_START);
    }

    const int content_panel_y = (int) round(scroll_multiplier * mux_item_panel);
    lv_obj_scroll_to_y(ui_pnl_content, content_panel_y, LV_ANIM_OFF);
    lv_obj_update_snap(ui_pnl_content, LV_ANIM_OFF);
}

void update_windowed_list(
    const lv_obj_t *ui_pnl_content, const int direction, const int current_item_index, const int total_count,
    const int visible_count, void (*update_item_cb)(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, int index),
    void (*update_items_cb)(int start_index)
) {
    if (total_count <= visible_count) return;

    const int items_before = (visible_count - visible_count % 2) / 2;
    const int items_after = (visible_count - 1) / 2;

    if (direction < 0) {
        if (current_item_index == total_count - 1) {
            update_items_cb(total_count - visible_count);
        } else if (current_item_index >= items_before && current_item_index < total_count - items_after - 1) {
            lv_obj_t *last_item = lv_obj_get_child(ui_pnl_content, visible_count - 1);
            lv_obj_move_to_index(last_item, 0);
            update_item_cb(
                lv_obj_get_child(last_item, 0), lv_obj_get_child(last_item, 1), current_item_index - items_before
            );
        }
    } else {
        if (current_item_index == 0) {
            update_items_cb(0);
        } else if (current_item_index > items_before && current_item_index < total_count - items_after) {
            lv_obj_t *first_item = lv_obj_get_child(ui_pnl_content, 0);
            lv_obj_move_to_index(first_item, visible_count - 1);
            update_item_cb(
                lv_obj_get_child(first_item, 0), lv_obj_get_child(first_item, 1), current_item_index + items_after
            );
        }
    }
}

void add_drop_down_options(lv_obj_t *ui_lbl_item_drop_down, char *options[], const int count) {
    lv_dropdown_clear_options(ui_lbl_item_drop_down);

    for (unsigned int i = 0; i < count; i++) {
        lv_dropdown_add_option(ui_lbl_item_drop_down, options[i], LV_DROPDOWN_POS_LAST);
    }
}

void map_drop_down_to_index(
    lv_obj_t *dropdown, const int value, const int *options, const int num_options, const int def_index
) {
    for (int i = 0; i < num_options; i++) {
        if (value == options[i]) {
            lv_dropdown_set_selected(dropdown, i);
            return;
        }
    }

    lv_dropdown_set_selected(dropdown, def_index);
}

int map_drop_down_to_value(const int selected_index, const int *options, const int num_options, const int def_value) {
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
    lv_bar_set_value(ui_bar_progress, progress_bar_value, LV_ANIM_OFF);

    char buf[256];
    snprintf(buf, sizeof(buf), "%s: %d%%", progress_bar_message, progress_bar_value);
    lv_label_set_text(ui_lbl_progress, buf);
    if (lv_obj_has_flag(ui_pnl_progress, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(ui_pnl_progress, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ui_pnl_progress);
    }
}

void hide_progress_bar(void) {
    if (timer_update_progress) {
        lv_timer_del(timer_update_progress);
        timer_update_progress = NULL;
    }

    lv_obj_add_flag(ui_pnl_progress, LV_OBJ_FLAG_HIDDEN);
}

static void update_bounce_progress_bar(lv_timer_t *timer) {
    (void) timer;

    bounce_pos += bounce_direction * BOUNCE_STEP;

    if (bounce_pos >= 100 - BOUNCE_SEGMENT_WIDTH) {
        bounce_pos = 100 - BOUNCE_SEGMENT_WIDTH;
        bounce_direction = -1;
    } else if (bounce_pos <= 0) {
        bounce_pos = 0;
        bounce_direction = 1;
    }

    lv_bar_set_start_value(ui_bar_progress, bounce_pos, LV_ANIM_OFF);
    lv_bar_set_value(ui_bar_progress, bounce_pos + BOUNCE_SEGMENT_WIDTH, LV_ANIM_OFF);
}

void show_bounce_progress_bar(const char *message) {
    lv_label_set_text(ui_lbl_progress, message);

    lv_bar_set_mode(ui_bar_progress, LV_BAR_MODE_RANGE);

    bounce_pos = 0;
    bounce_direction = 1;

    lv_bar_set_start_value(ui_bar_progress, 0, LV_ANIM_OFF);
    lv_bar_set_value(ui_bar_progress, BOUNCE_SEGMENT_WIDTH, LV_ANIM_OFF);

    lv_obj_clear_flag(ui_pnl_progress, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_pnl_progress);

    timer_bounce_progress = lv_timer_create(update_bounce_progress_bar, TIMER_REFRESH, NULL);
}

void hide_bounce_progress_bar(void) {
    if (timer_bounce_progress) {
        lv_timer_del(timer_bounce_progress);
        timer_bounce_progress = NULL;
    }

    lv_bar_set_mode(ui_bar_progress, LV_BAR_MODE_NORMAL);
    lv_obj_add_flag(ui_pnl_progress, LV_OBJ_FLAG_HIDDEN);
}

void set_nav_flags(const struct nav_flag *nav_flags, const size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (nav_flags[i].visible) {
            lv_obj_clear_flag(nav_flags[i].element, MU_OBJ_FLAG_HIDE_FLOAT);
        } else {
            lv_obj_add_flag(nav_flags[i].element, MU_OBJ_FLAG_HIDE_FLOAT);
        }
    }

    footer_nav_check_scroll();
}

int direct_to_previous(lv_obj_t **ui_objects, const size_t ui_count_static, int *nav_moved) {
    if (!file_exist(MUOS_PDI_LOAD)) return 0;

    char *prev = read_all_char_from(MUOS_PDI_LOAD);
    if (!prev) return 0;

    int text_hit = 0;
    for (size_t i = 0; i < ui_count_static; i++) {
        const int item_hidden = lv_obj_has_flag(ui_objects[i], LV_OBJ_FLAG_HIDDEN);
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

static lv_obj_t *shake_prev_focused = NULL;
static int shake_suppress = 0;
static lv_anim_exec_xcb_t shake_prev_exec_cb = NULL;

void nav_suppress_next_shake(void) {
    shake_suppress = 1;
    shake_prev_focused = NULL;
}

void nav_unsuppress_shake(void) {
    shake_suppress = 0;
}

static void focus_shake_y_cb(void *obj, const int32_t v) {
    lv_obj_set_style_translate_y(obj, v, MU_OBJ_MAIN_DEFAULT);
}

static void focus_shake_x_cb(void *obj, const int32_t v) {
    lv_obj_set_style_translate_x(obj, v, MU_OBJ_MAIN_DEFAULT);
}

static void reset_shake_styles(lv_obj_t *obj) {
    lv_obj_set_style_translate_y(obj, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_translate_x(obj, 0, MU_OBJ_MAIN_DEFAULT);
}

enum { sel_dir_up = 0, sel_dir_down = 1, sel_dir_left = 2, sel_dir_right = 3, sel_dir_all = 4 };

static int last_nav_dir = nav_dir_down;

void nav_set_last_dir(const enum nav_direction dir) {
    last_nav_dir = dir;
}

void nav_focus_shake_cb(const lv_group_t *group) {
    if (shake_prev_focused && lv_obj_is_valid(shake_prev_focused)) {
        lv_anim_del(shake_prev_focused, shake_prev_exec_cb);
        reset_shake_styles(shake_prev_focused);
    }

    lv_obj_t *focused = lv_group_get_focused(group);
    shake_prev_focused = focused;

    static const int shake_travel[] = {0, 2, 4, 6, 8, 10, 30};

    const int level = config.visual.selection_animation;
    if (shake_suppress || level <= 0 || level > 6) return;

    if (!focused || !lv_obj_is_valid(focused)) return;

    int dir = config.visual.selection_style;
    if (dir == sel_dir_all) {
        switch (last_nav_dir) {
            case nav_dir_up:
                dir = sel_dir_up;
                break;
            case nav_dir_left:
                dir = sel_dir_left;
                break;
            case nav_dir_right:
                dir = sel_dir_right;
                break;
            case nav_dir_down:
            default:
                dir = sel_dir_down;
                break;
        }
    }

    lv_anim_exec_xcb_t exec_cb;
    int sign;
    switch (dir) {
        case sel_dir_up:
            exec_cb = focus_shake_y_cb;
            sign = -1;
            break;
        case sel_dir_left:
            exec_cb = focus_shake_x_cb;
            sign = -1;
            break;
        case sel_dir_right:
            exec_cb = focus_shake_x_cb;
            sign = 1;
            break;
        case sel_dir_down:
        default:
            exec_cb = focus_shake_y_cb;
            sign = 1;
            break;
    }

    shake_prev_exec_cb = exec_cb;

    lv_anim_del(focused, exec_cb);
    reset_shake_styles(focused);

    const int travel = sign * shake_travel[level];

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, focused);
    lv_anim_set_exec_cb(&a, exec_cb);
    lv_anim_set_values(&a, 0, travel);
    lv_anim_set_time(&a, 80);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_playback_delay(&a, 20);
    lv_anim_set_playback_time(&a, 130);
    lv_anim_start(&a);
}
