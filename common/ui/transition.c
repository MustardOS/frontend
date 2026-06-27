#include "transition.h"
#include "common.h"
#include "../device.h"

#define SCROLL_STOP_MS 350

#define FADE_IN_MS   200
#define SLIDE_IN_MS  250
#define BOUNCE_IN_MS 450
#define SHOOT_IN_MS  320

static lv_timer_t *scroll_stop_timer = NULL;
static int is_scrolling = 0;

static void (*scroll_stop_cb)(void) = NULL;

static void opa_anim_cb(void *obj, const int32_t opa) {
    lv_obj_set_style_opa(obj, (lv_opa_t) opa, MU_OBJ_MAIN_DEFAULT);
}

static void translate_x_anim_cb(void *obj, const int32_t x) {
    lv_obj_set_style_translate_x(obj, x, MU_OBJ_MAIN_DEFAULT);
}

static void translate_y_anim_cb(void *obj, const int32_t y) {
    lv_obj_set_style_translate_y(obj, y, MU_OBJ_MAIN_DEFAULT);
}

static void slide_in_x(const lv_coord_t start_x, const lv_anim_path_cb_t path, const uint32_t duration) {
    lv_obj_set_style_translate_x(ui_pnl_box, start_x, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_translate_y(ui_pnl_box, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_opa(ui_pnl_box, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ui_pnl_box);
    lv_anim_set_exec_cb(&a, translate_x_anim_cb);
    lv_anim_set_values(&a, start_x, 0);
    lv_anim_set_time(&a, duration);
    lv_anim_set_path_cb(&a, path);
    lv_anim_start(&a);
}

static void slide_in_y(const lv_coord_t start_y, const lv_anim_path_cb_t path, const uint32_t duration) {
    lv_obj_set_style_translate_x(ui_pnl_box, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_translate_y(ui_pnl_box, start_y, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_opa(ui_pnl_box, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ui_pnl_box);
    lv_anim_set_exec_cb(&a, translate_y_anim_cb);
    lv_anim_set_values(&a, start_y, 0);
    lv_anim_set_time(&a, duration);
    lv_anim_set_path_cb(&a, path);
    lv_anim_start(&a);
}

static void scroll_stop_timer_cb(lv_timer_t *timer) {
    (void) timer;

    lv_timer_pause(scroll_stop_timer);
    is_scrolling = 0;

    if (scroll_stop_cb) scroll_stop_cb();
}

void transition_box_art_init(void (*on_scroll_stop)(void)) {
    scroll_stop_cb = on_scroll_stop;
    is_scrolling = 0;

    if (!scroll_stop_timer) {
        scroll_stop_timer = lv_timer_create(scroll_stop_timer_cb, SCROLL_STOP_MS, NULL);
        lv_timer_pause(scroll_stop_timer);
    }
}

void transition_box_art_nav_activity(void) {
    if (!scroll_stop_timer) return;

    if (!is_scrolling) {
        is_scrolling = 1;

        lv_anim_del(ui_pnl_box, opa_anim_cb);
        lv_anim_del(ui_pnl_box, translate_x_anim_cb);
        lv_anim_del(ui_pnl_box, translate_y_anim_cb);

        lv_obj_set_style_opa(ui_pnl_box, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);

        lv_obj_set_style_translate_x(ui_pnl_box, 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_translate_y(ui_pnl_box, 0, MU_OBJ_MAIN_DEFAULT);

        lv_timer_resume(scroll_stop_timer);
    } else {
        lv_timer_reset(scroll_stop_timer);
    }
}

void transition_box_art_key_released(void) {
    if (!is_scrolling || !scroll_stop_timer) return;
    lv_timer_reset(scroll_stop_timer);
}

void transition_box_art_apply_in(const int type) {
    if (!ui_pnl_box) return;

    lv_anim_del(ui_pnl_box, opa_anim_cb);
    lv_anim_del(ui_pnl_box, translate_x_anim_cb);
    lv_anim_del(ui_pnl_box, translate_y_anim_cb);

    const lv_coord_t w = device.mux.width;
    const lv_coord_t h = device.mux.height;

    switch (type) {
        case TSN_FADE_IN:
            lv_obj_set_style_translate_x(ui_pnl_box, 0, MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_style_translate_y(ui_pnl_box, 0, MU_OBJ_MAIN_DEFAULT);

            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, ui_pnl_box);
            lv_anim_set_exec_cb(&a, opa_anim_cb);
            lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
            lv_anim_set_time(&a, FADE_IN_MS);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
            lv_anim_start(&a);
            break;

        case TSN_SLIDE_RIGHT:
            slide_in_x(w, lv_anim_path_ease_out, SLIDE_IN_MS);
            break;
        case TSN_SLIDE_LEFT:
            slide_in_x(-w, lv_anim_path_ease_out, SLIDE_IN_MS);
            break;
        case TSN_SLIDE_UP:
            slide_in_y(h, lv_anim_path_ease_out, SLIDE_IN_MS);
            break;
        case TSN_SLIDE_DOWN:
            slide_in_y(-h, lv_anim_path_ease_out, SLIDE_IN_MS);
            break;

        case TSN_BOUNCE_RIGHT:
            slide_in_x(w, lv_anim_path_bounce, BOUNCE_IN_MS);
            break;
        case TSN_BOUNCE_LEFT:
            slide_in_x(-w, lv_anim_path_bounce, BOUNCE_IN_MS);
            break;
        case TSN_BOUNCE_UP:
            slide_in_y(h, lv_anim_path_bounce, BOUNCE_IN_MS);
            break;
        case TSN_BOUNCE_DOWN:
            slide_in_y(-h, lv_anim_path_bounce, BOUNCE_IN_MS);
            break;

        case TSN_SHOOT_RIGHT:
            slide_in_x(w, lv_anim_path_overshoot, SHOOT_IN_MS);
            break;
        case TSN_SHOOT_LEFT:
            slide_in_x(-w, lv_anim_path_overshoot, SHOOT_IN_MS);
            break;
        case TSN_SHOOT_UP:
            slide_in_y(h, lv_anim_path_overshoot, SHOOT_IN_MS);
            break;
        case TSN_SHOOT_DOWN:
            slide_in_y(-h, lv_anim_path_overshoot, SHOOT_IN_MS);
            break;

        default:
            lv_obj_set_style_opa(ui_pnl_box, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);

            lv_obj_set_style_translate_x(ui_pnl_box, 0, MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_style_translate_y(ui_pnl_box, 0, MU_OBJ_MAIN_DEFAULT);
            break;
    }
}

void transition_box_art_destroy(void) {
    if (scroll_stop_timer) {
        lv_timer_del(scroll_stop_timer);
        scroll_stop_timer = NULL;
    }

    if (ui_pnl_box) {
        lv_anim_del(ui_pnl_box, opa_anim_cb);
        lv_anim_del(ui_pnl_box, translate_x_anim_cb);
        lv_anim_del(ui_pnl_box, translate_y_anim_cb);

        lv_obj_set_style_opa(ui_pnl_box, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);

        lv_obj_set_style_translate_x(ui_pnl_box, 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_translate_y(ui_pnl_box, 0, MU_OBJ_MAIN_DEFAULT);
    }

    is_scrolling = 0;
    scroll_stop_cb = NULL;
}
