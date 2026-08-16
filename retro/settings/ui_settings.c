#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "../../common/audio.h"
#include "../../common/input.h"
#include "../../common/randname.h"
#include "../../common/ui/osk.h"
#include "../../module/muxshare.h"
#include "../netplay/netplay.h"
#include "../core/muxretro.h"
#include "pages.h"
#include "submenu.h"

enum { settings_row_limit = 96, settings_frame_limit = 12 };

typedef enum { settings_special_none = 0, settings_special_core_options, settings_special_save } settings_special;

static const char *row_labels[settings_row_limit];
static const char *row_glyphs[settings_row_limit];
static const char *row_help[settings_row_limit];
static const submenu_def *row_definitions[settings_row_limit];
static int row_local_indices[settings_row_limit];
static settings_special row_specials[settings_row_limit];
static int row_count;
static list_frame settings_frames[settings_frame_limit];
static int settings_frame_count;

static submenu self;
static submenu_def profile_def;

enum { profile_row_limit = play_profile_count + SESSION_USER_PROFILE_LIMIT + 1 };

static const char *profile_labels[profile_row_limit];
static const char *profile_glyphs[profile_row_limit];
static const char *profile_help[profile_row_limit];
static int profile_row_count;
static int profile_user_count;
static int profile_save_row = -1;
static mux_dialogue profile_scope_dlg;
static mux_dialogue profile_delete_dlg;
static lv_obj_t *profile_entry_panel;
static lv_obj_t *profile_entry_text;
static int profile_saving;
static int profile_delete_index = -1;
static char profile_pending_name[SESSION_USER_PROFILE_NAME_MAX];
static uint64_t profile_modal_prev_mask;
static nav_repeat_t profile_osk_up;
static nav_repeat_t profile_osk_down;
static nav_repeat_t profile_osk_left;
static nav_repeat_t profile_osk_right;
static nav_repeat_t profile_osk_backspace;

static void build_rows(void);
static int child_tick(void);
static int row_for_definition(const submenu_def *definition, int local_index);

static void build_profile_rows(void) {
    profile_row_count = 0;

    const char *built_in_labels[play_profile_count] = {
        lang.muxretro.settings_screen.profile_safe, lang.muxretro.settings_screen.profile_balanced,
        lang.muxretro.settings_screen.profile_quality
    };
    const char *built_in_help[play_profile_count] = {
        lang.muxretro.help.settings.profile_safe, lang.muxretro.help.settings.profile_balanced,
        lang.muxretro.help.settings.profile_quality
    };

    for (int index = 0; index < play_profile_count; index++) {
        const char *built_in_glyphs[play_profile_count] = {"settings", "performance", "videosettings"};
        profile_labels[profile_row_count] = built_in_labels[index];
        profile_glyphs[profile_row_count] = built_in_glyphs[index];
        profile_help[profile_row_count] = built_in_help[index];
        profile_row_count++;
    }

    profile_user_count = session_settings_refresh_user_profiles();
    for (int index = 0; index < profile_user_count; index++) {
        const char *description = session_settings_user_profile_description(index);
        profile_labels[profile_row_count] = session_settings_user_profile_name(index);
        profile_glyphs[profile_row_count] = "settings";
        profile_help[profile_row_count] =
            description && *description ? description : lang.muxretro.help.settings.profile_user;
        profile_row_count++;
    }

    profile_save_row = -1;
    if (profile_user_count < SESSION_USER_PROFILE_LIMIT && session_settings_play_profile() == play_profile_unmatched
        && session_settings_user_profile_current() < 0) {
        profile_save_row = profile_row_count;
        profile_labels[profile_row_count] = lang.muxretro.settings_screen.profile_save_current;
        profile_glyphs[profile_row_count] = "save";
        profile_help[profile_row_count] = lang.muxretro.help.settings.profile_save_current;
        profile_row_count++;
    }
    profile_def.row_count = profile_row_count;
}

static void profile_value_text(const int index, char *buf, const size_t len) {
    const enum play_profile current = session_settings_play_profile();
    const int current_user = session_settings_user_profile_current();
    const int is_built_in = index >= 0 && index < play_profile_count;
    const int is_user = index >= play_profile_count && index < play_profile_count + profile_user_count;
    const int selected = (is_built_in && current_user < 0 && current == index)
                         || (is_user && current_user == index - play_profile_count);
    snprintf(buf, len, "%s", selected ? lang.muxretro.settings_screen.profile_current : "");
}

static int profile_row_is_action(const int index) {
    return index >= 0 && index < profile_row_count;
}

static const char *profile_action_label(const int index) {
    return index == profile_save_row ? lang.generic.save : lang.muxretro.settings_screen.profile_apply;
}

static void profile_start_save(int index);

static void profile_action(const int index) {
    if (index < 0 || index >= profile_row_count) return;
    if (index == profile_save_row) {
        profile_start_save(index);
        return;
    }

    if (index < play_profile_count) {
        session_settings_apply_play_profile((enum play_profile) index);
    } else if (!session_settings_user_profile_apply(index - play_profile_count)) {
        pause_menu_show_toast(lang.muxretro.settings_screen.profile_failed);
        return;
    }

    pause_menu_show_toast(lang.muxretro.settings_screen.profile_applied);
}

static uint64_t profile_modal_nav_mask(void) {
    return nav_dir_bits() | (mux_input_pressed(mux_input_a) ? BIT(4) : 0)
           | (mux_input_pressed(mux_input_b) ? BIT(5) : 0) | (mux_input_pressed(mux_input_x) ? BIT(6) : 0)
           | (mux_input_pressed(mux_input_y) ? BIT(7) : 0) | (mux_input_pressed(mux_input_select) ? BIT(8) : 0)
           | (mux_input_pressed(mux_input_start) ? BIT(9) : 0) | nav_mask_page();
}

static void profile_save_osk_objects(void) {
    profile_entry_panel = lv_obj_create(ui_screen);
    lv_obj_set_width(profile_entry_panel, device.mux.width);
    lv_obj_set_height(profile_entry_panel, device.mux.height);
    lv_obj_set_align(profile_entry_panel, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(profile_entry_panel, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(profile_entry_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(profile_entry_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(profile_entry_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(profile_entry_panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(profile_entry_panel, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(profile_entry_panel, 128, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(profile_entry_panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(profile_entry_panel, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(profile_entry_panel, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(profile_entry_panel, 5, MU_OBJ_MAIN_DEFAULT);

    profile_entry_text = lv_textarea_create(profile_entry_panel);
    lv_obj_set_width(profile_entry_text, device.mux.width * 5 / 6);
    lv_obj_set_height(profile_entry_text, LV_SIZE_CONTENT);
    lv_obj_set_align(profile_entry_text, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(profile_entry_text, SESSION_USER_PROFILE_NAME_MAX - 1);
    lv_textarea_set_one_line(profile_entry_text, 1);
    lv_obj_set_style_radius(profile_entry_text, theme.osk.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(profile_entry_text, lv_color_hex(theme.osk.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(profile_entry_text, theme.osk.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(profile_entry_text, 2, MU_OBJ_MAIN_DEFAULT);
}

static void profile_start_save(const int index) {
    (void) index;
    if (profile_save_row < 0 || profile_user_count >= SESSION_USER_PROFILE_LIMIT) return;

    profile_saving = 1;
    init_osk(profile_entry_panel, profile_entry_text, 0, 0, SESSION_USER_PROFILE_NAME_MAX - 1);
    key_show = 1;
    osk_show(profile_entry_panel);

    char default_name[RANDNAME_MAX_LEN];
    randname_generate_with_separator(default_name, sizeof(default_name), " ");
    lv_textarea_set_text(profile_entry_text, default_name);
    profile_modal_prev_mask = profile_modal_nav_mask();
}

static void profile_close_osk(void) {
    profile_saving = 0;
    key_show = 0;
    reset_osk(key_entry);
    lv_textarea_set_text(profile_entry_text, "");
    lv_group_set_focus_cb(ui_group, NULL);
    osk_hide(profile_entry_panel);
}

static void profile_cancel_save(void) {
    profile_saving = 0;
    close_osk(key_entry, ui_group, profile_entry_text, profile_entry_panel);
    self.prev_nav_mask = profile_modal_nav_mask();
}

static void profile_finish_name(void) {
    const char *entered = lv_textarea_get_text(profile_entry_text);
    snprintf(profile_pending_name, sizeof(profile_pending_name), "%s", entered ? entered : "");

    const char *start = profile_pending_name;
    while (*start == ' ' || *start == '\t')
        start++;
    if (start != profile_pending_name) memmove(profile_pending_name, start, strlen(start) + 1);
    size_t length = strlen(profile_pending_name);
    while (length > 0 && (profile_pending_name[length - 1] == ' ' || profile_pending_name[length - 1] == '\t'))
        profile_pending_name[--length] = '\0';

    profile_close_osk();
    if (!profile_pending_name[0]) {
        pause_menu_show_toast(lang.muxretro.settings_screen.profile_save_failed);
        self.prev_nav_mask = profile_modal_nav_mask();
        return;
    }

    dialogue_open(&profile_scope_dlg, &theme);
    profile_modal_prev_mask = profile_modal_nav_mask();
}

static void profile_tick_osk(const uint64_t edge, const uint64_t mask) {
    if (edge & BIT(9)) {
        profile_finish_name();
        return;
    }
    if (edge & BIT(4)) {
        play_sound(snd_keypress);
        const char *key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
        if (key && strcasecmp(key, OSK_DONE) == 0)
            profile_finish_name();
        else
            lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
        return;
    }
    if (edge & BIT(6)) {
        profile_cancel_save();
        return;
    }
    if (edge & BIT(7)) {
        key_space(profile_entry_text);
        return;
    }
    if (edge & BIT(8)) {
        key_clear(profile_entry_text);
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
    const int up = nav_repeat_step(&profile_osk_up, edge & BIT(0), mask & BIT(0), 1, now);
    const int down = nav_repeat_step(&profile_osk_down, edge & BIT(1), mask & BIT(1), 1, now);
    const int left = nav_repeat_step(&profile_osk_left, edge & BIT(2), mask & BIT(2), 1, now);
    const int right = nav_repeat_step(&profile_osk_right, edge & BIT(3), mask & BIT(3), 1, now);
    const int backspace = nav_repeat_step(&profile_osk_backspace, edge & BIT(5), mask & BIT(5), 1, now);

    if (up)
        key_up();
    else if (down)
        key_down();
    else if (left)
        key_left();
    else if (right)
        key_right();
    else if (backspace)
        key_backspace(profile_entry_text);
}

static const char *profile_extra_label(const int index) {
    return index >= play_profile_count && index < play_profile_count + profile_user_count
               ? lang.muxretro.settings_screen.profile_delete
               : NULL;
}

static void profile_start_delete(const int index) {
    if (index < play_profile_count || index >= play_profile_count + profile_user_count) return;
    profile_delete_index = index - play_profile_count;
    dialogue_open(&profile_delete_dlg, &theme);
    profile_modal_prev_mask = profile_modal_nav_mask();
}

static void profile_rebuild_at_current(void) {
    build_rows();
    const int current = session_settings_user_profile_current();
    const enum play_profile built_in = session_settings_play_profile();
    int row = current >= 0 ? play_profile_count + current : (int) built_in;
    if (row < 0) row = profile_save_row >= 0 ? profile_save_row : 0;
    submenu_reopen_at(&self, row_for_definition(&profile_def, row));
}

static int profile_modal_tick(void) {
    if (!profile_saving && !dialogue_active(&profile_scope_dlg) && !dialogue_active(&profile_delete_dlg)) return 0;

    const uint64_t mask = profile_modal_nav_mask();
    const uint64_t edge = mask & ~profile_modal_prev_mask;
    profile_modal_prev_mask = mask;

    if (profile_saving) {
        profile_tick_osk(edge, mask);
        return 1;
    }

    if (dialogue_active(&profile_scope_dlg)) {
        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&profile_scope_dlg, &theme, edge & BIT(1) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const enum user_profile_scope scope = (enum user_profile_scope) profile_scope_dlg.selected;
            dialogue_dismiss(&profile_scope_dlg);
            if (session_settings_user_profile_create(profile_pending_name, scope) >= 0) {
                profile_rebuild_at_current();
                pause_menu_show_toast(lang.muxretro.settings_screen.profile_saved);
            } else {
                pause_menu_show_toast(lang.muxretro.settings_screen.profile_save_failed);
                self.prev_nav_mask = mask;
            }
        } else if (edge & BIT(5)) {
            dialogue_mark_cancelled(&profile_scope_dlg);
            dialogue_dismiss(&profile_scope_dlg);
            self.prev_nav_mask = mask;
        }
        return 1;
    }

    if (edge & (BIT(0) | BIT(1))) {
        dialogue_handle_dpad(&profile_delete_dlg, &theme, edge & BIT(1) ? 1 : -1, 1);
    } else if (edge & BIT(4)) {
        const mux_confirm_opt option = (mux_confirm_opt) profile_delete_dlg.selected;
        dialogue_dismiss(&profile_delete_dlg);
        if (option == mux_confirm_yep) {
            const int deleted = profile_delete_index;
            if (session_settings_user_profile_delete(deleted)) {
                build_rows();
                int row = play_profile_count + deleted;
                if (deleted >= profile_user_count)
                    row = profile_user_count > 0 ? play_profile_count + profile_user_count - 1 : profile_save_row;
                if (row < 0) {
                    const enum play_profile built_in = session_settings_play_profile();
                    row = built_in >= 0 ? (int) built_in : 0;
                }
                submenu_reopen_at(&self, row_for_definition(&profile_def, row));
                pause_menu_show_toast(lang.muxretro.settings_screen.profile_deleted);
            } else {
                pause_menu_show_toast(lang.muxretro.settings_screen.profile_delete_failed);
                self.prev_nav_mask = mask;
            }
        } else {
            self.prev_nav_mask = mask;
        }
        profile_delete_index = -1;
    } else if (edge & BIT(5)) {
        dialogue_mark_cancelled(&profile_delete_dlg);
        dialogue_dismiss(&profile_delete_dlg);
        profile_delete_index = -1;
        self.prev_nav_mask = mask;
    }
    return 1;
}

static void add_definition_row(const submenu_def *definition, const int local_index) {
    if (!definition || local_index < 0 || local_index >= definition->row_count || row_count >= settings_row_limit)
        return;
    row_labels[row_count] = definition->labels[local_index];
    row_glyphs[row_count] = definition->glyphs[local_index];
    row_help[row_count] = definition->help ? definition->help[local_index] : NULL;
    row_definitions[row_count] = definition;
    row_local_indices[row_count] = local_index;
    row_specials[row_count] = settings_special_none;
    row_count++;
}

static void add_definition(const submenu_def *definition) {
    if (!definition) return;
    for (int index = 0; index < definition->row_count; index++)
        add_definition_row(definition, index);
}

static void add_special_row(const settings_special special, const char *label, const char *glyph, const char *help) {
    if (row_count >= settings_row_limit) return;
    row_labels[row_count] = label;
    row_glyphs[row_count] = glyph;
    row_help[row_count] = help;
    row_definitions[row_count] = NULL;
    row_local_indices[row_count] = -1;
    row_specials[row_count] = special;
    row_count++;
}

static void add_settings_frame(const char *label, const int first) {
    if (settings_frame_count >= settings_frame_limit || row_count <= first) return;
    settings_frames[settings_frame_count++] = (list_frame) {.label = label, .first = first, .count = row_count - first};
}

static const submenu_def *row_definition(const int index) {
    return index >= 0 && index < row_count ? row_definitions[index] : NULL;
}

static int row_local_index(const int index) {
    return index >= 0 && index < row_count ? row_local_indices[index] : -1;
}

static settings_special row_special(const int index) {
    return index >= 0 && index < row_count ? row_specials[index] : settings_special_none;
}

static int row_for_definition(const submenu_def *definition, const int local_index) {
    for (int index = 0; index < row_count; index++)
        if (row_definitions[index] == definition && row_local_indices[index] == local_index) return index;
    return -1;
}

static int row_for_special(const settings_special special) {
    for (int index = 0; index < row_count; index++)
        if (row_specials[index] == special) return index;
    return -1;
}

static void row_value_text(const int index, char *buffer, const size_t length) {
    const submenu_def *definition = row_definition(index);
    if (definition && definition->value_text) {
        definition->value_text(row_local_index(index), buffer, length);
        return;
    }
    buffer[0] = '\0';
}

static int row_is_action(const int index) {
    const submenu_def *definition = row_definition(index);
    if (definition) return definition->row_is_action ? definition->row_is_action(row_local_index(index)) : 0;
    return row_special(index) != settings_special_none;
}

static int row_is_save(const int index) {
    return row_special(index) == settings_special_save;
}

static int row_can_cycle(const int index) {
    const submenu_def *definition = row_definition(index);
    if (!definition || !definition->cycle) return 0;
    return definition->row_can_cycle ? definition->row_can_cycle(row_local_index(index)) : !row_is_action(index);
}

static void row_cycle(const int index, const int direction) {
    const submenu_def *definition = row_definition(index);
    if (definition && definition->cycle) definition->cycle(row_local_index(index), direction);
}

static int row_coarse_step(const int index) {
    const submenu_def *definition = row_definition(index);
    return definition && definition->row_coarse_step ? definition->row_coarse_step(row_local_index(index)) : 0;
}

static const char *row_action_label(const int index) {
    const submenu_def *definition = row_definition(index);
    return definition && definition->action_label ? definition->action_label(row_local_index(index)) : NULL;
}

static const char *row_extra_label(const int index) {
    const submenu_def *definition = row_definition(index);
    return definition && definition->extra_label ? definition->extra_label(row_local_index(index)) : NULL;
}

static void row_extra_action(const int index) {
    const submenu_def *definition = row_definition(index);
    if (definition && definition->extra_action) definition->extra_action(row_local_index(index));
}

static const char *row_y_label(const int index) {
    const submenu_def *definition = row_definition(index);
    return definition && definition->y_label ? definition->y_label(row_local_index(index)) : NULL;
}

static void row_y_action(const int index) {
    const submenu_def *definition = row_definition(index);
    if (definition && definition->y_action) definition->y_action(row_local_index(index));
}

static void row_action(const int index) {
    const submenu_def *definition = row_definition(index);
    const int local_index = row_local_index(index);
    const settings_special special = row_special(index);

    if (special == settings_special_core_options)
        options_menu_open();
    else if (definition && definition->action)
        definition->action(local_index);

    if (!child_tick()) {
        build_rows();
        const int row = definition ? row_for_definition(definition, local_index) : row_for_special(special);
        if (row >= 0) submenu_reopen_at(&self, row);
    }
}

static int child_tick(void) {
    if (profile_modal_tick()) return 1;
    if (options_menu_is_active()) {
        options_menu_tick();
        return 1;
    }
    if (viewport_settings_child_tick()) return 1;
    if (colfilter_menu_is_active()) {
        colfilter_menu_tick();
        return 1;
    }
    if (shader_menu_is_active()) {
        shader_menu_tick();
        return 1;
    }
    if (input_port_menu_is_active()) {
        input_port_menu_tick();
        return 1;
    }
    if (controller_options_menu_is_active()) {
        controller_options_menu_tick();
        return 1;
    }
    if (hotkeys_menu_is_active()) {
        hotkeys_menu_tick();
        return 1;
    }
    if (cheevo_settings_child_tick()) return 1;
    return 0;
}

static void closed(void) {
    pause_menu_rebuild();
    pause_menu_focus_settings_item();
    pause_menu_show_nav_hints();
    pause_menu_sync_input_mask();
}

static submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .help = row_help,
    .row_count = 0,
    .frames = settings_frames,
    .frame_count = 0,
    .value_text = row_value_text,
    .cycle = row_cycle,
    .row_can_cycle = row_can_cycle,
    .row_is_action = row_is_action,
    .row_is_save = row_is_save,
    .row_coarse_step = row_coarse_step,
    .action_label = row_action_label,
    .action = row_action,
    .extra_label = row_extra_label,
    .extra_action = row_extra_action,
    .y_label = row_y_label,
    .y_action = row_y_action,
    .child_tick = child_tick,
    .action_without_save_guard = 1,
    .closed = closed,
    .save_title = lang.muxretro.save.settings_title,
    .save_desc = lang.muxretro.save.settings_desc,
};

static submenu_def profile_def = {
    .labels = profile_labels,
    .glyphs = profile_glyphs,
    .help = profile_help,
    .row_count = 0,
    .value_text = profile_value_text,
    .row_is_action = profile_row_is_action,
    .action_label = profile_action_label,
    .action = profile_action,
    .extra_label = profile_extra_label,
    .extra_action = profile_start_delete,
    .child_tick = profile_modal_tick,
    .action_without_save_guard = 1,
    .save_title = lang.muxretro.save.settings_title,
    .save_desc = lang.muxretro.save.settings_desc,
};

static void build_rows(void) {
    row_count = 0;
    settings_frame_count = 0;

    build_profile_rows();

    int first = row_count;
    add_definition(&profile_def);
    add_settings_frame(lang.muxretro.settings_screen.play_profile, first);

    first = row_count;
    add_definition(video_menu_definition());
    add_settings_frame(lang.muxretro.settings_screen.category_video, first);

    first = row_count;
    add_definition(visuals_menu_definition());
    add_settings_frame(lang.muxretro.display, first);

    first = row_count;
    add_definition(overlay_menu_definition());
    add_settings_frame(lang.muxretro.display_screen.overlay, first);

    first = row_count;
    add_definition(viewport_menu_definition());
    add_definition(hud_menu_definition());
    add_settings_frame(lang.muxretro.settings_screen.category_display, first);

    first = row_count;
    add_definition(sound_menu_definition());
    add_settings_frame(lang.muxretro.settings_screen.category_sound, first);

    first = row_count;
    add_definition(input_menu_definition());
    add_settings_frame(lang.muxretro.settings_screen.category_input, first);

    first = row_count;
    add_definition(storage_menu_definition());
    add_settings_frame(lang.muxretro.settings_screen.category_storage, first);

    first = row_count;
    if (device.board.has_network) add_definition(cheevo_settings_definition());
    add_settings_frame(lang.muxretro.retroachievements, first);

    first = row_count;
    if (!netplay_is_active())
        add_special_row(
            settings_special_core_options, lang.muxretro.core_options, "core", lang.muxretro.help.settings.core_options
        );
    add_definition(performance_menu_definition());
    add_special_row(
        settings_special_save, lang.muxretro.save_settings, "settings", lang.muxretro.help.settings.save_all
    );
    add_settings_frame(lang.muxretro.settings_screen.category_advanced, first);

    def.row_count = row_count;
    def.frame_count = settings_frame_count;
}

void settings_menu_init(void) {
    static const char *profile_scope_options[3];
    profile_scope_options[0] = lang.muxretro.settings_screen.profile_scope_content;
    profile_scope_options[1] = lang.muxretro.settings_screen.profile_scope_core;
    profile_scope_options[2] = lang.muxretro.settings_screen.profile_scope_all;
    dialogue_init(
        &profile_scope_dlg, &theme, ui_screen, lang.muxretro.settings_screen.profile_scope_title,
        lang.muxretro.settings_screen.profile_scope_desc, profile_scope_options, 3, lang.generic.select,
        lang.generic.cancel
    );
    dialogue_init_confirm(
        &profile_delete_dlg, &theme, ui_screen, lang.muxretro.settings_screen.profile_delete_title,
        lang.muxretro.settings_screen.profile_delete_desc, lang.muxretro.settings_screen.profile_delete,
        lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );
    profile_save_osk_objects();

    options_menu_init();
    video_menu_init();
    display_menu_init();
    overlay_menu_init();
    viewport_menu_init();
    sound_menu_init();
    input_menu_init();
    performance_menu_init();
    hud_menu_init();
    storage_menu_init();

    build_rows();
    submenu_init(&self, &def);
}

void settings_menu_open(void) {
    build_rows();
    submenu_open(&self);
}

void settings_menu_reopen_core_options(void) {
    build_rows();
    const int row = row_for_special(settings_special_core_options);
    if (row >= 0) submenu_reopen_at(&self, row);
}

int settings_menu_is_active(void) {
    return submenu_is_active(&self);
}

void settings_menu_tick(void) {
    submenu_tick(&self);
}

void settings_menu_reopen_video(void) {
    build_rows();
    submenu_reopen_at(&self, row_for_definition(video_menu_definition(), 0));
}

void settings_menu_reopen_visuals(void) {
    settings_menu_reopen_visuals_at(0);
}

void settings_menu_reopen_visuals_at(const int local_index) {
    build_rows();
    submenu_reopen_at(&self, row_for_definition(visuals_menu_definition(), local_index));
}

void settings_menu_reopen_overlay(void) {
    build_rows();
    submenu_reopen_at(&self, row_for_definition(overlay_menu_definition(), 0));
}

void settings_menu_reopen_viewport_at(const int local_index) {
    build_rows();
    submenu_reopen_at(&self, row_for_definition(viewport_menu_definition(), local_index));
}

void settings_menu_reopen_sound(void) {
    build_rows();
    submenu_reopen_at(&self, row_for_definition(sound_menu_definition(), 0));
}

void settings_menu_reopen_input(void) {
    settings_menu_reopen_input_at(0);
}

void settings_menu_reopen_input_at(const int local_index) {
    build_rows();
    submenu_reopen_at(&self, row_for_definition(input_menu_definition(), local_index));
}

void settings_menu_reopen_performance(void) {
    build_rows();
    submenu_reopen_at(&self, row_for_definition(performance_menu_definition(), 0));
}

void settings_menu_reopen_hud(void) {
    build_rows();
    submenu_reopen_at(&self, row_for_definition(hud_menu_definition(), 0));
}

void settings_menu_reopen_storage(void) {
    build_rows();
    submenu_reopen_at(&self, row_for_definition(storage_menu_definition(), 0));
}

void settings_menu_reopen_cheevo(void) {
    settings_menu_reopen_cheevo_at(0);
}

void settings_menu_reopen_cheevo_at(const int local_index) {
    build_rows();
    const int row = row_for_definition(cheevo_settings_definition(), local_index);
    if (row >= 0) submenu_reopen_at(&self, row);
}
