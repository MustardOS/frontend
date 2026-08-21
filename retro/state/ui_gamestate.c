#include <time.h>
#include "../../common/audio.h"
#include "../../common/init.h"
#include "../../common/input.h"
#include "../../common/log.h"
#include "../../common/ui/common.h"
#include "../../common/ui/dialogue.h"
#include "../../common/ui/glyph.h"
#include "../../common/ui/image.h"
#include "../../common/ui/list_frame.h"
#include "../../common/ui/osk.h"
#include "../../common/randname.h"
#include "../../module/muxshare.h"
#include "../video/image_writer.h"
#include "gamestate.h"
#include "history.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../input/nav_repeat.h"

static int active = 0;
static uint64_t prev_nav_mask = 0;

static nav_repeat_t rpt_up = {0};
static nav_repeat_t rpt_down = {0};

static nav_repeat_t rpt_osk_up = {0};
static nav_repeat_t rpt_osk_down = {0};
static nav_repeat_t rpt_osk_left = {0};
static nav_repeat_t rpt_osk_right = {0};

static nav_repeat_t rpt_backspace = {0};

static mux_dialogue load_dlg;

static mux_dialogue delete_dlg;

static mux_dialogue mismatch_dlg;

static mux_dialogue purge_dlg;
static mux_dialogue empty_dlg;

static mux_dialogue notice_dlg;
static uint64_t notice_prev_mask = 0;

typedef enum { pending_none, pending_load, pending_delete, pending_purge, pending_empty } pending_action_t;

static pending_action_t pending_action = pending_none;
static int pending_index = -1;

static lv_obj_t *ui_pnl_entry_gamestate = NULL;
static lv_obj_t *ui_txt_entry_gamestate = NULL;
static int creating_save = 0;

enum { section_states = 0, section_history, section_trash, section_count };

#define GAMESTATE_ROW_MAX (GAMESTATE_MAX_SLOTS + GAMESTATE_TIMELINE_DEPTH + 2 + HISTORY_DEPTH_MAX + GAMESTATE_TRASH_MAX)

static lv_obj_t *row_panels[GAMESTATE_ROW_MAX];
static lv_obj_t *row_labels[GAMESTATE_ROW_MAX];
static lv_obj_t *row_glyphs[GAMESTATE_ROW_MAX];

static int state_row_count = 0;
static int frame_section[section_count];
static int frame_total = 0;
static int history_row_count = 0;
static int trash_row_count = 0;
static int sectioned = 0;

static int focused_row(void) {
    return sectioned ? list_frame_current_row() : -1;
}

static int current_section(void) {
    if (!sectioned || frame_total <= 0) return -1;

    const int frame = list_frame_current();
    return frame >= 0 && frame < frame_total ? frame_section[frame] : -1;
}

static int in_history(void) {
    return current_section() == section_history;
}

static int in_trash(void) {
    return current_section() == section_trash;
}

static uint64_t current_nav_mask(void) {
    const int confirm = mux_input_pressed(mux_input_a);
    const int back = mux_input_pressed(mux_input_b);
    const int del = mux_input_pressed(mux_input_x);
    const int save = mux_input_pressed(mux_input_y);
    const int select_btn = mux_input_pressed(mux_input_select);
    const int start_btn = mux_input_pressed(mux_input_start);

    return nav_dir_bits() | (confirm ? BIT(4) : 0) | (back ? BIT(5) : 0) | (del ? BIT(6) : 0) | (save ? BIT(7) : 0)
           | (select_btn ? BIT(8) : 0) | (start_btn ? BIT(9) : 0) | nav_mask_page();
}

static void nav_show_x(const int show, const char *text) {
    if (show) {
        lv_label_set_text(ui_lbl_nav_x, text);
        lv_obj_clear_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void nav_show_y(const int show, const char *text) {
    if (show) {
        lv_label_set_text(ui_lbl_nav_y, text);
        lv_obj_clear_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static const char *glyph_or_state(const char *name) {
    char embed[MAX_BUFFER_SIZE];
    return get_glyph_path("muxretro", name, embed, sizeof(embed)) ? name : "state";
}

static const char *history_source_label(const enum history_source source) {
    switch (source) {
        case history_source_quick:
            return lang.muxretro.gamestate.source_quick;
        case history_source_auto:
            return lang.muxretro.gamestate.source_auto;
        case history_source_timeline:
            return lang.muxretro.gamestate.source_timeline;
        default:
            return lang.muxretro.gamestate.source_standard;
    }
}

static void history_row_text(const int index, char *buf, const size_t size) {
    enum history_source source = history_source_standard;
    long long created = 0;

    if (history_describe(index, &source, &created) != 0) {
        buf[0] = '\0';
        return;
    }

    const time_t moment = created;
    const struct tm *parts = localtime(&moment);

    char stamp[32];
    if (parts)
        strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", parts);
    else
        snprintf(stamp, sizeof(stamp), "%s", lang.generic.unknown);

    snprintf(buf, size, "%s - %s", history_source_label(source), stamp);
}

static int quicksave_row_offset(void) {
    return gamestate_quicksave_exists ? 1 : 0;
}

static int timeline_rows(int out[GAMESTATE_TIMELINE_DEPTH]) {
    int count = 0;

    for (int i = 0; i < GAMESTATE_TIMELINE_DEPTH; i++)
        if (gamestate_timeline_exists[i]) out[count++] = i;

    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (gamestate_timeline[out[j]].created > gamestate_timeline[out[i]].created) {
                const int tmp = out[i];
                out[i] = out[j];
                out[j] = tmp;
            }
        }
    }

    return count;
}

static int timeline_row_count(void) {
    int rows[GAMESTATE_TIMELINE_DEPTH];
    return timeline_rows(rows);
}

static int timeline_base_row(void) {
    return quicksave_row_offset() + (gamestate_autosave_exists ? 1 : 0);
}

static int pinned_row_offset(void) {
    return timeline_base_row() + timeline_row_count();
}

static int timeline_at_row(const int row) {
    const int rel = row - timeline_base_row();
    if (rel < 0) return -1;

    int rows[GAMESTATE_TIMELINE_DEPTH];
    const int count = timeline_rows(rows);
    if (rel >= count) return -1;

    return rows[rel];
}

static int timeline_slot_is_newest(const int slot) {
    return slot >= 0 && slot == timeline_at_row(timeline_base_row());
}

static const struct gamestate_slot *state_at_row(const int row) {
    if (gamestate_quicksave_exists && row == 0) return &gamestate_quicksave;
    if (gamestate_autosave_exists && row == quicksave_row_offset()) return &gamestate_autosave;

    const int timeline_slot = timeline_at_row(row);
    if (timeline_slot >= 0) return &gamestate_timeline[timeline_slot];

    const int slot_index = row - pinned_row_offset();
    if (slot_index >= 0 && slot_index < gamestate_slot_count) return &gamestate_slots[slot_index];

    return NULL;
}

static void refresh_row_nav(void) {
    const int on_row = focused_row() >= 0;

    if (in_trash()) {
        nav_show_a(on_row, lang.muxretro.gamestate.restore);
        nav_show_x(on_row, lang.muxretro.gamestate.purge);
        nav_show_y(gamestate_trash_count > 0, lang.muxretro.gamestate.empty);
    } else {
        nav_show_a(on_row, lang.generic.load);
        nav_show_x(on_row && !in_history(), lang.muxretro.gamestate.delete);
        nav_show_y(!in_history() && gamestate_slot_count < GAMESTATE_MAX_SLOTS, lang.generic.save);
    }

    const int on_bar = sectioned && frame_total > 1 && list_frame_focused();

    nav_show_lr(on_bar);
    if (on_bar) lv_label_set_text(ui_lbl_nav_lr, lang.generic.change);
}

static void refresh_thumbnail(void) {
    const char *thumb_path = NULL;

    int reused_path = 0;

    const int row = focused_row();

    if (row >= 0 && in_trash()) {
        const int entry = row - state_row_count - history_row_count;
        if (entry >= 0 && entry < gamestate_trash_count && gamestate_trash[entry].thumb_path[0])
            thumb_path = gamestate_trash[entry].thumb_path;
    } else if (row >= 0 && in_history()) {
        thumb_path = history_thumbnail(row - state_row_count);
        reused_path = 1;
    } else if (row >= 0) {
        const struct gamestate_slot *sel = state_at_row(row);
        if (sel) {
            thumb_path = sel->thumb_path;
            reused_path = row < pinned_row_offset();
        }
    }

    refresh_row_nav();

    lv_obj_clear_flag(ui_img_box, LV_OBJ_FLAG_HIDDEN);

    if (!thumb_path) {
        clear_image(ui_img_box);
        return;
    }

    if (reused_path) lv_img_cache_invalidate_src(NULL);

    const int16_t box = (int16_t) session_settings_state_thumbnail_width();

    const struct image_settings settings = {
        .image_path = (char *) thumb_path,
        .align = LV_ALIGN_BOTTOM_RIGHT,
        .max_width = box,
        .max_height = box,
    };

    update_image(ui_img_box, settings);
}

static void refresh_empty_state(void) {
    const char *empty = lang.muxretro.gamestate.none_found;
    if (in_history()) empty = lang.muxretro.gamestate.history_none;
    if (in_trash()) empty = lang.muxretro.gamestate.trash_none;
    const int rows = sectioned ? ui_count_static - 1 : 0;

    lv_label_set_text(ui_lbl_screen_message, rows > 0 ? "" : empty);

    refresh_row_nav();
}

static void capture_row(const int row) {
    if (row < 0 || row >= GAMESTATE_ROW_MAX) return;

    lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, lv_obj_get_child_cnt(ui_pnl_content) - 1);
    if (!panel) return;

    row_panels[row] = panel;
    row_labels[row] = lv_obj_get_child(panel, 0);
    row_glyphs[row] = lv_obj_get_child(panel, 1);
}

static void rebuild_rows(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();
    list_frame_reset();

    memset(row_panels, 0, sizeof(row_panels));
    memset(row_labels, 0, sizeof(row_labels));
    memset(row_glyphs, 0, sizeof(row_glyphs));

    ui_count_static = 0;
    current_item_index = 0;
    sectioned = 0;

    int row = 0;

    if (gamestate_quicksave_exists) {
        gen_label("muxretro", "state", gamestate_quicksave.name);
        capture_row(row++);
    }

    if (gamestate_autosave_exists) {
        gen_label("muxretro", "state", gamestate_autosave.name);
        capture_row(row++);
    }

    int trows[GAMESTATE_TIMELINE_DEPTH];
    const int tcount = timeline_rows(trows);
    for (int i = 0; i < tcount; i++) {
        gen_label("muxretro", "state", gamestate_timeline[trows[i]].name);
        capture_row(row++);
    }

    for (int i = 0; i < gamestate_slot_count; i++) {
        gen_label("muxretro", "state", gamestate_slots[i].name);
        capture_row(row++);
    }

    state_row_count = row;

    history_row_count = history_count();
    if (history_row_count > 0) {
        const char *glyph = glyph_or_state("save_history");

        for (int i = 0; i < history_row_count; i++) {
            char label[128];
            history_row_text(i, label, sizeof(label));
            gen_label("muxretro", glyph, label);
            capture_row(row++);
        }
    }

    trash_row_count = gamestate_trash_count;
    if (trash_row_count > 0) {
        const char *glyph = glyph_or_state("save_trash");

        for (int i = 0; i < trash_row_count; i++) {
            gen_label("muxretro", glyph, gamestate_trash[i].name);
            capture_row(row++);
        }
    }

    static list_frame frames[section_count];
    frame_total = 0;

    if (state_row_count > 0) {
        frame_section[frame_total] = section_states;
        frames[frame_total++] = (list_frame){lang.muxretro.gamestate.section_states, 0, state_row_count};
    }

    if (history_row_count > 0) {
        frame_section[frame_total] = section_history;
        frames[frame_total++] = (list_frame){lang.muxretro.gamestate.history, state_row_count, history_row_count};
    }

    if (trash_row_count > 0) {
        frame_section[frame_total] = section_trash;
        frames[frame_total++] =
            (list_frame){lang.muxretro.gamestate.trash, state_row_count + history_row_count, trash_row_count};
    }

    if (frame_total > 0
        && list_frame_init(
            &theme, ui_pnl_content, frames, frame_total, row_panels, row_labels, row_glyphs, NULL, row
        )) {
        sectioned = 1;
        list_frame_apply();
    } else {
        ui_count_static = row;
    }

    first_open = 0;

    refresh_thumbnail();
    refresh_empty_state();
}

static void focus_row(const int row) {
    if (!sectioned) return;
    if (row < 0 || row >= state_row_count + history_row_count + trash_row_count) return;

    int section = section_states;
    int first = 0;

    if (row >= state_row_count + history_row_count) {
        section = section_trash;
        first = state_row_count + history_row_count;
    } else if (row >= state_row_count) {
        section = section_history;
        first = state_row_count;
    }

    int frame = -1;
    for (int i = 0; i < frame_total; i++)
        if (frame_section[i] == section) frame = i;

    if (frame < 0) return;
    if (list_frame_current() != frame && !list_frame_go(frame)) return;

    current_item_index = 0;
    gen_step_movement(row - first + 1, +1, 1, 0, 0);
}

static int move_section(const int direction) {
    if (!sectioned || !list_frame_move(direction)) return 0;

    play_sound(snd_option);
    gen_step_movement(0, +1, 1, 0, 0);

    refresh_thumbnail();
    refresh_empty_state();

    return 1;
}

static void create_osk_objects(void) {
    ui_pnl_entry_gamestate = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnl_entry_gamestate, device.mux.width);
    lv_obj_set_height(ui_pnl_entry_gamestate, device.mux.height);
    lv_obj_set_align(ui_pnl_entry_gamestate, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnl_entry_gamestate, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_pnl_entry_gamestate, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnl_entry_gamestate, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_entry_gamestate, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnl_entry_gamestate, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_entry_gamestate, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_entry_gamestate, 128, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_entry_gamestate, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnl_entry_gamestate, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnl_entry_gamestate, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnl_entry_gamestate, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnl_entry_gamestate, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnl_entry_gamestate, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnl_entry_gamestate, 5, MU_OBJ_MAIN_DEFAULT);

    ui_txt_entry_gamestate = lv_textarea_create(ui_pnl_entry_gamestate);
    lv_obj_set_width(ui_txt_entry_gamestate, device.mux.width * 5 / 6);
    lv_obj_set_height(ui_txt_entry_gamestate, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_txt_entry_gamestate, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ui_txt_entry_gamestate, GAMESTATE_NAME_MAX - 1);
    lv_textarea_set_one_line(ui_txt_entry_gamestate, 1);
    lv_obj_set_style_radius(ui_txt_entry_gamestate, theme.osk.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_txt_entry_gamestate, lv_color_hex(theme.osk.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_txt_entry_gamestate, theme.osk.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_txt_entry_gamestate, 2, MU_OBJ_MAIN_DEFAULT);
}

void gamestate_menu_init(void) {
    dialogue_init_confirm(
        &load_dlg, &theme, ui_screen, lang.muxretro.gamestate.load_title, lang.muxretro.gamestate.load_desc,
        lang.generic.load, lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );
    dialogue_init_confirm(
        &delete_dlg, &theme, ui_screen, lang.muxretro.gamestate.delete_title, lang.muxretro.gamestate.delete_desc,
        lang.muxretro.gamestate.delete, lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );
    dialogue_init_confirm(
        &mismatch_dlg, &theme, ui_screen, lang.generic.warning, lang.muxretro.gamestate.mismatch_load,
        lang.generic.load, lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );
    dialogue_init_confirm(
        &purge_dlg, &theme, ui_screen, lang.muxretro.gamestate.purge_title, lang.muxretro.gamestate.purge_desc,
        lang.muxretro.gamestate.purge, lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );
    dialogue_init_confirm(
        &empty_dlg, &theme, ui_screen, lang.muxretro.gamestate.empty_title, lang.muxretro.gamestate.empty_desc,
        lang.muxretro.gamestate.empty, lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );
    dialogue_init_accept(
        &notice_dlg, &theme, ui_screen, lang.generic.warning, lang.muxretro.gamestate.mismatch_notice,
        lang.generic.understand
    );

    create_osk_objects();
}

void gamestate_menu_open(void) {
    active = 1;
    prev_nav_mask = current_nav_mask();

    image_writer_flush();

    lv_obj_clear_flag(ui_pnl_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});
    nav_show_y(1, lang.generic.save);

    rebuild_rows();

    if (sectioned && ui_count_static > 1) {
        gen_step_movement(1, +1, 1, 0, 0);
        refresh_thumbnail();
    }

    pause_menu_fix_nav_order();
}

int gamestate_menu_is_active(void) {
    return active;
}

static void close_gamestate(void) {
    active = 0;

    clear_image(ui_img_box);
    lv_obj_add_flag(ui_pnl_box, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui_lbl_screen_message, "");

    lv_obj_clear_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);

    list_frame_reset();
    sectioned = 0;
    frame_total = 0;

    nav_show_a(0, NULL);
    nav_show_x(0, NULL);
    nav_show_y(0, NULL);

    pause_menu_rebuild();
    pause_menu_focus_gamestate_item();
    pause_menu_show_nav_hints();

    pause_menu_sync_input_mask();
}

static void start_new_save(void) {
    creating_save = 1;

    play_sound(snd_confirm);

    init_osk(ui_pnl_entry_gamestate, ui_txt_entry_gamestate, 0, 0, GAMESTATE_NAME_MAX - 1);

    key_show = 1;
    osk_show(ui_pnl_entry_gamestate);

    char default_name[RANDNAME_MAX_LEN];
    randname_generate_with_separator(default_name, sizeof(default_name), " ");
    lv_textarea_set_text(ui_txt_entry_gamestate, default_name);
}

static void finish_new_save_confirm(void) {
    if (creating_save) {
        const char *name = lv_textarea_get_text(ui_txt_entry_gamestate);
        const int position = gamestate_create(name);
        if (position >= 0) {
            rebuild_rows();
            focus_row(pinned_row_offset() + position);
            refresh_thumbnail();
        }
    }

    creating_save = 0;
    key_show = 0;
    reset_osk(key_entry);
    lv_textarea_set_text(ui_txt_entry_gamestate, "");
    lv_group_set_focus_cb(ui_group, NULL);
    osk_hide(ui_pnl_entry_gamestate);
}

static void finish_new_save_cancel(void) {
    creating_save = 0;
    close_osk(key_entry, ui_group, ui_txt_entry_gamestate, ui_pnl_entry_gamestate);
}

static void tick_osk(const uint64_t edge, const uint64_t mask) {
    if (edge & BIT(9)) {
        finish_new_save_confirm();
        return;
    }

    if (edge & BIT(4)) {
        play_sound(snd_keypress);
        const char *is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
        if (is_key && strcasecmp(is_key, OSK_DONE) == 0)
            finish_new_save_confirm();
        else
            lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
        return;
    }

    if (edge & BIT(6)) {
        finish_new_save_cancel();
        return;
    }

    if (edge & BIT(7)) {
        key_space(ui_txt_entry_gamestate);
        return;
    }

    if (edge & BIT(8)) {
        key_clear(ui_txt_entry_gamestate);
        return;
    }

    if (edge & NAV_PAGE_UP_BIT) {
        key_swap_back();
        return;
    }

    if (edge & NAV_PAGE_DOWN_BIT) {
        key_swap();
        return;
    }

    const uint32_t now = SDL_GetTicks();

    const int do_up = nav_repeat_step(&rpt_osk_up, edge & BIT(0), mask & BIT(0), 1, now);
    const int do_down = nav_repeat_step(&rpt_osk_down, edge & BIT(1), mask & BIT(1), 1, now);
    const int do_left = nav_repeat_step(&rpt_osk_left, edge & BIT(2), mask & BIT(2), 1, now);
    const int do_right = nav_repeat_step(&rpt_osk_right, edge & BIT(3), mask & BIT(3), 1, now);

    const int do_backspace = nav_repeat_step(&rpt_backspace, edge & BIT(5), mask & BIT(5), 1, now);

    if (do_up)
        key_up();
    else if (do_down)
        key_down();
    else if (do_left)
        key_left();
    else if (do_right)
        key_right();
    else if (do_backspace)
        key_backspace(ui_txt_entry_gamestate);
}

void gamestate_menu_tick(void) {
    if (pending_action != pending_none) {
        const pending_action_t action = pending_action;
        const int index = pending_index;

        pending_action = pending_none;
        pending_index = -1;

        if (action == pending_empty) {
            gamestate_trash_empty();
            rebuild_rows();
            focus_row(0);
            refresh_thumbnail();
            return;
        }

        if (index < 0) return;

        if (action == pending_purge) {
            gamestate_trash_delete(index - state_row_count - history_row_count);
            rebuild_rows();
            focus_row(0);
            refresh_thumbnail();
            return;
        }

        const int qs_offset = quicksave_row_offset();
        const int offset = pinned_row_offset();
        const int timeline_slot = timeline_at_row(index);

        if (action == pending_load) {
            int load_ok;
            if (gamestate_quicksave_exists && index == 0) {
                load_ok = gamestate_quicksave_load() == 0;
            } else if (gamestate_autosave_exists && index == qs_offset) {
                load_ok = gamestate_autosave_load() == 0;
            } else if (timeline_slot >= 0) {
                load_ok = gamestate_timeline_load(timeline_slot) == 0;
            } else {
                load_ok = gamestate_load(index - offset) == 0;
            }
            close_gamestate();
            pause_menu_toggle();
            if (!load_ok) pause_menu_show_toast(lang.muxretro.gamestate.load_failed);
        } else if (action == pending_delete) {
            if (gamestate_quicksave_exists && index == 0) {
                gamestate_quicksave_delete();
                rebuild_rows();

                if (state_row_count > 0) {
                    focus_row(0);
                    refresh_thumbnail();
                }
            } else if (gamestate_autosave_exists && index == qs_offset) {
                gamestate_autosave_delete();
                rebuild_rows();

                if (state_row_count > 0) {
                    focus_row(0);
                    refresh_thumbnail();
                }
            } else if (timeline_slot >= 0) {
                gamestate_timeline_delete(timeline_slot);
                rebuild_rows();

                if (state_row_count > 0) {
                    focus_row(0);
                    refresh_thumbnail();
                }
            } else {
                const int slot_index = index - offset;
                gamestate_delete(slot_index);
                rebuild_rows();

                if (gamestate_slot_count > 0) {
                    const int next_slot_index =
                        slot_index < gamestate_slot_count ? slot_index : gamestate_slot_count - 1;
                    focus_row(pinned_row_offset() + next_slot_index);
                    refresh_thumbnail();
                } else if (state_row_count > 0) {
                    focus_row(0);
                    refresh_thumbnail();
                }
            }
        }
        return;
    }

    const uint64_t mask = current_nav_mask();
    const uint64_t edge = mask & ~prev_nav_mask;
    prev_nav_mask = mask;

    if (nav_input_halted()) return;

    if (key_show) {
        tick_osk(edge, mask);
        return;
    }

    if (dialogue_active(&load_dlg)) {
        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&load_dlg, &theme, (edge & BIT(1)) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const mux_confirm_opt opt = (mux_confirm_opt) load_dlg.selected;
            dialogue_dismiss(&load_dlg);
            if (opt == mux_confirm_yep) {
                pending_action = pending_load;
                pending_index = focused_row();
            }
        } else if (edge & BIT(5)) {
            dialogue_mark_cancelled(&load_dlg);
            dialogue_dismiss(&load_dlg);
        }
        return;
    }

    if (dialogue_active(&delete_dlg)) {
        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&delete_dlg, &theme, (edge & BIT(1)) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const mux_confirm_opt opt = (mux_confirm_opt) delete_dlg.selected;
            dialogue_dismiss(&delete_dlg);
            if (opt == mux_confirm_yep) {
                pending_action = pending_delete;
                pending_index = focused_row();
            }
        } else if (edge & BIT(5)) {
            dialogue_mark_cancelled(&delete_dlg);
            dialogue_dismiss(&delete_dlg);
        }
        return;
    }

    if (dialogue_active(&mismatch_dlg)) {
        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&mismatch_dlg, &theme, (edge & BIT(1)) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const mux_confirm_opt opt = (mux_confirm_opt) mismatch_dlg.selected;
            dialogue_dismiss(&mismatch_dlg);
            if (opt == mux_confirm_yep) {
                pending_action = pending_load;
                pending_index = focused_row();
            }
        } else if (edge & BIT(5)) {
            dialogue_mark_cancelled(&mismatch_dlg);
            dialogue_dismiss(&mismatch_dlg);
        }
        return;
    }

    if (dialogue_active(&purge_dlg)) {
        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&purge_dlg, &theme, (edge & BIT(1)) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const mux_confirm_opt opt = (mux_confirm_opt) purge_dlg.selected;
            dialogue_dismiss(&purge_dlg);
            if (opt == mux_confirm_yep) {
                pending_action = pending_purge;
                pending_index = focused_row();
            }
        } else if (edge & BIT(5)) {
            dialogue_mark_cancelled(&purge_dlg);
            dialogue_dismiss(&purge_dlg);
        }
        return;
    }

    if (dialogue_active(&empty_dlg)) {
        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&empty_dlg, &theme, (edge & BIT(1)) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const mux_confirm_opt opt = (mux_confirm_opt) empty_dlg.selected;
            dialogue_dismiss(&empty_dlg);
            if (opt == mux_confirm_yep) {
                pending_action = pending_empty;
                pending_index = 0;
            }
        } else if (edge & BIT(5)) {
            dialogue_mark_cancelled(&empty_dlg);
            dialogue_dismiss(&empty_dlg);
        }
        return;
    }

    const uint32_t now = SDL_GetTicks();

    int do_up = nav_repeat_step(&rpt_up, edge & BIT(0), mask & BIT(0), current_item_index > 0, now);
    int do_down =
        nav_repeat_step(&rpt_down, edge & BIT(1), mask & BIT(1), current_item_index < ui_count_static - 1, now);

    if (ui_count_static < 2) {
        do_up = 0;
        do_down = 0;
    }

    if (do_up) {
        nav_set_last_dir(nav_dir_up);
        nav_unsuppress_shake();
        gen_step_movement(1, -1, 1, 0, 1);
        refresh_thumbnail();
    } else if (do_down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 1, 0, 1);
        refresh_thumbnail();
    } else if (edge & (NAV_PAGE_UP_BIT | NAV_PAGE_DOWN_BIT)) {
        move_section(edge & NAV_PAGE_UP_BIT ? -1 : +1);
    } else if (edge & (BIT(2) | BIT(3))) {
        if (list_frame_focused()) move_section(edge & BIT(2) ? -1 : +1);
    } else if (edge & BIT(6) && in_trash()) {
        if (focused_row() >= 0) {
            play_sound(snd_confirm);
            dialogue_open(&purge_dlg, &theme);
        }
    } else if (edge & BIT(6)) {
        const int row = in_history() ? -1 : focused_row();

        if (row >= 0 && timeline_slot_is_newest(timeline_at_row(row))) {
            play_sound(snd_error);
            pause_menu_show_toast(lang.muxretro.gamestate.timeline_protected);
        } else if (row >= 0) {
            play_sound(snd_confirm);
            dialogue_open(&delete_dlg, &theme);
        }
    } else if (edge & BIT(7)) {
        if (in_trash()) {
            if (gamestate_trash_count > 0) {
                play_sound(snd_confirm);
                dialogue_open(&empty_dlg, &theme);
            }
        } else if (!in_history() && gamestate_slot_count < GAMESTATE_MAX_SLOTS) {
            start_new_save();
        }
    } else if (edge & BIT(5)) {
        play_sound(snd_back);
        close_gamestate();
    } else if (edge & BIT(4)) {
        const int row = focused_row();
        if (row < 0) return;

        if (in_trash()) {
            play_sound(snd_confirm);

            const int entry = row - state_row_count - history_row_count;
            const int restored = gamestate_trash_restore(entry) == 0;

            rebuild_rows();
            focus_row(0);
            refresh_thumbnail();

            if (!restored) pause_menu_show_toast(lang.muxretro.gamestate.restore_failed);
        } else if (in_history()) {
            play_sound(snd_confirm);

            const int restored = history_restore(row - state_row_count) == 0;

            close_gamestate();
            pause_menu_toggle();
            pause_menu_show_toast(
                restored ? lang.muxretro.gamestate.history_restored : lang.muxretro.gamestate.history_failed
            );
        } else {
            play_sound(snd_confirm);

            const struct gamestate_slot *sel = state_at_row(row);
            if (sel && !gamestate_metadata_matches(sel)) {
                dialogue_open(&mismatch_dlg, &theme);
            } else {
                dialogue_open(&load_dlg, &theme);
            }
        }
    }
}

void gamestate_notice_open(void) {
    notice_prev_mask = current_nav_mask();
    dialogue_open(&notice_dlg, &theme);
}

int gamestate_notice_is_active(void) {
    return dialogue_active(&notice_dlg);
}

void gamestate_notice_tick(void) {
    const uint64_t mask = current_nav_mask();
    const uint64_t edge = mask & ~notice_prev_mask;
    notice_prev_mask = mask;

    if (edge & (BIT(4) | BIT(5))) {
        dialogue_dismiss(&notice_dlg);
        pause_menu_sync_input_mask();
    }
}
