#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "../../common/input.h"
#include "../../common/ui/common.h"
#include "../../common/ui/dialogue.h"
#include "../../common/ui/osk.h"
#include "../../module/muxshare.h"
#include "cheats.h"
#include "../core/muxretro.h"
#include "../input/nav_repeat.h"

static int active = 0;
static uint64_t prev_nav_mask = 0;

static nav_repeat_t rpt_up = {0};
static nav_repeat_t rpt_down = {0};
static nav_repeat_t rpt_left = {0};
static nav_repeat_t rpt_right = {0};
static nav_repeat_t rpt_osk_up = {0};
static nav_repeat_t rpt_osk_down = {0};
static nav_repeat_t rpt_osk_left = {0};
static nav_repeat_t rpt_osk_right = {0};
static nav_repeat_t rpt_osk_backspace = {0};

static mux_dialogue manage_dlg;
static mux_dialogue delete_dlg;
static const char *manage_options[2];
enum { manage_edit = 0, manage_delete };

typedef enum { editor_closed = 0, editor_name, editor_code } editor_stage;

static editor_stage editor = editor_closed;
static int editing_index = -1;
static char draft_name[CHEAT_DESC_MAX];
static char draft_code[CHEAT_CODE_MAX];
static lv_obj_t *entry_panel;
static lv_obj_t *entry_heading;
static lv_obj_t *entry_text;
static uint64_t modal_prev_mask;

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

static uint64_t current_nav_mask(void) {
    const int confirm = mux_input_pressed(mux_input_a);
    const int back = mux_input_pressed(mux_input_b);
    const int manage = mux_input_pressed(mux_input_x);
    const int add = mux_input_pressed(mux_input_y);
    const int select_btn = mux_input_pressed(mux_input_select);
    const int start_btn = mux_input_pressed(mux_input_start);

    return nav_dir_bits() | (confirm ? BIT(4) : 0) | (back ? BIT(5) : 0) | (manage ? BIT(6) : 0)
           | (add ? BIT(7) : 0) | (select_btn ? BIT(8) : 0) | (start_btn ? BIT(9) : 0) | nav_mask_page();
}

static const char *row_value_text(const int row) {
    return cheats_list[row].enabled ? lang.generic.enabled : lang.generic.disabled;
}

static void build_cheat_row(const int row) {
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value = lv_label_create(panel);

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label, cheats_list[row].desc, 1);
    apply_theme_list_glyph(&theme, icon, "muxretro", "cheat");
    apply_theme_list_value(&theme, value, row_value_text(row));
    apply_size_to_content(&theme, ui_pnl_content, label, icon, cheats_list[row].desc);
    apply_text_long_dot(&theme, label);

    lv_group_add_obj(ui_group, label);
    lv_group_add_obj(ui_group_glyph, icon);
    lv_group_add_obj(ui_group_panel, panel);
    lv_group_add_obj(ui_group_value, value);
}

static void refresh_row(const int row, const enum nav_direction shake_dir) {
    lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, row);
    if (!panel) return;

    lv_obj_t *value = lv_obj_get_child(panel, 2);
    if (!value) return;

    lv_label_set_text(value, row_value_text(row));
    nav_play_shake(value, shake_dir);
}

static void apply_list_nav(void) {
    nav_show_a(cheats_count > 0, lang.muxretro.cheats_screen.toggle);
    nav_show_x(cheats_count > 0, lang.muxretro.cheats_screen.manage);
    nav_show_y(cheats_count < CHEAT_MAX, lang.generic.add);
    nav_show_lr(cheats_count > 0);
    if (cheats_count > 0) lv_label_set_text(ui_lbl_nav_lr, lang.generic.change);

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
}

static void rebuild_cheat_rows_at(const int focus) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;

    for (int row = 0; row < cheats_count; row++)
        build_cheat_row(row);

    ui_count_static = cheats_count;
    first_open = 0;

    lv_label_set_text(ui_lbl_screen_message, cheats_count > 0 ? "" : lang.muxretro.cheats_screen.empty_help);

    if (cheats_count > 0 && focus > 0) {
        const int wanted = focus < cheats_count ? focus : cheats_count - 1;
        gen_step_movement(0, wanted, 0, 0, 1);
    }

    apply_list_nav();
}

static void rebuild_cheat_rows(void) {
    rebuild_cheat_rows_at(0);
}

static void close_cheats(void) {
    active = 0;
    lv_label_set_text(ui_lbl_screen_message, "");
    nav_show_x(0, NULL);
    nav_show_y(0, NULL);

    pause_menu_rebuild();
    pause_menu_focus_cheats_item();
    pause_menu_show_nav_hints();
    pause_menu_sync_input_mask();
}

static void create_editor_objects(void) {
    entry_panel = lv_obj_create(ui_screen);
    lv_obj_set_width(entry_panel, device.mux.width);
    lv_obj_set_height(entry_panel, device.mux.height);
    lv_obj_set_align(entry_panel, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(entry_panel, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(entry_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(entry_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(entry_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(entry_panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(entry_panel, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(entry_panel, 192, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(entry_panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(entry_panel, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(entry_panel, 5, MU_OBJ_MAIN_DEFAULT);

    entry_heading = lv_label_create(entry_panel);
    lv_obj_set_width(entry_heading, device.mux.width * 5 / 6);
    lv_obj_set_style_text_align(entry_heading, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(entry_heading, lv_color_hex(theme.osk.text), MU_OBJ_MAIN_DEFAULT);

    entry_text = lv_textarea_create(entry_panel);
    lv_obj_set_width(entry_text, device.mux.width * 5 / 6);
    lv_obj_set_height(entry_text, LV_SIZE_CONTENT);
    lv_obj_set_align(entry_text, LV_ALIGN_CENTER);
    lv_textarea_set_one_line(entry_text, 1);
    lv_obj_set_style_radius(entry_text, theme.osk.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(entry_text, lv_color_hex(theme.osk.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(entry_text, theme.osk.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(entry_text, 2, MU_OBJ_MAIN_DEFAULT);
}

static void trim_entry(char *value) {
    char *start = value;
    while (*start == ' ' || *start == '\t')
        start++;
    if (start != value) memmove(value, start, strlen(start) + 1);

    size_t length = strlen(value);
    while (length > 0 && (value[length - 1] == ' ' || value[length - 1] == '\t'))
        value[--length] = '\0';
}

static void open_editor_stage(const editor_stage stage, const char *seed) {
    editor = stage;
    lv_label_set_text(
        entry_heading, stage == editor_name ? lang.muxretro.cheats_screen.name : lang.muxretro.cheats_screen.code
    );
    init_osk(
        entry_panel, entry_text, 0, 0,
        (uint16_t) ((stage == editor_name ? CHEAT_DESC_MAX : CHEAT_CODE_MAX) - 1)
    );
    key_show = 1;
    osk_show(entry_panel);
    lv_textarea_set_text(entry_text, seed ? seed : "");
    modal_prev_mask = current_nav_mask();
}

static void start_editor(const int index) {
    if (index < -1 || index >= cheats_count) return;
    if (index < 0 && cheats_count >= CHEAT_MAX) {
        pause_menu_show_toast(lang.muxretro.cheats_screen.limit_reached);
        return;
    }

    editing_index = index;
    if (index >= 0) {
        snprintf(draft_name, sizeof(draft_name), "%s", cheats_list[index].desc);
        snprintf(draft_code, sizeof(draft_code), "%s", cheats_list[index].code);
    } else {
        draft_name[0] = '\0';
        draft_code[0] = '\0';
    }

    open_editor_stage(editor_name, draft_name);
}

static void hide_editor(void) {
    editor = editor_closed;
    key_show = 0;
    reset_osk(key_entry);
    lv_textarea_set_text(entry_text, "");
    lv_group_set_focus_cb(ui_group, NULL);
    osk_hide(entry_panel);
}

static void cancel_editor(void) {
    const int focus = editing_index;
    editing_index = -1;
    close_osk(key_entry, ui_group, entry_text, entry_panel);
    editor = editor_closed;
    rebuild_cheat_rows_at(focus);
    prev_nav_mask = current_nav_mask();
}

static void finish_editor_stage(void) {
    const char *text = lv_textarea_get_text(entry_text);

    if (editor == editor_name) {
        snprintf(draft_name, sizeof(draft_name), "%s", text ? text : "");
        trim_entry(draft_name);
        if (!draft_name[0]) {
            pause_menu_show_toast(lang.muxretro.cheats_screen.name_needed);
            return;
        }

        open_editor_stage(editor_code, draft_code);
        return;
    }

    snprintf(draft_code, sizeof(draft_code), "%s", text ? text : "");
    trim_entry(draft_code);
    if (!draft_code[0]) {
        pause_menu_show_toast(lang.muxretro.cheats_screen.code_needed);
        return;
    }

    const int focus = editing_index < 0 ? cheats_count : editing_index;
    const int saved = editing_index < 0 ? cheats_create(draft_name, draft_code)
                                        : cheats_update(editing_index, draft_name, draft_code);
    hide_editor();
    editing_index = -1;
    rebuild_cheat_rows_at(focus);
    pause_menu_show_toast(saved ? lang.muxretro.cheats_screen.saved : lang.muxretro.cheats_screen.save_failed);
    prev_nav_mask = current_nav_mask();
}

static lv_obj_t *active_keyboard(void) {
    return key_show == 2 && num_entry ? num_entry : key_entry;
}

static void tick_editor(const uint64_t edge, const uint64_t mask) {
    if (edge & BIT(9)) {
        finish_editor_stage();
        return;
    }

    if (edge & BIT(4)) {
        play_sound(snd_keypress);
        lv_obj_t *keyboard = active_keyboard();
        const char *key = keyboard ? lv_btnmatrix_get_btn_text(keyboard, key_curr) : NULL;
        if (key && strcasecmp(key, OSK_DONE) == 0)
            finish_editor_stage();
        else if (keyboard)
            lv_event_send(keyboard, LV_EVENT_CLICKED, &key_curr);
        return;
    }

    if (edge & BIT(6)) {
        cancel_editor();
        return;
    }

    if (edge & BIT(7)) {
        key_space(entry_text);
        return;
    }

    if (edge & BIT(8)) {
        key_clear(entry_text);
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
    const int up = nav_repeat_step(&rpt_osk_up, edge & BIT(0), mask & BIT(0), 1, now);
    const int down = nav_repeat_step(&rpt_osk_down, edge & BIT(1), mask & BIT(1), 1, now);
    const int left = nav_repeat_step(&rpt_osk_left, edge & BIT(2), mask & BIT(2), 1, now);
    const int right = nav_repeat_step(&rpt_osk_right, edge & BIT(3), mask & BIT(3), 1, now);
    const int backspace = nav_repeat_step(&rpt_osk_backspace, edge & BIT(5), mask & BIT(5), 1, now);

    if (up)
        key_up();
    else if (down)
        key_down();
    else if (left)
        key_left();
    else if (right)
        key_right();
    else if (backspace)
        key_backspace(entry_text);
}

static void apply_delete(void) {
    const int focus = current_item_index;
    if (cheats_delete(focus)) {
        rebuild_cheat_rows_at(focus);
        pause_menu_show_toast(lang.muxretro.cheats_screen.deleted);
    } else {
        pause_menu_show_toast(lang.muxretro.cheats_screen.save_failed);
    }
}

static int modal_tick(void) {
    if (editor == editor_closed && !dialogue_active(&manage_dlg) && !dialogue_active(&delete_dlg)) return 0;

    const uint64_t mask = current_nav_mask();
    const uint64_t edge = mask & ~modal_prev_mask;
    modal_prev_mask = mask;

    if (editor != editor_closed) {
        tick_editor(edge, mask);
        return 1;
    }

    if (dialogue_active(&manage_dlg)) {
        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&manage_dlg, &theme, edge & BIT(1) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const int choice = manage_dlg.selected;
            dialogue_dismiss(&manage_dlg);
            if (choice == manage_edit) {
                start_editor(current_item_index);
            } else {
                dialogue_open(&delete_dlg, &theme);
                modal_prev_mask = current_nav_mask();
            }
        } else if (edge & BIT(5)) {
            dialogue_mark_cancelled(&manage_dlg);
            dialogue_dismiss(&manage_dlg);
            prev_nav_mask = mask;
        }
        return 1;
    }

    if (edge & (BIT(0) | BIT(1))) {
        dialogue_handle_dpad(&delete_dlg, &theme, edge & BIT(1) ? 1 : -1, 1);
    } else if (edge & BIT(4)) {
        const mux_confirm_opt option = (mux_confirm_opt) delete_dlg.selected;
        dialogue_dismiss(&delete_dlg);
        if (option == mux_confirm_yep)
            apply_delete();
        else
            prev_nav_mask = mask;
    } else if (edge & BIT(5)) {
        dialogue_mark_cancelled(&delete_dlg);
        dialogue_dismiss(&delete_dlg);
        prev_nav_mask = mask;
    }

    return 1;
}

void cheats_menu_init(void) {
    manage_options[manage_edit] = lang.muxretro.cheats_screen.edit;
    manage_options[manage_delete] = lang.muxretro.cheats_screen.delete;
    dialogue_init(
        &manage_dlg, &theme, ui_screen, lang.muxretro.cheats_screen.manage, NULL, manage_options, 2,
        lang.generic.select, lang.generic.cancel
    );
    dialogue_init_confirm(
        &delete_dlg, &theme, ui_screen, lang.muxretro.cheats_screen.delete_title,
        lang.muxretro.cheats_screen.delete_desc, lang.muxretro.cheats_screen.delete, lang.generic.cancel,
        lang.generic.select, lang.generic.cancel
    );
    delete_dlg.safe_default = mux_confirm_nah;
    create_editor_objects();
}

void cheats_menu_open(void) {
    active = 1;
    editor = editor_closed;
    editing_index = -1;
    prev_nav_mask = current_nav_mask();
    rebuild_cheat_rows();
}

int cheats_menu_is_active(void) {
    return active;
}

void cheats_menu_tick(void) {
    if (modal_tick()) return;

    const uint64_t mask = current_nav_mask();
    const uint64_t edge = mask & ~prev_nav_mask;
    prev_nav_mask = mask;

    if (nav_input_halted()) return;

    const uint32_t now = SDL_GetTicks();
    int do_up = nav_repeat_step(&rpt_up, edge & BIT(0), mask & BIT(0), current_item_index > 0, now);
    int do_down =
        nav_repeat_step(&rpt_down, edge & BIT(1), mask & BIT(1), current_item_index < ui_count_static - 1, now);
    const int do_left = nav_repeat_step(&rpt_left, edge & BIT(2), mask & BIT(2), cheats_count > 0, now);
    const int do_right = nav_repeat_step(&rpt_right, edge & BIT(3), mask & BIT(3), cheats_count > 0, now);

    if (ui_count_static < 2) {
        do_up = 0;
        do_down = 0;
    }

    if (do_up) {
        nav_set_last_dir(nav_dir_up);
        nav_unsuppress_shake();
        gen_step_movement(1, -1, 2, 0, 1);
    } else if (do_down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 2, 0, 1);
    } else if ((do_left || do_right || (edge & BIT(4))) && cheats_count > 0) {
        play_sound(snd_option);
        if (cheats_toggle(current_item_index))
            refresh_row(current_item_index, do_left ? nav_dir_left : nav_dir_right);
        else
            pause_menu_show_toast(lang.muxretro.cheats_screen.save_failed);
    } else if ((edge & BIT(6)) && cheats_count > 0) {
        play_sound(snd_confirm);
        dialogue_open(&manage_dlg, &theme);
        modal_prev_mask = current_nav_mask();
    } else if (edge & BIT(7)) {
        play_sound(snd_confirm);
        start_editor(-1);
    } else if (nav_page_tick(edge, mask, 2)) {
        // Page navigation is handled by the shared list helper.
    } else if (edge & BIT(5)) {
        play_sound(snd_back);
        close_cheats();
    }
}
