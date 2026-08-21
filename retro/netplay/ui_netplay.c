#include <stdio.h>
#include <string.h>
#include "../../common/input.h"
#include "../../common/options.h"
#include "../../common/ui/common.h"
#include "../../common/ui/osk.h"
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../input/nav_repeat.h"
#include "../settings/submenu.h"
#include "netplay.h"
#include "ui_netplay.h"

enum { root_row_host = 0, root_row_join, root_row_status, root_row_disconnect, root_row_count };

enum {
    host_row_status = 0,
    host_row_name,
    host_row_mode,
    host_row_slots,
    host_row_port,
    host_row_session,
    host_row_pair,
    host_row_connection,
    host_row_count
};

enum {
    join_row_status = 0,
    join_row_mode,
    join_row_find,
    join_row_session,
    join_row_direct,
    join_row_pair,
    join_row_connection,
    join_row_count
};

static const char *root_labels[root_row_count];
static const char *host_labels[host_row_count];
static const char *join_labels[join_row_count];

static const char *root_glyphs[root_row_count] = {"network", "search", "network", "exit"};
static const char *host_glyphs[host_row_count] = {"network", "user",    "controller", "controller",
                                                  "network", "network", "valid",      "performance"};
static const char *join_glyphs[join_row_count] = {"network", "controller", "search",     "network",
                                                  "network", "valid",      "performance"};

static submenu root_menu;
static submenu host_menu;
static submenu join_menu;
static int menus_initialised;

typedef enum { entry_none = 0, entry_address, entry_host_name } entry_mode;

static const uint16_t host_ports[] = {55435, 57193, 59387, 61231, 63863};

static lv_obj_t *entry_panel;
static lv_obj_t *entry_text;
static entry_mode entering;
static uint64_t entry_previous_mask;
static nav_repeat_t entry_up, entry_down, entry_left, entry_right, entry_backspace;
static unsigned selected_host;
static unsigned selected_port;
static int pairing_focused;

static void status_value(const netplay_info *info, char *buffer, const size_t length) {
    if (info->status == netplay_status_pairing && info->role == netplay_role_host)
        snprintf(
            buffer, length, lang.muxretro.netplay.pairing_progress, info->pairing_confirmed_count,
            info->pairing_remaining_count
        );
    else
        snprintf(buffer, length, "%s", netplay_status_name(info->status));
}

static void connection_value(const netplay_info *info, char *buffer, const size_t length) {
    if (info->status == netplay_status_playing && info->input_delay == 0)
        snprintf(buffer, length, lang.muxretro.netplay.ping_core, info->ping_ms);
    else if (info->status == netplay_status_playing)
        snprintf(buffer, length, lang.muxretro.netplay.ping_delay, info->ping_ms, info->input_delay);
    else
        snprintf(buffer, length, "%s", lang.muxretro.netplay.not_connected);
}

static void pairing_value(const netplay_info *info, char *buffer, const size_t length) {
    if (info->status == netplay_status_pairing && info->role == netplay_role_client && info->pairing_local_confirmed)
        snprintf(buffer, length, "%s", lang.muxretro.netplay.waiting_host);
    else
        snprintf(buffer, length, "%s", info->pairing_code[0] ? info->pairing_code : lang.muxretro.netplay.not_required);
}

static void root_value_text(const int index, char *buffer, const size_t length) {
    netplay_info info;
    netplay_get_info(&info);
    switch (index) {
        case root_row_status:
            status_value(&info, buffer, length);
            break;
        default:
            buffer[0] = '\0';
            break;
    }
}

static void host_value_text(const int index, char *buffer, const size_t length) {
    netplay_info info;
    netplay_get_info(&info);
    switch (index) {
        case host_row_status:
            status_value(&info, buffer, length);
            break;
        case host_row_name:
            netplay_get_host_name(buffer, length);
            break;
        case host_row_mode:
            snprintf(
                buffer, length, "%s",
                netplay_get_host_mode() == netplay_mode_play_together ? lang.muxretro.netplay.play_together
                                                                      : lang.muxretro.netplay.separate_players
            );
            break;
        case host_row_slots:
            snprintf(buffer, length, "%u", netplay_get_host_slots());
            break;
        case host_row_port:
            snprintf(buffer, length, "%u", host_ports[selected_port]);
            break;
        case host_row_session:
            snprintf(
                buffer, length, "%s",
                info.role == netplay_role_host && netplay_is_active() ? netplay_status_name(info.status)
                : netplay_is_active()                                 ? lang.muxretro.netplay.unavailable
                                                                      : lang.muxretro.netplay.ready
            );
            break;
        case host_row_pair:
            pairing_value(&info, buffer, length);
            break;
        case host_row_connection:
            connection_value(&info, buffer, length);
            break;
        default:
            buffer[0] = '\0';
            break;
    }
}

static void join_value_text(const int index, char *buffer, const size_t length) {
    netplay_info info;
    netplay_get_info(&info);
    switch (index) {
        case join_row_status:
            status_value(&info, buffer, length);
            break;
        case join_row_mode:
            snprintf(
                buffer, length, "%s",
                info.mode == netplay_mode_play_together ? lang.muxretro.netplay.play_together
                                                        : lang.muxretro.netplay.separate_players
            );
            break;
        case join_row_find:
            snprintf(buffer, length, lang.muxretro.netplay.found, netplay_discovered_count());
            break;
        case join_row_session: {
            netplay_discovered_host host;
            const unsigned count = netplay_discovered_count();
            if (count && netplay_discovered_get(selected_host % count, &host) == 0)
                snprintf(buffer, length, "%s", host.label[0] ? host.label : host.address);
            else
                snprintf(buffer, length, "%s", lang.muxretro.netplay.none_found);
            break;
        }
        case join_row_direct:
            snprintf(buffer, length, "%s", lang.muxretro.netplay.address_hostname);
            break;
        case join_row_pair:
            pairing_value(&info, buffer, length);
            break;
        case join_row_connection:
            connection_value(&info, buffer, length);
            break;
        default:
            buffer[0] = '\0';
            break;
    }
}

static int root_is_action(const int index) {
    return index == root_row_host || index == root_row_join || (index == root_row_disconnect && netplay_is_active());
}

static int host_is_action(const int index) {
    if (index == host_row_name) return 1;
    if (index == host_row_session) return !netplay_is_active();
    return 0;
}

static int join_is_action(const int index) {
    netplay_info info;
    netplay_get_info(&info);
    if (index == join_row_find || index == join_row_direct) return !netplay_is_active();
    if (index == join_row_session) return !netplay_is_active() && netplay_discovered_count() > 0;
    if (index == join_row_pair) return info.status == netplay_status_pairing && info.role == netplay_role_client;
    return 0;
}

static void host_cycle(const int index, const int direction) {
    if (netplay_is_active()) return;
    if (index == host_row_mode) {
        const netplay_mode mode = netplay_get_host_mode();
        netplay_set_host_mode(mode == netplay_mode_play_together ? netplay_mode_separate : netplay_mode_play_together);
    } else if (index == host_row_slots) {
        const unsigned slots = netplay_get_host_slots();
        const unsigned limit = netplay_get_host_slot_limit();
        netplay_set_host_slots(direction > 0 ? slots % limit + 1 : (slots + limit - 2) % limit + 1);
    } else if (index == host_row_port) {
        const unsigned count = sizeof(host_ports) / sizeof(host_ports[0]);
        selected_port = direction > 0 ? (selected_port + 1) % count : (selected_port + count - 1) % count;
    }
}

static int host_can_cycle(const int index) {
    if (netplay_is_active()) return 0;
    if (index == host_row_mode) return netplay_play_together_available();
    if (index == host_row_slots) return netplay_get_host_slot_limit() > 1;
    return index == host_row_port;
}

static void join_cycle(const int index, const int direction) {
    if (index == join_row_session) {
        const unsigned count = netplay_discovered_count();
        if (count < 2) return;
        selected_host = direction > 0 ? (selected_host + 1) % count : (selected_host + count - 1) % count;
    }
}

static int join_can_cycle(const int index) {
    return index == join_row_session && !netplay_is_active() && netplay_discovered_count() > 1;
}

static const char *join_action_label(const int index) {
    if (index == join_row_session) return lang.muxretro.netplay.join;
    if (index == join_row_pair) return lang.generic.confirm;
    return NULL;
}

static const char *root_action_label(const int index) {
    if (index == root_row_host) return lang.muxretro.netplay.host;
    if (index == root_row_join) return lang.muxretro.netplay.join;
    if (index == root_row_disconnect) return lang.muxretro.netplay.disconnect;
    return NULL;
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

static void entry_close(void) {
    const entry_mode mode = entering;
    entering = entry_none;
    close_osk(key_entry, ui_group, entry_text, entry_panel);
    submenu_reopen_at(
        mode == entry_host_name ? &host_menu : &join_menu, mode == entry_host_name ? host_row_name : join_row_direct
    );
}

static int join_address(const char *specification) {
    char address[256];
    uint16_t port;
    return netplay_parse_address(specification, address, sizeof(address), &port) == 0 ? netplay_join(address, port)
                                                                                      : -1;
}

static void entry_finish(void) {
    const char *text = lv_textarea_get_text(entry_text);
    const entry_mode mode = entering;
    const int result = mode == entry_host_name ? netplay_set_host_name(text) : join_address(text);
    reset_osk(key_entry);
    lv_textarea_set_text(entry_text, "");
    osk_hide(entry_panel);
    entering = entry_none;
    if (mode == entry_host_name)
        pause_menu_show_toast(
            result == 0 ? lang.muxretro.netplay.host_name_saved : lang.muxretro.netplay.invalid_host_name
        );
    else
        pause_menu_show_toast(
            result == 0 ? lang.muxretro.netplay.connecting_host : lang.muxretro.netplay.invalid_address
        );
    submenu_reopen_at(
        mode == entry_host_name ? &host_menu : &join_menu, mode == entry_host_name ? host_row_name : join_row_direct
    );
}

static void entry_open(const entry_mode mode) {
    entry_create();
    entering = mode;
    if (mode == entry_host_name) {
        char host_name[NETPLAY_HOST_NAME_SIZE];
        netplay_get_host_name(host_name, sizeof(host_name));
        lv_textarea_set_text(entry_text, host_name);
    } else {
        lv_textarea_set_text(entry_text, "");
    }
    init_osk(entry_panel, entry_text, 0, 0, mode == entry_host_name ? NETPLAY_HOST_NAME_MAX : 255);
    key_show = 1;
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
        const char *key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
        if (key && strcasecmp(key, OSK_DONE) == 0)
            entry_finish();
        else
            lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
        return;
    }
    if (edge & BIT(6)) {
        entry_close();
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

static void disconnect_session(void) {
    if (!netplay_is_active()) return;
    netplay_disconnect();
    pause_menu_show_toast(lang.muxretro.netplay.disconnected);
}

static void root_action(const int index) {
    switch (index) {
        case root_row_host:
            pairing_focused = 0;
            submenu_open(&host_menu);
            break;
        case root_row_join:
            pairing_focused = 0;
            submenu_open(&join_menu);
            break;
        case root_row_disconnect:
            disconnect_session();
            break;
        default:
            break;
    }
}

static void host_action(const int index) {
    switch (index) {
        case host_row_name:
            entry_open(entry_host_name);
            break;
        case host_row_session:
            if (!netplay_is_active() && netplay_host(host_ports[selected_port]) == 0)
                pause_menu_show_toast(lang.muxretro.netplay.host_ready);
            break;
        default:
            break;
    }
}

static const char *host_action_label(const int index) {
    if (index == host_row_session) return lang.muxretro.netplay.host;
    return NULL;
}

static void join_action(const int index) {
    netplay_info info;
    netplay_get_info(&info);
    switch (index) {
        case join_row_find:
            if (!netplay_is_active() && netplay_discover() == 0) {
                selected_host = 0;
                pause_menu_show_toast(lang.muxretro.netplay.looking_lan);
            }
            break;
        case join_row_session: {
            const unsigned count = netplay_discovered_count();
            if (!count)
                pause_menu_show_toast(lang.muxretro.netplay.no_discovered);
            else if (netplay_join_discovered(selected_host % count) == 0)
                pause_menu_show_toast(lang.muxretro.netplay.connecting_host);
            else
                pause_menu_show_toast(lang.muxretro.netplay.selected_unavailable);
            break;
        }
        case join_row_direct:
            if (!netplay_is_active()) entry_open(entry_address);
            break;
        case join_row_pair:
            if (info.status == netplay_status_pairing && info.role == netplay_role_client) {
                netplay_confirm_pairing();
                pause_menu_show_toast(lang.muxretro.netplay.pairing_confirmed);
            }
            break;
        default:
            break;
    }
}

static void root_closed(void) {
    pause_menu_rebuild();
    pause_menu_focus_netplay_item();
    pause_menu_show_nav_hints();
    pause_menu_sync_input_mask();
}

static void host_closed(void) {
    pairing_focused = 0;
    submenu_reopen_at(&root_menu, root_row_host);
}

static void join_closed(void) {
    pairing_focused = 0;
    submenu_reopen_at(&root_menu, root_row_join);
}

static void pairing_focus(submenu *menu, const int row, const netplay_role role) {
    netplay_info info;
    netplay_get_info(&info);
    if (info.status == netplay_status_pairing && info.role == role && !pairing_focused) {
        pairing_focused = 1;
        submenu_focus_at(menu, row);
        submenu_refresh_nav(menu);
    } else if (info.status != netplay_status_pairing) {
        pairing_focused = 0;
    }
}

static int child_tick(void) {
    if (submenu_is_active(&host_menu)) {
        submenu_refresh_values(&host_menu);
        submenu_refresh_nav(&host_menu);
        pairing_focus(&host_menu, host_row_pair, netplay_role_host);
        submenu_tick(&host_menu);
        return 1;
    }
    if (submenu_is_active(&join_menu)) {
        submenu_refresh_values(&join_menu);
        submenu_refresh_nav(&join_menu);
        pairing_focus(&join_menu, join_row_pair, netplay_role_client);
        submenu_tick(&join_menu);
        return 1;
    }
    return 0;
}

static submenu_def root_definition = {
    .labels = root_labels,
    .glyphs = root_glyphs,
    .row_count = root_row_count,
    .value_text = root_value_text,
    .row_is_action = root_is_action,
    .action_label = root_action_label,
    .action = root_action,
    .child_tick = child_tick,
    .closed = root_closed,
    .save_title = NULL,
    .save_desc = NULL,
};

static submenu_def host_definition = {
    .labels = host_labels,
    .glyphs = host_glyphs,
    .row_count = host_row_count,
    .value_text = host_value_text,
    .cycle = host_cycle,
    .row_can_cycle = host_can_cycle,
    .row_is_action = host_is_action,
    .action_label = host_action_label,
    .action = host_action,
    .closed = host_closed,
    .save_title = NULL,
    .save_desc = NULL,
};

static submenu_def join_definition = {
    .labels = join_labels,
    .glyphs = join_glyphs,
    .row_count = join_row_count,
    .value_text = join_value_text,
    .cycle = join_cycle,
    .row_can_cycle = join_can_cycle,
    .row_is_action = join_is_action,
    .action_label = join_action_label,
    .action = join_action,
    .closed = join_closed,
    .save_title = NULL,
    .save_desc = NULL,
};

static int configure_root_rows(void) {
    const int wanted = netplay_is_active() ? root_row_count : root_row_count - 1;
    if (root_definition.row_count == wanted) return 0;

    root_definition.row_count = wanted;
    return 1;
}

void netplay_menu_init(void) {
    if (!device.board.has_network) return;

    root_labels[root_row_host] = lang.muxretro.netplay.host_network;
    root_labels[root_row_join] = lang.muxretro.netplay.join_network;
    root_labels[root_row_status] = lang.muxretro.netplay.status;
    root_labels[root_row_disconnect] = lang.muxretro.netplay.disconnect;

    host_labels[host_row_status] = lang.muxretro.netplay.status;
    host_labels[host_row_name] = lang.muxretro.netplay.host_name;
    host_labels[host_row_mode] = lang.muxretro.netplay.play_mode;
    host_labels[host_row_slots] = lang.muxretro.netplay.client_slots;
    host_labels[host_row_port] = lang.muxretro.netplay.host_port;
    host_labels[host_row_session] = lang.muxretro.netplay.host_session;
    host_labels[host_row_pair] = lang.muxretro.netplay.pairing_code;
    host_labels[host_row_connection] = lang.muxretro.netplay.connection;

    join_labels[join_row_status] = lang.muxretro.netplay.status;
    join_labels[join_row_mode] = lang.muxretro.netplay.play_mode;
    join_labels[join_row_find] = lang.muxretro.netplay.find_lan;
    join_labels[join_row_session] = lang.muxretro.netplay.join_found;
    join_labels[join_row_direct] = lang.muxretro.netplay.join_address;
    join_labels[join_row_pair] = lang.muxretro.netplay.confirm_pairing;
    join_labels[join_row_connection] = lang.muxretro.netplay.connection;

    root_definition.save_title = lang.muxretro.network_play;
    root_definition.save_desc = lang.muxretro.netplay.menu_desc;
    host_definition.save_title = lang.muxretro.netplay.host_network;
    host_definition.save_desc = lang.muxretro.netplay.host_menu_desc;
    join_definition.save_title = lang.muxretro.netplay.join_network;
    join_definition.save_desc = lang.muxretro.netplay.join_menu_desc;

    configure_root_rows();
    submenu_init(&root_menu, &root_definition);
    submenu_init(&host_menu, &host_definition);
    submenu_init(&join_menu, &join_definition);
    menus_initialised = 1;
}

void netplay_menu_open(void) {
    if (!device.board.has_network) return;
    if (!menus_initialised) netplay_menu_init();
    pairing_focused = 0;
    configure_root_rows();
    submenu_open(&root_menu);
}

int netplay_menu_is_active(void) {
    return menus_initialised && submenu_is_active(&root_menu);
}

void netplay_menu_tick(void) {
    if (entering != entry_none) {
        entry_tick();
        return;
    }
    if (!submenu_is_active(&host_menu) && !submenu_is_active(&join_menu)) {
        if (configure_root_rows() && submenu_is_active(&root_menu)) {
            int row = current_item_index;
            if (row >= root_definition.row_count) row = root_definition.row_count - 1;

            submenu_reopen_at(&root_menu, row);
        }

        submenu_refresh_values(&root_menu);
        submenu_refresh_nav(&root_menu);
    }
    submenu_tick(&root_menu);
}
