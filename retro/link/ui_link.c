#include <stdio.h>
#include <strings.h>
#include "../../common/device.h"
#include "../../common/init.h"
#include "../../common/input.h"
#include "../../common/language.h"
#include "../../common/sysinfo.h"
#include "../../common/theme.h"
#include "../../common/ui/common.h"
#include "../../common/ui/osk.h"
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../input/nav_repeat.h"
#include "../settings/submenu.h"
#include "../ui/options.h"
#include "link.h"
#include "ui_link.h"

enum { link_row_mode = 0, link_row_address, link_row_single, link_row_count };

static const char *base_labels[link_row_count];
static const char *base_glyphs[link_row_count] = {"network", "network", "controller"};

static const char *link_labels[link_row_count];
static const char *link_glyphs[link_row_count];
static int row_map[link_row_count];
static int visible_rows = 1;

static int logical_row(const int index) {
    return index >= 0 && index < visible_rows ? row_map[index] : link_row_mode;
}

static submenu link_menu;
static int menus_initialised = 0;

static lv_obj_t *entry_panel = NULL;
static lv_obj_t *entry_text = NULL;
static int entering = 0;
static uint64_t entry_previous_mask = 0;
static nav_repeat_t entry_up = {0};
static nav_repeat_t entry_down = {0};
static nav_repeat_t entry_left = {0};
static nav_repeat_t entry_right = {0};
static nav_repeat_t entry_backspace = {0};
static enum link_mode shown_mode = link_mode_off;

static const char *mode_label(const enum link_mode mode) {
    switch (mode) {
        case link_mode_host:
            return lang.muxretro.link.mode_host;
        case link_mode_join:
            return lang.muxretro.link.mode_join;
        case link_mode_local:
            return lang.muxretro.link.mode_local;
        default:
            return lang.generic.disabled;
    }
}

static void link_value_text(const int index, char *buffer, const size_t length) {
    switch (logical_row(index)) {
        case link_row_mode:
            snprintf(buffer, length, "%s", mode_label(link_get_mode()));
            return;

        case link_row_address: {
            char host[LINK_HOST_LEN];

            if (link_get_mode() == link_mode_host) {
                if (!get_any_ipv4_address(host, sizeof(host))) {
                    snprintf(buffer, length, "%s", lang.muxretro.link.no_address);
                    return;
                }
            } else {
                link_get_host(host, sizeof(host));
            }

            snprintf(buffer, length, "%s", host);
            return;
        }

        case link_row_single:
            snprintf(buffer, length, "%s", link_single_screen_setting() ? lang.generic.enabled : lang.generic.disabled);
            return;

        default:
            buffer[0] = '\0';
    }
}

static int link_row_can_cycle(const int index) {
    const int row = logical_row(index);
    return row == link_row_mode || row == link_row_single;
}

static int link_row_is_action(const int index) {
    return logical_row(index) == link_row_address && link_get_mode() == link_mode_join;
}

static const char *link_action_label(const int index) {
    if (link_row_is_action(index)) return lang.generic.select;
    return NULL;
}

static void link_cycle(const int index, const int direction) {
    const int row = logical_row(index);

    if (row == link_row_single) {
        link_single_screen_set(!link_single_screen_setting());
        video_bridge_apply_scaling();
        pause_menu_refresh_gb_slot();
        return;
    }

    if (row != link_row_mode) return;

    int next = (int) link_get_mode();
    for (int step = 0; step < link_mode_count; step++) {
        next = (next + direction + link_mode_count) % link_mode_count;
        if (link_mode_available((enum link_mode) next)) break;
    }

    if (!link_set_mode((enum link_mode) next)) return;
    options_save_content();

    if (next == link_mode_local) pause_menu_show_toast(lang.muxretro.link.restart_needed);
}

static uint64_t entry_nav_mask(void) {
    return nav_dir_bits() | (mux_input_pressed(mux_input_a) ? BIT(4) : 0)
           | (mux_input_pressed(mux_input_b) ? BIT(5) : 0) | (mux_input_pressed(mux_input_x) ? BIT(6) : 0)
           | (mux_input_pressed(mux_input_y) ? BIT(7) : 0) | (mux_input_pressed(mux_input_select) ? BIT(8) : 0)
           | (mux_input_pressed(mux_input_start) ? BIT(9) : 0) | nav_mask_page();
}

static void entry_create(void) {
    if (entry_panel) return;

    entry_panel = lv_obj_create(ui_screen);
    lv_obj_set_size(entry_panel, device.mux.width, device.mux.height);
    lv_obj_center(entry_panel);
    lv_obj_set_flex_flow(entry_panel, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(entry_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(entry_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(entry_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(entry_panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(entry_panel, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(entry_panel, 128, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(entry_panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(entry_panel, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(entry_panel, 5, MU_OBJ_MAIN_DEFAULT);

    entry_text = lv_textarea_create(entry_panel);
    lv_obj_set_width(entry_text, device.mux.width * 5 / 6);
    lv_obj_set_height(entry_text, LV_SIZE_CONTENT);
    lv_obj_center(entry_text);
    lv_textarea_set_one_line(entry_text, 1);
    lv_obj_set_style_radius(entry_text, theme.osk.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(entry_text, lv_color_hex(theme.osk.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(entry_text, theme.osk.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(entry_text, 2, MU_OBJ_MAIN_DEFAULT);
}

static void entry_finish(void) {
    const char *text = lv_textarea_get_text(entry_text);
    const int accepted = link_set_host(text);
    if (accepted) options_save_content();

    reset_osk(num_entry);
    lv_textarea_set_text(entry_text, "");
    osk_hide(entry_panel);
    entering = 0;

    pause_menu_show_toast(accepted ? lang.muxretro.link.address_saved : lang.muxretro.link.invalid_address);
    submenu_reopen_at(&link_menu, link_row_address);
}

static void entry_close(void) {
    entering = 0;
    close_osk(num_entry, ui_group, entry_text, entry_panel);
    submenu_reopen_at(&link_menu, link_row_address);
}

static void entry_open(void) {
    entry_create();
    entering = 1;

    char host[LINK_HOST_LEN];
    link_get_host(host, sizeof(host));
    lv_textarea_set_text(entry_text, host);

    key_show = 2;
    init_osk(entry_panel, entry_text, 1, 0, LINK_HOST_LEN - 1);

    lv_obj_add_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_state(key_entry, LV_STATE_DISABLED);
    lv_obj_clear_flag(num_entry, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(num_entry, LV_STATE_DISABLED);

    osk_show(entry_panel);
    entry_previous_mask = entry_nav_mask();
}

static void entry_tick(void) {
    const uint64_t mask = entry_nav_mask();
    const uint64_t edge = mask & ~entry_previous_mask;

    entry_previous_mask = mask;

    if (nav_input_halted()) return;

    if (edge & BIT(9)) {
        entry_finish();
        return;
    }

    if (edge & BIT(4)) {
        const char *key = lv_btnmatrix_get_btn_text(num_entry, key_curr);
        if (key && strcasecmp(key, OSK_DONE) == 0)
            entry_finish();
        else
            lv_event_send(num_entry, LV_EVENT_CLICKED, &key_curr);
        return;
    }

    if (edge & BIT(6)) {
        entry_close();
        return;
    }

    if (edge & BIT(8)) {
        key_clear(entry_text);
        return;
    }

    const uint32_t now = SDL_GetTicks();
    const int up = nav_repeat_step(&entry_up, edge & BIT(0), mask & BIT(0), 1, now);
    const int down = nav_repeat_step(&entry_down, edge & BIT(1), mask & BIT(1), 1, now);
    const int left = nav_repeat_step(&entry_left, edge & BIT(2), mask & BIT(2), 1, now);
    const int right = nav_repeat_step(&entry_right, edge & BIT(3), mask & BIT(3), 1, now);
    const int backspace = nav_repeat_step(&entry_backspace, edge & BIT(5), mask & BIT(5), 1, now);

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

static void link_action(const int index) {
    if (logical_row(index) == link_row_address && link_get_mode() == link_mode_join) entry_open();
}

static void link_closed(void) {
    pause_menu_rebuild();
    pause_menu_focus_game_link_item();
    pause_menu_show_nav_hints();
    pause_menu_sync_input_mask();
}

static submenu_def link_definition = {
    .labels = link_labels,
    .glyphs = link_glyphs,
    .row_count = link_row_count,
    .value_text = link_value_text,
    .cycle = link_cycle,
    .row_can_cycle = link_row_can_cycle,
    .row_is_action = link_row_is_action,
    .action_label = link_action_label,
    .action = link_action,
    .closed = link_closed,
    .save_title = NULL,
    .save_desc = NULL,
};

static void apply_row_count(void) {
    const enum link_mode mode = link_get_mode();

    int n = 0;
    row_map[n++] = link_row_mode;

    if (mode == link_mode_host || mode == link_mode_join)
        row_map[n++] = link_row_address;
    else if (mode == link_mode_local)
        row_map[n++] = link_row_single;

    for (int i = 0; i < n; i++) {
        link_labels[i] = base_labels[row_map[i]];
        link_glyphs[i] = base_glyphs[row_map[i]];
    }

    visible_rows = n;
    link_definition.row_count = n;
}

void link_menu_init(void) {
    if (!link_is_supported()) return;

    base_labels[link_row_mode] = lang.muxretro.link.mode;
    base_labels[link_row_address] = lang.muxretro.link.address;
    base_labels[link_row_single] = lang.muxretro.link.single_screen;

    link_reveal_settings();
    link_align_port();

    link_definition.save_title = lang.muxretro.link.game_link;
    link_definition.save_desc = lang.muxretro.link.menu_desc;

    submenu_init(&link_menu, &link_definition);
    menus_initialised = 1;
}

void link_menu_open(void) {
    if (!link_is_supported()) return;
    if (!menus_initialised) link_menu_init();

    shown_mode = link_get_mode();
    apply_row_count();
    submenu_open(&link_menu);
}

int link_menu_is_active(void) {
    return menus_initialised && submenu_is_active(&link_menu);
}

void link_menu_tick(void) {
    if (entering) {
        entry_tick();
        return;
    }

    submenu_tick(&link_menu);

    if (shown_mode != link_get_mode()) {
        shown_mode = link_get_mode();
        apply_row_count();
        submenu_reopen_at(&link_menu, link_row_mode);
    }
}

void link_menu_teardown(void) {
    entry_panel = NULL;
    entry_text = NULL;
    entering = 0;
    menus_initialised = 0;
}
