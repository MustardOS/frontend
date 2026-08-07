#include <stdio.h>
#include <string.h>
#include "task_progress.h"
#include "task_prompt.h"
#include "dialogue.h"
#include "../audio.h"
#include "../device.h"
#include "modal.h"
#include "../language.h"
#include "common.h"
#include "glyph.h"

#define BOUNCE_WIDTH    20
#define BOUNCE_CYCLE_MS 1500

static struct theme_config *theme_ref = NULL;

static lv_obj_t *dim = NULL;
static lv_obj_t *panel = NULL;
static lv_obj_t *lbl_title = NULL;
static lv_obj_t *lbl_status = NULL;
static lv_obj_t *lbl_detail = NULL;
static lv_obj_t *bar = NULL;
static lv_obj_t *lbl_elapsed = NULL;
static lv_obj_t *nav_glyph = NULL;
static lv_obj_t *nav_label = NULL;

static int active = 0;
static int bounce_pos = 0;

static mux_dialogue cancel_dlg;
static int cancel_asking = 0;

typedef enum { nav_offers_none = 0, nav_offers_a, nav_offers_b } nav_offer;

static nav_offer offered = nav_offers_none;

static void close_cancel_ask(void);

void task_progress_init(struct theme_config *t, lv_obj_t *parent) {
    theme_ref = t;

    active = 0;
    cancel_asking = 0;
    modal_reset();
    offered = nav_offers_none;
    memset(&cancel_dlg, 0, sizeof(cancel_dlg));

    dim = lv_obj_create(parent);
    lv_obj_set_size(dim, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(dim, 0, 0);
    lv_obj_set_style_bg_color(dim, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dim, t->dialogue.dim_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(dim, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(dim, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(dim, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(dim, MU_OBJ_FLAG_HIDE_FLOAT);

    panel = lv_obj_create(parent);
    lv_obj_set_width(panel, (int) (device.mux.width * .8));
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_set_align(panel, LV_ALIGN_CENTER);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, MU_OBJ_FLAG_HIDE_FLOAT);

    lv_obj_set_style_bg_color(panel, lv_color_hex(t->help.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(panel, t->help.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(panel, lv_color_hex(t->help.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(panel, t->help.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(panel, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(panel, t->help.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_hor(panel, 16, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_ver(panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(panel, 6, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);

    lbl_title = lv_label_create(panel);
    lv_obj_set_width(lbl_title, lv_pct(100));
    lv_label_set_long_mode(lbl_title, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(t->help.title), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(lbl_title, 8, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(lbl_title, 8, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(lbl_title, LV_BORDER_SIDE_BOTTOM, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(lbl_title, lv_color_hex(t->list_default.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(lbl_title, t->list_default.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(lbl_title, 1, MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(lbl_title, "");

    lbl_status = lv_label_create(panel);
    lv_obj_set_width(lbl_status, lv_pct(100));
    lv_label_set_long_mode(lbl_status, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(t->help.content), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(lbl_status, 8, MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(lbl_status, "");

    bar = lv_bar_create(panel);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, 12);
    lv_obj_set_style_radius(bar, t->bar.progress_radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(bar, lv_color_hex(t->bar.progress_main_background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(bar, t->bar.progress_main_background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(bar, t->bar.progress_radius, MU_OBJ_INDI_DEFAULT);
    lv_obj_set_style_bg_color(bar, lv_color_hex(t->bar.progress_active_background), MU_OBJ_INDI_DEFAULT);
    lv_obj_set_style_bg_opa(bar, t->bar.progress_active_background_alpha, MU_OBJ_INDI_DEFAULT);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);

    lbl_detail = lv_label_create(panel);
    lv_obj_set_width(lbl_detail, lv_pct(100));
    lv_label_set_long_mode(lbl_detail, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(lbl_detail, lv_color_hex(t->help.content), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(lbl_detail, 160, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(lbl_detail, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(lbl_detail, "");

    lv_obj_t *footer = lv_obj_create(panel);
    lv_obj_set_width(footer, lv_pct(100));
    lv_obj_set_height(footer, LV_SIZE_CONTENT);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(footer, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(footer, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(footer, 8, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(footer, 8, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(footer, LV_BORDER_SIDE_TOP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(footer, lv_color_hex(t->list_default.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(footer, t->list_default.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(footer, 1, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(footer, 2, MU_OBJ_MAIN_DEFAULT);

    lv_obj_t *nav_row = lv_obj_create(footer);
    lv_obj_set_width(nav_row, LV_SIZE_CONTENT);
    lv_obj_set_height(nav_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(nav_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(nav_row, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(nav_row, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(nav_row, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_flex_flow(nav_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    nav_glyph = create_footer_glyph(nav_row, t, "b", t->nav.b, 0);
    nav_label = create_footer_text(nav_row, t, t->nav.b.text, t->nav.b.text_alpha, 0);
    lv_label_set_text(nav_label, "");

    lbl_elapsed = lv_label_create(footer);
    lv_obj_set_style_text_color(lbl_elapsed, lv_color_hex(t->help.content), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(lbl_elapsed, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(lbl_elapsed, "");

    task_prompt_init(t, parent);
}

int task_progress_active(void) {
    return active;
}

void task_progress_show(void) {
    if (!panel) return;

    active = 1;
    bounce_pos = 0;
    cancel_asking = 0;
    offered = nav_offers_none;

    lv_obj_clear_flag(bar, MU_OBJ_FLAG_HIDE_FLOAT);

    modal_claim(MODAL_MASK_ACKNOWLEDGE);

    lv_obj_clear_flag(dim, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(panel, MU_OBJ_FLAG_HIDE_FLOAT);

    lv_obj_move_foreground(dim);
    lv_obj_move_foreground(panel);
}

void task_progress_hide(void) {
    if (!panel) return;

    close_cancel_ask();

    active = 0;
    modal_release();

    lv_obj_add_flag(panel, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(dim, MU_OBJ_FLAG_HIDE_FLOAT);
}

static void set_nav(const char *glyph_name, const char *text) {
    if (!nav_label) return;

    if (!text || !text[0]) {
        offered = nav_offers_none;

        lv_obj_add_flag(nav_label, MU_OBJ_FLAG_HIDE_FLOAT);
        if (nav_glyph) lv_obj_add_flag(nav_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        return;
    }

    offered = glyph_name[0] == 'a' ? nav_offers_a : nav_offers_b;

    char embed[MAX_BUFFER_SIZE];
    if (nav_glyph && get_glyph_path("footer", glyph_name, embed, sizeof(embed)))
        set_footer_glyph_image(nav_glyph, embed);

    lv_label_set_text(nav_label, text);

    lv_obj_clear_flag(nav_label, MU_OBJ_FLAG_HIDE_FLOAT);
    if (nav_glyph) lv_obj_clear_flag(nav_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
}

static void ask_cancel(void) {
    if (cancel_dlg.panel) {
        if (cancel_dlg.dim && lv_obj_is_valid(cancel_dlg.dim)) lv_obj_del(cancel_dlg.dim);
        if (lv_obj_is_valid(cancel_dlg.panel)) lv_obj_del(cancel_dlg.panel);

        memset(&cancel_dlg, 0, sizeof(cancel_dlg));
    }

    dialogue_init_confirm(
        &cancel_dlg, theme_ref, lv_obj_get_parent(panel), lang.generic.warning, lang.generic.cancel_task,
        lang.generic.confirm, lang.generic.back, lang.generic.select, lang.generic.back
    );

    dialogue_show(&cancel_dlg);
    lv_obj_move_foreground(cancel_dlg.dim);
    lv_obj_move_foreground(cancel_dlg.panel);

    cancel_dlg.selected = mux_confirm_nah;
    dialogue_refresh(&cancel_dlg, theme_ref);

    cancel_asking = 1;
}

static void close_cancel_ask(void) {
    if (!cancel_asking) return;

    cancel_asking = 0;
    dialogue_hide(&cancel_dlg);
}

void task_progress_reset(void) {
    theme_ref = NULL;

    dim = NULL;
    panel = NULL;
    lbl_title = NULL;
    lbl_status = NULL;
    lbl_detail = NULL;
    bar = NULL;
    lbl_elapsed = NULL;
    nav_glyph = NULL;
    nav_label = NULL;

    active = 0;
    cancel_asking = 0;
    offered = nav_offers_none;

    memset(&cancel_dlg, 0, sizeof(cancel_dlg));
}

void task_progress_tick(void) {
    if (!active || !panel || !lv_obj_is_valid(panel)) return;

    task_exec_poll();

    const task_exec_status *st = task_exec_get_status();

    if (st->prompt_active && !task_prompt_active()) {
        if (task_prompt_show(&st->prompt) != 0) task_exec_respond(st->prompt.id, st->prompt.fallback);
    } else if (!st->prompt_active && task_prompt_active()) {
        task_prompt_hide();
    }

    lv_label_set_text(lbl_title, st->title[0] ? st->title : lang.generic.loading);

    lv_label_set_text(lbl_detail, st->detail);
    if (st->detail[0])
        lv_obj_clear_flag(lbl_detail, MU_OBJ_FLAG_HIDE_FLOAT);
    else
        lv_obj_add_flag(lbl_detail, MU_OBJ_FLAG_HIDE_FLOAT);

    char clock[16];
    snprintf(clock, sizeof(clock), "%lu:%02lu", st->elapsed_ms / 60000UL, st->elapsed_ms / 1000UL % 60UL);

    char elapsed[MAX_BUFFER_SIZE];
    if (st->state == task_state_complete) {
        snprintf(elapsed, sizeof(elapsed), "%s %s", lang.generic.finished_in, clock);
    } else if (st->state == task_state_error) {
        snprintf(elapsed, sizeof(elapsed), "%s %s", lang.generic.stopped_after, clock);
    } else {
        snprintf(elapsed, sizeof(elapsed), "%s", clock);
    }

    lv_label_set_text(lbl_elapsed, elapsed);

    switch (st->state) {
        case task_state_complete:
            lv_label_set_text(lbl_status, st->message[0] ? st->message : lang.generic.completed);
            lv_obj_add_flag(bar, MU_OBJ_FLAG_HIDE_FLOAT);
            set_nav("b", lang.generic.close);
            return;

        case task_state_error:
            lv_label_set_text(
                lbl_status, st->cancelled    ? lang.generic.cancelled
                            : st->message[0] ? st->message
                                             : lang.generic.failed
            );
            lv_obj_add_flag(bar, MU_OBJ_FLAG_HIDE_FLOAT);
            set_nav("b", lang.generic.close);
            return;

        case task_state_cancelling:
            lv_label_set_text(lbl_status, lang.generic.cancelled);
            set_nav("b", "");
            break;

        default:
            lv_label_set_text(lbl_status, st->status);
            set_nav("b", st->can_cancel ? lang.generic.cancel : "");
            break;
    }

    if (st->determinate && st->max > 0) {
        const long percent = st->value * 100 / st->max;
        lv_bar_set_mode(bar, LV_BAR_MODE_NORMAL);
        lv_bar_set_start_value(bar, 0, LV_ANIM_OFF);
        lv_bar_set_value(bar, (int32_t) (percent < 0 ? 0 : percent > 100 ? 100 : percent), LV_ANIM_OFF);
        return;
    }

    const long travel = 100 - BOUNCE_WIDTH;
    bounce_pos = (int) (travel * (long) (st->elapsed_ms % BOUNCE_CYCLE_MS) / BOUNCE_CYCLE_MS);

    lv_bar_set_mode(bar, LV_BAR_MODE_RANGE);
    lv_bar_set_start_value(bar, bounce_pos, LV_ANIM_OFF);
    lv_bar_set_value(bar, bounce_pos + BOUNCE_WIDTH, LV_ANIM_OFF);
}

int task_progress_handle_a(void) {
    if (!active) return 0;
    if (task_prompt_handle_a()) return 1;

    if (cancel_asking) {
        const int stop = cancel_dlg.selected == mux_confirm_yep;

        play_sound(stop ? snd_confirm : snd_back);
        close_cancel_ask();

        if (stop) task_exec_cancel();
        return 1;
    }

    return 1;
}

int task_progress_handle_b(void) {
    if (!active) return 0;
    if (task_prompt_handle_b()) return 1;

    if (cancel_asking) {
        play_sound(snd_back);
        close_cancel_ask();
        return 1;
    }

    if (offered != nav_offers_b) return 1;

    const task_exec_status *st = task_exec_get_status();

    if (st->state == task_state_complete || st->state == task_state_error) {
        play_sound(snd_back);
        task_exec_acknowledge();
        task_progress_hide();
        return 1;
    }

    play_sound(snd_info_open);
    ask_cancel();

    return 1;
}

int task_progress_handle_dpad(const int direction) {
    if (!active) return 0;

    if (cancel_asking) {
        dialogue_handle_dpad(&cancel_dlg, theme_ref, direction, 1);
        return 1;
    }

    return task_prompt_handle_dpad(direction);
}

int task_progress_handle_dpad_hold(const int direction) {
    if (!active) return 0;

    if (cancel_asking) {
        dialogue_handle_dpad_hold(&cancel_dlg, theme_ref, direction, 1);
        return 1;
    }

    return task_prompt_handle_dpad_hold(direction);
}
