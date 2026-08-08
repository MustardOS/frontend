#include <stdlib.h>
#include "../../common/audio.h"
#include "../../common/input.h"
#include "../../common/ui/common.h"
#include "../../common/ui/dialogue.h"
#include "../../common/ui/osk.h"
#include "../../common/randname.h"
#include "../../module/muxshare.h"
#include "macro.h"
#include "../core/muxretro.h"
#include "../input/nav_repeat.h"
#include "../settings/settings.h"

#define ROW_VALUE_MAX 64

static int active_port = 0;
static int list_active = 0;
static uint64_t prev_nav_mask = 0;

static nav_repeat_t rpt_up = {0};
static nav_repeat_t rpt_down = {0};

static nav_repeat_t rpt_osk_up = {0};
static nav_repeat_t rpt_osk_down = {0};
static nav_repeat_t rpt_osk_left = {0};
static nav_repeat_t rpt_osk_right = {0};
static nav_repeat_t rpt_backspace = {0};

static mux_dialogue delete_dlg;

static mux_dialogue unsaved_dlg;

static mux_dialogue edit_bind_dlg;
static const char *edit_bind_options[2];
enum { edit_bind_edit = 0, edit_bind_bind };

static mux_dialogue add_step_dlg;
static const char *add_step_options[2];
enum { add_step_button = 0, add_step_pause };

static int creating_macro = 0;
static int editing_step_timing = 0;
static int editing_field_index = 0;
static int editing_pause_direct = 0;
static lv_obj_t *ui_pnl_entry_macro = NULL;
static lv_obj_t *ui_txt_entry_macro = NULL;

static int edit_active = 0;
static int editing_position = -1;
static struct macro_entry editor_snapshot;

enum { field_wait = 0, field_hold, field_repeat };

static int field_edit_active = 0;
static int field_edit_step = -1;
static int field_edit_return_index = -1;

static nav_repeat_t rpt_field_up = {0};
static nav_repeat_t rpt_field_down = {0};

typedef enum { capture_none, capture_waiting_press, capture_recording } capture_state_t;
typedef enum { capture_purpose_none, capture_purpose_assign, capture_purpose_add_step } capture_purpose_t;

static capture_state_t capture_state = capture_none;
static capture_purpose_t capture_purpose = capture_purpose_none;
static int capture_source = -1;
static uint64_t capture_prev_mask = 0;
static int capture_prev_b = 0;
static uint16_t chord_mask = 0;

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
    const int del = mux_input_pressed(mux_input_x);
    const int extra = mux_input_pressed(mux_input_y);
    const int select_btn = mux_input_pressed(mux_input_select);
    const int start_btn = mux_input_pressed(mux_input_start);

    return nav_dir_bits() | (confirm ? BIT(4) : 0) | (back ? BIT(5) : 0) | (del ? BIT(6) : 0) | (extra ? BIT(7) : 0)
           | (select_btn ? BIT(8) : 0) | (start_btn ? BIT(9) : 0);
}

static void build_row(const char *label_text, const char *value_text, const char *glyph) {
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value = lv_label_create(panel);

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label, label_text, 1);
    apply_theme_list_glyph(&theme, icon, "muxretro", glyph);
    apply_theme_list_value(&theme, value, value_text);
    apply_size_to_content(&theme, ui_pnl_content, label, icon, label_text);
    apply_text_long_dot(&theme, label);

    lv_group_add_obj(ui_group, label);
    lv_group_add_obj(ui_group_glyph, icon);
    lv_group_add_obj(ui_group_panel, panel);
    lv_group_add_obj(ui_group_value, value);
}

static void focus_row(const int index) {
    if (index < 0 || index >= ui_count_static) return;
    current_item_index = index;

    lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, index);
    if (!panel) return;

    lv_obj_t *label = lv_obj_get_child(panel, 0);
    lv_obj_t *glyph = lv_obj_get_child(panel, 1);
    lv_obj_t *value = lv_obj_get_child(panel, 2);

    nav_suppress_next_shake();

    if (label) lv_group_focus_obj(label);
    if (glyph) lv_group_focus_obj(glyph);
    if (value) lv_group_focus_obj(value);
    lv_group_focus_obj(panel);

    update_scroll_position(
        theme.mux.item.count, theme.mux.item.panel, ui_count_static, current_item_index, ui_pnl_content
    );
}

static void refresh_row_value(const int index, const char *text) {
    lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, index);
    if (!panel) return;

    lv_obj_t *value = lv_obj_get_child(panel, 2);
    if (value) lv_label_set_text(value, text);
}

static void describe_assignment(const int position, char *buf) {
    const int macro_index = macro_list[position].index;

    for (int s = 0; s < PORT_SOURCE_COUNT; s++) {
        if (session_settings.port_source_macro[active_port][s] == macro_index) {
            snprintf(buf, ROW_VALUE_MAX, "%s", session_settings_button_type_label(session_settings_source_types[s]));
            return;
        }
    }

    snprintf(buf, ROW_VALUE_MAX, "%s", lang.muxretro.settings_screen.unbound);
}

static void list_row_value(const int position, char *buf) {
    if (capture_state != capture_none && capture_purpose == capture_purpose_assign && position == current_item_index) {
        snprintf(buf, ROW_VALUE_MAX, "%s", lang.muxretro.settings_screen.assign_control);
        return;
    }

    if (macro_list[position].compile_error[0]) {
        snprintf(buf, ROW_VALUE_MAX, "%s", lang.muxretro.macros.relish_broken);
        return;
    }

    describe_assignment(position, buf);
}

static void format_step_targets(const int target_mask, char *buf) {
    size_t used = 0;
    buf[0] = '\0';

    for (int position = 0; position < PORT_DIGITAL_COUNT && used < ROW_VALUE_MAX; position++) {
        const int t = session_settings_target_at_position(position);
        if (t < 0 || !(target_mask & (1 << t))) continue;

        const int n =
            snprintf(buf + used, ROW_VALUE_MAX - used, "%s%s", used ? "+" : "", session_settings_target_label(t));
        if (n > 0) used += (size_t) n;
    }

    if (buf[0] == '\0') snprintf(buf, ROW_VALUE_MAX, "%s", lang.muxretro.settings_screen.unbound);
}

static void rebuild_list(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;

    for (int i = 0; i < macro_count; i++) {
        char value_text[ROW_VALUE_MAX];
        list_row_value(i, value_text);
        build_row(macro_list[i].name, value_text, macro_list[i].is_relish ? "manual" : "macro");
    }

    ui_count_static = macro_count;
    first_open = 0;

    lv_label_set_text(ui_lbl_screen_message, macro_count > 0 ? "" : lang.muxretro.macros.none_found);
}

static void rebuild_editor_rows(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;

    const int step_count = macro_list[editing_position].step_count;
    for (int i = 0; i < step_count; i++) {
        const struct macro_step *step = &macro_list[editing_position].steps[i];

        char label_text[ROW_VALUE_MAX];
        const char *glyph = "macro";
        if (step->target_mask == 0) {
            snprintf(label_text, sizeof(label_text), "%s", lang.muxretro.macros.pause);
            glyph = "pause";
        } else {
            format_step_targets(step->target_mask, label_text);
        }

        char value_text[ROW_VALUE_MAX];
        macros_step_compact_label(step, value_text, sizeof(value_text));

        build_row(label_text, value_text, glyph);
    }

    ui_count_static = step_count;
    first_open = 0;

    lv_label_set_text(ui_lbl_screen_message, step_count > 0 ? "" : lang.muxretro.macros.no_steps);
}

static void apply_list_nav(void) {
    nav_show_a(1, lang.generic.select);
    nav_show_x(macro_count > 0, lang.muxretro.macros.delete);
    nav_show_y(macro_count < MACRO_MAX, lang.generic.add);
    nav_show_lr(0);
    pause_menu_fix_nav_order();
}

static void apply_editor_nav(void) {
    nav_show_a(ui_count_static > 0, lang.muxretro.macros.timing);
    nav_show_x(ui_count_static > 0, lang.muxretro.macros.delete);
    nav_show_y(macro_list[editing_position].step_count < MACRO_STEP_MAX, lang.generic.add);
    nav_show_lr(0);
    pause_menu_fix_nav_order();
}

static void apply_field_nav(void) {
    nav_show_a(1, lang.muxretro.macros.custom);
    nav_show_x(0, NULL);
    nav_show_y(0, NULL);
    nav_show_lr(1);
    lv_label_set_text(ui_lbl_nav_lr, lang.generic.change);
    pause_menu_fix_nav_order();
}

static void close_editor(void) {
    edit_active = 0;
    editing_position = -1;

    rebuild_list();
    apply_list_nav();

    prev_nav_mask = current_nav_mask();
}

static void open_editor(const int position) {
    editing_position = position;
    editor_snapshot = macro_list[position];
    edit_active = 1;

    rebuild_editor_rows();
    apply_editor_nav();

    prev_nav_mask = current_nav_mask();
}

static void rebuild_field_rows(void) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;

    const struct macro_step *step = &macro_list[editing_position].steps[field_edit_step];

    char repeat_text[16];
    snprintf(repeat_text, sizeof(repeat_text), "%d", step->repeat);

    build_row(lang.muxretro.macros.wait, macros_time_ms_name(step->wait_ms), "pause");
    build_row(lang.muxretro.macros.hold, macros_time_ms_name(step->hold_ms), "controller");
    build_row(lang.muxretro.macros.repeat, repeat_text, "restart");
    ui_count_static = 3;

    first_open = 0;

    lv_label_set_text(ui_lbl_screen_message, "");
}

static void open_field_editor(const int step_pos) {
    field_edit_step = step_pos;
    field_edit_return_index = step_pos;
    field_edit_active = 1;

    rebuild_field_rows();
    apply_field_nav();

    prev_nav_mask = current_nav_mask();
}

static void close_field_editor(void) {
    field_edit_active = 0;
    field_edit_step = -1;

    rebuild_editor_rows();
    apply_editor_nav();
    if (ui_count_static > 0)
        focus_row(field_edit_return_index < ui_count_static ? field_edit_return_index : ui_count_static - 1);

    prev_nav_mask = current_nav_mask();
}

static int editor_is_modified(void) {
    const struct macro_entry *current = &macro_list[editing_position];
    if (current->step_count != editor_snapshot.step_count) return 1;

    return memcmp(current->steps, editor_snapshot.steps, sizeof(struct macro_step) * (size_t) current->step_count) != 0;
}

static void begin_capture(const capture_purpose_t purpose) {
    capture_source = session_settings_resolve_port_source(active_port);
    if (capture_source < 0) {
        pause_menu_show_toast(lang.generic.not_connected);
        return;
    }

    capture_purpose = purpose;
    capture_prev_mask = mux_input_source_pressed_mask(capture_source);
    capture_prev_b = mux_input_pressed(mux_input_b);
    capture_state = capture_waiting_press;
    chord_mask = 0;

    if (purpose == capture_purpose_assign) {
        refresh_row_value(current_item_index, lang.muxretro.settings_screen.assign_control);
    } else {
        lv_label_set_text(ui_lbl_screen_message, lang.muxretro.settings_screen.assign_control);
    }
}

static void end_capture(void) {
    capture_state = capture_none;
    capture_purpose = capture_purpose_none;
    prev_nav_mask = current_nav_mask();
}

static void finish_assign(const mux_input_type pressed) {
    const int row = current_item_index;
    const int source = session_settings_source_for_input(pressed);
    if (source >= 0) {
        session_settings_assign_source_macro(active_port, source, macro_list[row].index);
        play_sound(snd_confirm);
    }

    char value_text[ROW_VALUE_MAX];
    list_row_value(row, value_text);
    refresh_row_value(row, value_text);
}

static void cancel_assign(void) {
    play_sound(snd_back);

    char value_text[ROW_VALUE_MAX];
    list_row_value(current_item_index, value_text);
    refresh_row_value(current_item_index, value_text);
}

static void finish_add_step(const uint16_t target_mask) {
    int new_pos = -1;
    if (target_mask != 0) {
        struct macro_step new_step = {0};
        new_step.kind = macro_step_button;
        new_step.target_mask = target_mask;
        new_step.wait_ms = MACRO_WAIT_MS_DEFAULT;
        new_step.hold_ms = MACRO_HOLD_MS_DEFAULT;
        new_step.repeat = MACRO_REPEAT_DEFAULT;

        new_pos = macros_add_step(editing_position, &new_step);
        play_sound(snd_confirm);
    }

    rebuild_editor_rows();
    apply_editor_nav();
    if (new_pos >= 0) focus_row(new_pos);
}

static void cancel_add_step(void) {
    play_sound(snd_back);
    rebuild_editor_rows();
    apply_editor_nav();
}

static void add_pause_step(void) {
    struct macro_step new_step = {0};
    new_step.kind = macro_step_button;
    new_step.target_mask = 0;
    new_step.wait_ms = MACRO_WAIT_MS_DEFAULT;
    new_step.hold_ms = MACRO_HOLD_MS_DEFAULT;
    new_step.repeat = MACRO_REPEAT_DEFAULT;

    const int new_pos = macros_add_step(editing_position, &new_step);
    if (new_pos >= 0) {
        rebuild_editor_rows();
        apply_editor_nav();
        focus_row(new_pos);
    }
}

static void start_pause_edit(void) {
    if (current_item_index < 0 || current_item_index >= ui_count_static) return;

    field_edit_step = current_item_index;
    field_edit_return_index = current_item_index;
    editing_step_timing = 1;
    editing_pause_direct = 1;

    init_osk(ui_pnl_entry_macro, ui_txt_entry_macro, 1, 0, 10);

    lv_obj_clear_flag(num_entry, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(num_entry, LV_STATE_DISABLED);
    lv_obj_add_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_state(key_entry, LV_STATE_DISABLED);

    key_show = 2;
    osk_show(ui_pnl_entry_macro);

    const struct macro_step *step = &macro_list[editing_position].steps[current_item_index];
    char default_value[16];
    snprintf(default_value, sizeof(default_value), "%d", step->hold_ms);
    lv_textarea_set_text(ui_txt_entry_macro, default_value);
}

static void capture_tick(void) {
    const uint64_t mask = mux_input_source_pressed_mask(capture_source);

    if (capture_purpose == capture_purpose_add_step) {
        const uint64_t new_bits = mask & ~capture_prev_mask;
        capture_prev_mask = mask;

        uint64_t candidate_mask = 0;
        for (int i = 0; i < PORT_DIGITAL_COUNT; i++) {
            const mux_input_type type = (mux_input_type) session_settings_source_types[i];

            if (capture_state == capture_waiting_press && type == mux_input_b) continue;
            candidate_mask |= BIT(type);

            if (new_bits & BIT(type)) {
                const int target = session_settings_target_for_button(type);
                if (target >= 0) chord_mask |= (uint16_t) (1u << target);
                capture_state = capture_recording;
            }
        }

        if (capture_state == capture_recording && !(mask & candidate_mask)) {
            const uint16_t final_mask = chord_mask;
            chord_mask = 0;
            finish_add_step(final_mask);
            end_capture();
            return;
        }

        if (capture_state == capture_waiting_press) {
            const int b_now = mux_input_pressed(mux_input_b);
            const int b_edge = b_now && !capture_prev_b;
            capture_prev_b = b_now;

            if (b_edge) {
                cancel_add_step();
                end_capture();
            }
        }

        return;
    }

    if (capture_state == capture_waiting_press) {
        const uint64_t new_bits = mask & ~capture_prev_mask;
        capture_prev_mask = mask;

        for (int i = 0; i < PORT_SOURCE_COUNT; i++) {
            const mux_input_type pressed = (mux_input_type) session_settings_source_types[i];
            if (pressed == mux_input_b) continue;

            if (new_bits & BIT(pressed)) {
                end_capture();
                finish_assign(pressed);
                return;
            }
        }

        const int b_now = mux_input_pressed(mux_input_b);
        const int b_edge = b_now && !capture_prev_b;
        capture_prev_b = b_now;

        if (b_edge) {
            cancel_assign();
            end_capture();
        }
    }
}

static void create_osk_objects(void) {
    ui_pnl_entry_macro = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnl_entry_macro, device.mux.width);
    lv_obj_set_height(ui_pnl_entry_macro, device.mux.height);
    lv_obj_set_align(ui_pnl_entry_macro, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnl_entry_macro, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_pnl_entry_macro, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnl_entry_macro, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_entry_macro, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnl_entry_macro, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_entry_macro, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_entry_macro, 128, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_entry_macro, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnl_entry_macro, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnl_entry_macro, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnl_entry_macro, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnl_entry_macro, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnl_entry_macro, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnl_entry_macro, 5, MU_OBJ_MAIN_DEFAULT);

    ui_txt_entry_macro = lv_textarea_create(ui_pnl_entry_macro);
    lv_obj_set_width(ui_txt_entry_macro, device.mux.width * 5 / 6);
    lv_obj_set_height(ui_txt_entry_macro, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_txt_entry_macro, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ui_txt_entry_macro, MACRO_NAME_MAX - 1);
    lv_textarea_set_one_line(ui_txt_entry_macro, 1);
    lv_obj_set_style_radius(ui_txt_entry_macro, theme.osk.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_txt_entry_macro, lv_color_hex(theme.osk.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_txt_entry_macro, theme.osk.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_txt_entry_macro, 2, MU_OBJ_MAIN_DEFAULT);
}

static void start_new_macro(void) {
    creating_macro = 1;

    play_sound(snd_confirm);

    init_osk(ui_pnl_entry_macro, ui_txt_entry_macro, 0, 0, MACRO_NAME_MAX - 1);

    key_show = 1;
    osk_show(ui_pnl_entry_macro);

    char default_name[RANDNAME_MAX_LEN];
    randname_generate_with_separator(default_name, sizeof(default_name), " ");
    lv_textarea_set_text(ui_txt_entry_macro, default_name);
}

static void finish_new_macro_confirm(void) {
    if (creating_macro) {
        const char *name = lv_textarea_get_text(ui_txt_entry_macro);
        const int position = macros_create(name);
        if (position >= 0) {
            rebuild_list();
            apply_list_nav();
            focus_row(position);
        }
    }

    creating_macro = 0;
    key_show = 0;
    reset_osk(key_entry);
    lv_textarea_set_text(ui_txt_entry_macro, "");
    lv_group_set_focus_cb(ui_group, NULL);
    osk_hide(ui_pnl_entry_macro);
}

static void finish_new_macro_cancel(void) {
    creating_macro = 0;
    close_osk(key_entry, ui_group, ui_txt_entry_macro, ui_pnl_entry_macro);
}

static void start_field_value_edit(void) {
    if (current_item_index < 0 || current_item_index >= ui_count_static) return;

    editing_step_timing = 1;
    editing_field_index = current_item_index;
    play_sound(snd_confirm);

    init_osk(ui_pnl_entry_macro, ui_txt_entry_macro, 1, 0, 10);

    lv_obj_clear_flag(num_entry, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(num_entry, LV_STATE_DISABLED);
    lv_obj_add_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_state(key_entry, LV_STATE_DISABLED);

    key_show = 2;
    osk_show(ui_pnl_entry_macro);

    const struct macro_step *step = &macro_list[editing_position].steps[field_edit_step];
    const int current_value = editing_field_index == field_wait   ? step->wait_ms
                              : editing_field_index == field_hold ? step->hold_ms
                                                                  : step->repeat;

    char default_value[16];
    snprintf(default_value, sizeof(default_value), "%d", current_value);
    lv_textarea_set_text(ui_txt_entry_macro, default_value);
}

static void finish_step_timing_confirm(void) {
    if (editing_step_timing && field_edit_step >= 0) {
        double value = atof(lv_textarea_get_text(ui_txt_entry_macro));
        if (value < 0.0) value = 0.0;
        if (value > 65535.0) value = 65535.0;

        const struct macro_step *step = &macro_list[editing_position].steps[field_edit_step];

        if (editing_pause_direct) {
            const int hold_ms = (int) (value + 0.5);
            macros_set_step_hold_ms(editing_position, field_edit_step, hold_ms);

            char value_text[ROW_VALUE_MAX];
            macros_step_compact_label(step, value_text, sizeof(value_text));
            refresh_row_value(field_edit_step, value_text);
        } else if (editing_field_index == field_wait) {
            const int wait_ms = (int) (value + 0.5);
            macros_set_step_wait_ms(editing_position, field_edit_step, wait_ms);
            refresh_row_value(field_wait, macros_time_ms_name(step->wait_ms));
        } else if (editing_field_index == field_hold) {
            const int hold_ms = (int) (value + 0.5);
            macros_set_step_hold_ms(editing_position, field_edit_step, hold_ms);
            refresh_row_value(field_hold, macros_time_ms_name(step->hold_ms));
        } else {
            int repeat = (int) (value + 0.5);
            if (repeat < 1) repeat = 1;
            macros_set_step_repeat(editing_position, field_edit_step, repeat);

            char repeat_text[16];
            snprintf(repeat_text, sizeof(repeat_text), "%d", step->repeat);
            refresh_row_value(field_repeat, repeat_text);
        }
    }

    editing_pause_direct = 0;
    editing_step_timing = 0;
    key_show = 0;
    reset_osk(num_entry);
    lv_textarea_set_text(ui_txt_entry_macro, "");
    lv_group_set_focus_cb(ui_group, NULL);
    osk_hide(ui_pnl_entry_macro);
}

static void finish_step_timing_cancel(void) {
    editing_pause_direct = 0;
    editing_step_timing = 0;
    close_osk(num_entry, ui_group, ui_txt_entry_macro, ui_pnl_entry_macro);
}

static void tick_timing_osk(const uint64_t edge, const uint64_t mask) {
    if (edge & BIT(9)) {
        finish_step_timing_confirm();
        return;
    }

    if (edge & BIT(4)) {
        play_sound(snd_keypress);
        const char *is_key = lv_btnmatrix_get_btn_text(num_entry, key_curr);
        if (is_key && strcasecmp(is_key, OSK_DONE) == 0)
            finish_step_timing_confirm();
        else
            lv_event_send(num_entry, LV_EVENT_CLICKED, &key_curr);
        return;
    }

    if (edge & BIT(6)) {
        finish_step_timing_cancel();
        return;
    }

    if (edge & BIT(8)) {
        key_clear(ui_txt_entry_macro);
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
        key_backspace(ui_txt_entry_macro);
}

static void tick_osk(const uint64_t edge, const uint64_t mask) {
    if (edge & BIT(9)) {
        finish_new_macro_confirm();
        return;
    }

    if (edge & BIT(4)) {
        play_sound(snd_keypress);
        const char *is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
        if (is_key && strcasecmp(is_key, OSK_DONE) == 0)
            finish_new_macro_confirm();
        else
            lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
        return;
    }

    if (edge & BIT(6)) {
        finish_new_macro_cancel();
        return;
    }

    if (edge & BIT(7)) {
        key_space(ui_txt_entry_macro);
        return;
    }

    if (edge & BIT(8)) {
        key_clear(ui_txt_entry_macro);
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
        key_backspace(ui_txt_entry_macro);
}

void macros_menu_init(void) {
    dialogue_init_confirm(
        &delete_dlg, &theme, ui_screen, lang.muxretro.macros.delete_title, lang.muxretro.macros.delete_desc,
        lang.muxretro.macros.delete, lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );
    dialogue_init_unsaved(
        &unsaved_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.cancel
    );

    edit_bind_options[edit_bind_edit] = lang.generic.edit;
    edit_bind_options[edit_bind_bind] = lang.muxretro.macros.bind;
    dialogue_init(
        &edit_bind_dlg, &theme, ui_screen, lang.muxretro.macros.edit_bind_title, NULL, edit_bind_options, 2,
        lang.generic.select, lang.generic.cancel
    );

    add_step_options[add_step_button] = lang.muxretro.macros.button;
    add_step_options[add_step_pause] = lang.muxretro.macros.pause;
    dialogue_init(
        &add_step_dlg, &theme, ui_screen, lang.muxretro.macros.add_title, NULL, add_step_options, 2,
        lang.generic.select, lang.generic.cancel
    );

    create_osk_objects();
}

void macros_menu_open(const int port) {
    if (port < 0 || port >= MUX_INPUT_PORT_COUNT) return;

    active_port = port;
    list_active = 1;
    edit_active = 0;
    field_edit_active = 0;
    field_edit_step = -1;
    add_step_dlg.active = 0;
    capture_state = capture_none;
    capture_purpose = capture_purpose_none;
    chord_mask = 0;

    prev_nav_mask = current_nav_mask();

    rebuild_list();
    apply_list_nav();
}

int macros_menu_is_active(void) {
    return list_active;
}

static void close_macros(void) {
    list_active = 0;
    edit_active = 0;

    nav_show_a(0, NULL);
    nav_show_x(0, NULL);
    nav_show_y(0, NULL);

    lv_label_set_text(ui_lbl_screen_message, "");

    input_port_menu_reopen_macros(active_port);
}

void macros_menu_tick(void) {
    if (!list_active) return;

    if (capture_state != capture_none) {
        capture_tick();
        return;
    }

    if (key_show) {
        const uint64_t mask = current_nav_mask();
        const uint64_t edge = mask & ~prev_nav_mask;
        prev_nav_mask = mask;
        if (editing_step_timing) {
            tick_timing_osk(edge, mask);
        } else {
            tick_osk(edge, mask);
        }
        return;
    }

    if (dialogue_active(&delete_dlg)) {
        const uint64_t mask = current_nav_mask();
        const uint64_t edge = mask & ~prev_nav_mask;
        prev_nav_mask = mask;

        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&delete_dlg, &theme, (edge & BIT(1)) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const mux_confirm_opt opt = (mux_confirm_opt) delete_dlg.selected;
            dialogue_dismiss(&delete_dlg);

            if (opt == mux_confirm_yep && current_item_index >= 0 && current_item_index < macro_count) {
                const int macro_index = macro_list[current_item_index].index;
                const int next_focus = current_item_index;

                macros_delete(current_item_index);
                session_settings_clear_macro_references(macro_index);

                rebuild_list();
                apply_list_nav();

                if (macro_count > 0) focus_row(next_focus < macro_count ? next_focus : macro_count - 1);
            }
        } else if (edge & BIT(5)) {
            dialogue_mark_cancelled(&delete_dlg);
            dialogue_dismiss(&delete_dlg);
        }

        return;
    }

    if (dialogue_active(&unsaved_dlg)) {
        const uint64_t mask = current_nav_mask();
        const uint64_t edge = mask & ~prev_nav_mask;
        prev_nav_mask = mask;

        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&unsaved_dlg, &theme, (edge & BIT(1)) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const mux_unsaved_opt opt = (mux_unsaved_opt) unsaved_dlg.selected;
            dialogue_dismiss(&unsaved_dlg);

            if (opt == mux_unsaved_save) {
                macros_save(editing_position);
                close_editor();
            } else if (opt == mux_unsaved_discard) {
                macro_list[editing_position] = editor_snapshot;
                close_editor();
            }
        } else if (edge & BIT(5)) {
            dialogue_mark_cancelled(&unsaved_dlg);
            dialogue_dismiss(&unsaved_dlg);
        }

        return;
    }

    if (dialogue_active(&edit_bind_dlg)) {
        const uint64_t mask = current_nav_mask();
        const uint64_t edge = mask & ~prev_nav_mask;
        prev_nav_mask = mask;

        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&edit_bind_dlg, &theme, (edge & BIT(1)) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const int opt = edit_bind_dlg.selected;
            dialogue_dismiss(&edit_bind_dlg);

            if (opt == edit_bind_edit) {
                open_editor(current_item_index);
            } else if (opt == edit_bind_bind) {
                begin_capture(capture_purpose_assign);
            }
        } else if (edge & BIT(5)) {
            dialogue_mark_cancelled(&edit_bind_dlg);
            dialogue_dismiss(&edit_bind_dlg);
        }

        return;
    }

    if (dialogue_active(&add_step_dlg)) {
        const uint64_t mask = current_nav_mask();
        const uint64_t edge = mask & ~prev_nav_mask;
        prev_nav_mask = mask;

        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&add_step_dlg, &theme, (edge & BIT(1)) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const int opt = add_step_dlg.selected;
            dialogue_dismiss(&add_step_dlg);

            if (opt == add_step_button) {
                begin_capture(capture_purpose_add_step);
            } else if (opt == add_step_pause) {
                add_pause_step();
            }
        } else if (edge & BIT(5)) {
            dialogue_mark_cancelled(&add_step_dlg);
            dialogue_dismiss(&add_step_dlg);
        }

        return;
    }

    if (field_edit_active) {
        const uint64_t mask = current_nav_mask();
        const uint64_t edge = mask & ~prev_nav_mask;
        prev_nav_mask = mask;

        if (nav_input_halted()) return;

        const uint32_t now = SDL_GetTicks();

        const int do_up = nav_repeat_step(&rpt_field_up, edge & BIT(0), mask & BIT(0), current_item_index > 0, now);
        const int do_down = nav_repeat_step(
            &rpt_field_down, edge & BIT(1), mask & BIT(1), current_item_index < ui_count_static - 1, now
        );
        const int do_left = edge & BIT(2);
        const int do_right = edge & BIT(3);

        struct macro_step *step = &macro_list[editing_position].steps[field_edit_step];

        if (do_up) {
            nav_set_last_dir(nav_dir_up);
            nav_unsuppress_shake();
            gen_step_movement(1, -1, 1, 0, 1);
        } else if (do_down) {
            nav_set_last_dir(nav_dir_down);
            nav_unsuppress_shake();
            gen_step_movement(1, +1, 1, 0, 1);
        } else if (do_left || do_right) {
            const int direction = do_right ? 1 : -1;

            if (current_item_index == field_wait) {
                step->wait_ms = macros_cycle_step_time_ms(step->wait_ms, direction);
                refresh_row_value(field_wait, macros_time_ms_name(step->wait_ms));
            } else if (current_item_index == field_hold) {
                step->hold_ms = macros_cycle_step_time_ms(step->hold_ms, direction);
                refresh_row_value(field_hold, macros_time_ms_name(step->hold_ms));
            } else {
                step->repeat = macros_cycle_step_repeat(step->repeat, direction);
                char repeat_text[16];
                snprintf(repeat_text, sizeof(repeat_text), "%d", step->repeat);
                refresh_row_value(field_repeat, repeat_text);
            }
            play_sound(snd_option);
        } else if (edge & BIT(4)) {
            start_field_value_edit();
        } else if (edge & BIT(5)) {
            play_sound(snd_back);
            close_field_editor();
        }

        return;
    }

    const uint64_t mask = current_nav_mask();
    const uint64_t edge = mask & ~prev_nav_mask;
    prev_nav_mask = mask;

    if (nav_input_halted()) return;

    const uint32_t now = SDL_GetTicks();

    const int do_up = nav_repeat_step(&rpt_up, edge & BIT(0), mask & BIT(0), current_item_index > 0, now);
    const int do_down =
        nav_repeat_step(&rpt_down, edge & BIT(1), mask & BIT(1), current_item_index < ui_count_static - 1, now);
    if (do_up) {
        nav_set_last_dir(nav_dir_up);
        nav_unsuppress_shake();
        gen_step_movement(1, -1, 1, 0, 1);
    } else if (do_down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 1, 0, 1);
    } else if (edge & BIT(6)) {
        if (edit_active) {
            if (ui_count_static > 0) {
                const int next_focus = current_item_index;
                macros_remove_step(editing_position, current_item_index);
                play_sound(snd_confirm);
                rebuild_editor_rows();
                apply_editor_nav();

                const int step_count = macro_list[editing_position].step_count;
                if (step_count > 0) focus_row(next_focus < step_count ? next_focus : step_count - 1);
            }
        } else if (macro_count > 0) {
            play_sound(snd_confirm);
            dialogue_open(&delete_dlg, &theme);
        }
    } else if (edge & BIT(7)) {
        if (edit_active) {
            if (macro_list[editing_position].step_count < MACRO_STEP_MAX) {
                play_sound(snd_confirm);
                dialogue_open(&add_step_dlg, &theme);
            }
        } else if (macro_count < MACRO_MAX) {
            start_new_macro();
        }
    } else if (edge & BIT(4)) {
        if (edit_active) {
            if (ui_count_static > 0) {
                play_sound(snd_confirm);
                if (macro_list[editing_position].steps[current_item_index].target_mask == 0) {
                    start_pause_edit();
                } else {
                    open_field_editor(current_item_index);
                }
            }
        } else if (ui_count_static > 0) {
            if (macro_list[current_item_index].compile_error[0]) {
                play_sound(snd_error);
                pause_menu_show_toast(macro_list[current_item_index].compile_error);
            } else if (macro_list[current_item_index].is_relish) {
                play_sound(snd_confirm);
                begin_capture(capture_purpose_assign);
            } else {
                play_sound(snd_confirm);
                dialogue_open(&edit_bind_dlg, &theme);
            }
        }
    } else if (edge & BIT(5)) {
        if (edit_active) {
            if (editor_is_modified()) {
                play_sound(snd_confirm);
                dialogue_open(&unsaved_dlg, &theme);
            } else {
                play_sound(snd_back);
                close_editor();
            }
        } else {
            play_sound(snd_back);
            close_macros();
        }
    }
}
