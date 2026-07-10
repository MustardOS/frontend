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
#include "../theme.h"
#include "../../module/muxshare.h"

char progress_bar_message[MAX_BUFFER_SIZE];
volatile int progress_bar_value = 0;
lv_timer_t *timer_update_progress;

static int progress_bar_last_value = -1;

static lv_timer_t *timer_bounce_progress = NULL;
static int bounce_pos = 0;
static int bounce_direction = 1;

static char bounce_base_message[MAX_BUFFER_SIZE];
static int bounce_timeout = 0;
static time_t bounce_start = 0;
static int bounce_last_remaining = -1;

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
    nav_play_shake(element, count < 0 ? nav_dir_left : nav_dir_right);
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

static void blank_overflow_rows(const lv_obj_t *panel) {
    lv_area_t panel_area;
    lv_obj_get_coords(panel, &panel_area);

    const uint32_t child_count = lv_obj_get_child_cnt(panel);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(panel, (int32_t) i);
        if (!child) continue;

        lv_area_t child_area;
        lv_obj_get_coords(child, &child_area);

        const lv_coord_t tr_y = lv_obj_get_style_translate_y(child, LV_PART_MAIN);
        const lv_coord_t y1 = child_area.y1 - tr_y;
        const lv_coord_t y2 = child_area.y2 - tr_y;

        // 2px grace covers row padding, hopefully it is enough!
        const int outside = y2 <= panel_area.y1 + 2 || y1 >= panel_area.y2 - 2;
        const lv_opa_t want = outside ? LV_OPA_TRANSP : LV_OPA_COVER;

        if (lv_obj_get_style_opa(child, LV_PART_MAIN) != want) {
            lv_obj_set_style_opa(child, want, MU_OBJ_MAIN_DEFAULT);
        }
    }
}

static void blank_overflow_rows_cb(lv_event_t *e) {
    blank_overflow_rows(lv_event_get_target(e));
}

static lv_obj_t *raise_row = NULL;

static void raise_row_draw_cb(lv_event_t *e);

void nav_watch_list_overflow(lv_obj_t *panel) {
    lv_obj_add_event_cb(panel, blank_overflow_rows_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(panel, blank_overflow_rows_cb, LV_EVENT_LAYOUT_CHANGED, NULL);
    lv_obj_add_event_cb(panel, raise_row_draw_cb, LV_EVENT_DRAW_POST, NULL);
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

void list_win_focus_group(const int index) {
    if (index < 0 || index >= theme.mux.item.count) return;
    lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, index);

    if (!panel) return;

    lv_group_focus_obj(panel);
    lv_group_focus_obj(lv_obj_get_child(panel, 0));
    lv_group_focus_obj(lv_obj_get_child(panel, 1));
}

int list_win_focus_index(void) {
    const int before = (theme.mux.item.count - theme.mux.item.count % 2) / 2;
    const int after = (theme.mux.item.count - 1) / 2;

    if (current_item_index < before) return current_item_index;
    if (current_item_index >= (int) item_count - after)
        return theme.mux.item.count - ((int) item_count - current_item_index);

    return before;
}

void list_win_move_index(const int direction) {
    if (direction < 0) {
        current_item_index = current_item_index == 0 ? ui_count_static - 1 : current_item_index - 1;
    } else {
        current_item_index = current_item_index == ui_count_static - 1 ? 0 : current_item_index + 1;
    }
}

void list_win_update_items(
    const int start_index, void (*update_item_cb)(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, int index)
) {
    const int max = (int) item_count - start_index;
    if (max <= 0) return;

    int count = theme.mux.item.count;
    if (count > max) count = max;

    for (int index = 0; index < count; ++index) {
        const lv_obj_t *panel_item = lv_obj_get_child(ui_pnl_content, index);
        update_item_cb(lv_obj_get_child(panel_item, 0), lv_obj_get_child(panel_item, 1), start_index + index);
    }
}

void list_win_focus_initial(void (*update_item_cb)(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, int index)) {
    const int count = theme.mux.item.count;

    if ((int) item_count <= count) {
        list_win_focus_group(current_item_index);
    } else {
        const int before = (count - count % 2) / 2;
        const int after = (count - 1) / 2;

        int start_index;
        if (current_item_index < before) {
            start_index = 0;
        } else if (current_item_index >= (int) item_count - after) {
            start_index = (int) item_count - count;
        } else {
            start_index = current_item_index - before;
        }

        list_win_update_items(start_index, update_item_cb);

        int new_item_index = current_item_index - start_index;
        if (new_item_index < 0) new_item_index = 0;
        if (new_item_index >= count) new_item_index = count - 1;

        list_win_focus_group(new_item_index);
    }

    set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);

    nav_moved = 1;
    first_open = 0;
}

static void (*list_win_active_update_item_cb)(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, int index) = NULL;

static void list_win_update_items_trampoline(const int start_index) {
    list_win_update_items(start_index, list_win_active_update_item_cb);
}

void list_win_nav_move(
    const int steps, const int direction,
    void (*update_item_cb)(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, int index)
) {
    if (!ui_count_static) return;
    first_open ? (first_open = 0) : play_sound(snd_navigate);

    const int visible_count = theme.mux.item.count;
    const int static_list = (int) item_count <= visible_count;

    apply_text_long_dot(&theme, lv_group_get_focused(ui_group));

    if (static_list) {
        for (int step = 0; step < steps; ++step) {
            list_win_move_index(direction);
        }
        list_win_focus_group(current_item_index);
    } else {
        list_win_active_update_item_cb = update_item_cb;

        for (int step = 0; step < steps; ++step) {
            list_win_move_index(direction);

            nav_move(ui_group, direction);
            nav_move(ui_group_glyph, direction);
            nav_move(ui_group_panel, direction);

            update_windowed_list(
                ui_pnl_content, direction, current_item_index, (int) item_count, visible_count, update_item_cb,
                list_win_update_items_trampoline
            );

            list_win_focus_group(list_win_focus_index());
        }
    }

    set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);

    nav_moved = 1;
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
    progress_bar_last_value = -1; // reset so a fresh session always renders its first tick
    snprintf(progress_bar_message, sizeof(progress_bar_message), "%s", message);
    timer_update_progress = lv_timer_create(update_progress_bar, TIMER_REFRESH, NULL);
}

void update_progress_bar(lv_timer_t *timer) {
    (void) timer;
    if (progress_bar_last_value == progress_bar_value) return;
    progress_bar_last_value = progress_bar_value;
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

    if (bounce_timeout > 0) {
        const int elapsed = (int) (time(NULL) - bounce_start);
        const int remaining = bounce_timeout - elapsed > 0 ? bounce_timeout - elapsed : 0;

        if (remaining != bounce_last_remaining) {
            bounce_last_remaining = remaining;

            char buf[MAX_BUFFER_SIZE];
            snprintf(buf, sizeof(buf), "%s (%ds)", bounce_base_message, remaining);
            lv_label_set_text(ui_lbl_progress, buf);
        }
    }
}

void show_bounce_progress_bar(const char *message, const int timeout_seconds) {
    snprintf(bounce_base_message, sizeof(bounce_base_message), "%s", message);
    bounce_timeout = timeout_seconds;
    bounce_start = time(NULL);
    bounce_last_remaining = -1;

    if (bounce_timeout > 0) {
        char buf[MAX_BUFFER_SIZE];
        snprintf(buf, sizeof(buf), "%s (%ds)", bounce_base_message, bounce_timeout);
        lv_label_set_text(ui_lbl_progress, buf);
        bounce_last_remaining = bounce_timeout;
    } else {
        lv_label_set_text(ui_lbl_progress, bounce_base_message);
    }

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

    bounce_timeout = 0;

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

static lv_obj_t *shake_screen_ancestor(lv_obj_t *obj) {
    lv_obj_t *ancestor = obj;
    while (ancestor && lv_obj_get_parent(ancestor) != ui_screen) {
        ancestor = lv_obj_get_parent(ancestor);
    }
    return ancestor;
}

static void raise_row_draw_cb(lv_event_t *e) {
    if (!raise_row || !lv_obj_is_valid(raise_row)) return;
    if (lv_obj_get_parent(raise_row) != lv_event_get_target(e)) return;
    if (lv_obj_has_flag(raise_row, LV_OBJ_FLAG_HIDDEN)) return;

    lv_obj_redraw(lv_event_get_draw_ctx(e), raise_row);
}

static lv_obj_t *hoist_panel = NULL;
static uint32_t hoist_index = 0;

static void shake_cleanup_cb(lv_anim_t *a) {
    lv_obj_t *obj = a->var;
    if (!obj || !lv_obj_is_valid(obj)) return;

    lv_obj_t *ancestor = shake_screen_ancestor(obj);
    if (!ancestor) return;

    if (ancestor == ui_pnl_content && lv_obj_has_flag(ancestor, LV_OBJ_FLAG_OVERFLOW_VISIBLE)) {
        lv_obj_clear_flag(ancestor, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_invalidate(ancestor);

        if (hoist_panel && lv_obj_is_valid(hoist_panel)) {
            lv_obj_move_to_index(hoist_panel, (int32_t) hoist_index);
        }
        hoist_panel = NULL;
    }

    const lv_obj_t *row = obj;
    while (row && lv_obj_get_parent(row) != ancestor)
        row = lv_obj_get_parent(row);

    if (row && row == raise_row) {
        lv_obj_invalidate(raise_row);
        raise_row = NULL;
    }
}

static lv_anim_exec_xcb_t play_shake(lv_obj_t *obj, const enum nav_direction hint) {
    const int level = config.visual.selection_animation;
    if (level <= 0 || level > 6) return NULL;
    if (!obj || !lv_obj_is_valid(obj)) return NULL;

    int dir = config.visual.selection_style;
    if (dir == sel_dir_all) {
        switch (hint) {
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

    static const int shake_travel[] = {0, 2, 4, 6, 8, 10, 30};

    lv_anim_del(obj, exec_cb);
    reset_shake_styles(obj);

    const int travel = sign * shake_travel[level];

    lv_obj_t *ancestor = shake_screen_ancestor(obj);
    if (ancestor) {
        int poke = 0;

        if (exec_cb == focus_shake_y_cb && ancestor == ui_pnl_content) {
            lv_area_t obj_area;
            lv_area_t panel_area;
            lv_obj_get_coords(obj, &obj_area);
            lv_obj_get_coords(ancestor, &panel_area);

            poke = sign < 0 ? obj_area.y1 + travel < panel_area.y1 : obj_area.y2 + travel > panel_area.y2;
        }

        if (poke) {
            if (!hoist_panel) {
                hoist_index = lv_obj_get_index(ancestor);
                hoist_panel = ancestor;
                lv_obj_move_foreground(ancestor);
            }

            lv_obj_add_flag(ancestor, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
            blank_overflow_rows(ancestor);
        }

        lv_obj_t *row = obj;
        while (row && lv_obj_get_parent(row) != ancestor)
            row = lv_obj_get_parent(row);

        if (row && row != ancestor) {
            if (raise_row && raise_row != row && lv_obj_is_valid(raise_row)) lv_obj_invalidate(raise_row);
            raise_row = row;
        }
    }

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, exec_cb);
    lv_anim_set_values(&a, 0, travel);
    lv_anim_set_time(&a, 80);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_playback_delay(&a, 20);
    lv_anim_set_playback_time(&a, 130);
    lv_anim_set_deleted_cb(&a, shake_cleanup_cb);
    lv_anim_start(&a);

    return exec_cb;
}

void nav_focus_shake_cb(const lv_group_t *group) {
    if (shake_prev_focused && lv_obj_is_valid(shake_prev_focused)) {
        lv_anim_del(shake_prev_focused, shake_prev_exec_cb);
        reset_shake_styles(shake_prev_focused);
    }

    lv_obj_t *focused = lv_group_get_focused(group);
    shake_prev_focused = focused;

    if (shake_suppress) return;

    const lv_anim_exec_xcb_t exec_cb = play_shake(focused, last_nav_dir);
    if (exec_cb) shake_prev_exec_cb = exec_cb;
}

void nav_play_shake(lv_obj_t *obj, const enum nav_direction hint) {
    play_shake(obj, hint);
}
