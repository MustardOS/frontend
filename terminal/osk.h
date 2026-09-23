#pragma once

#include <SDL2/SDL.h>
#include <lvgl/lvgl.h>

#define OSK_STATE_HIDDEN        0
#define OSK_STATE_BOTTOM_OPAQUE 1
#define OSK_STATE_BOTTOM_TRANS  2
#define OSK_STATE_TOP_OPAQUE    3
#define OSK_STATE_TOP_TRANS     4
#define OSK_NUM_STATES          5

typedef enum {
    INPUT_ACT_NONE = 0,
    INPUT_ACT_OSK_TOGGLE,
    INPUT_ACT_QUIT,
    INPUT_ACT_MENU,
    INPUT_ACT_PRESS,
    INPUT_ACT_BACKSPACE,
    INPUT_ACT_SPACE,
    INPUT_ACT_LAYER_PREV,
    INPUT_ACT_LAYER_NEXT,
    INPUT_ACT_ENTER,
    INPUT_ACT_PAGE_UP,
    INPUT_ACT_PAGE_DOWN
} input_action_t;

void osk_init(lv_obj_t *parent, int screen_w, int screen_h, int cell_h);

void osk_update_metrics(int screen_w, int cell_h);

int osk_get_height(void);

int osk_is_visible(void);

int osk_get_state(void);

void osk_show_nav(void);

void osk_cycle_state(void);

void osk_close(void);

void osk_move(int drow, int dcol);

void osk_switch_layer(int delta);

void osk_press_key(void);

void osk_hold_start(input_action_t action);

void osk_hold_end(void);

int osk_hold_tick(Uint32 now);

input_action_t osk_hold_action(void);

int osk_ctrl_active(void);

int osk_alt_active(void);

void osk_reset_axis_state(void);

void osk_refresh(void);

void osk_apply_action(input_action_t action, int *running, int *vis_rows, int term_height, int readonly, int pty_fd);

void osk_handle_axis(int axis, int val);

void osk_handle_hat(Uint8 hat);

int osk_load_layout(const char *path);

void osk_free(void);

void osk_set_repeat(int delay_ms, int rate_ms);
