#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

enum { row_controller = 0, row_core_device, row_stick_dpad, row_button_mapping, row_macros, row_reset_port, row_count };

static const char *row_labels[row_count] = {
    lang.muxretro.settings_screen.controller, lang.muxretro.settings_screen.core_device,
    lang.muxretro.settings_screen.stick_dpad, lang.muxretro.settings_screen.button_mapping,
    lang.muxretro.settings_screen.macros,     lang.muxretro.settings_screen.reset_port,
};

static const char *row_glyphs[row_count] = {"controller",    "coredevice", "sticksensitivity",
                                            "buttonmapping", "macro",      "portreset"};

static const char *row_help[row_count] = {lang.muxretro.help.port.controller, lang.muxretro.help.port.core_device,
                                          lang.muxretro.help.port.stick_dpad, lang.muxretro.help.port.button_mapping,
                                          lang.muxretro.help.port.macros,     lang.muxretro.help.port.reset_port};

static int active_port = 0;

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_controller:
            session_settings_port_summary(active_port, buf, buf_len);
            break;
        case row_core_device:
            session_settings_port_device_summary(active_port, buf, buf_len);
            break;
        case row_stick_dpad:
            session_settings_stick_dpad_summary(active_port, buf, buf_len);
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (index) {
        case row_controller:
            session_settings_cycle_port_controller(active_port, direction);
            break;
        case row_core_device:
            session_settings_cycle_port_device(active_port, direction);
            break;
        case row_stick_dpad:
            session_settings_cycle_stick_dpad(active_port, direction);
            break;
        default:
            break;
    }
}

static int row_is_action(const int index) {
    return index == row_button_mapping || index == row_macros || index == row_reset_port;
}

static submenu port_self[MUX_INPUT_PORT_COUNT];

static void row_action(const int index) {
    switch (index) {
        case row_button_mapping:
            button_mapping_menu_open(active_port);
            break;
        case row_macros:
            macros_menu_open(active_port);
            break;
        case row_reset_port:
            session_settings_reset_input_port(active_port);
            submenu_refresh_values(&port_self[active_port]);
            break;
        default:
            break;
    }
}

static int child_tick(void) {
    if (button_mapping_menu_is_active()) {
        button_mapping_menu_tick();
        return 1;
    }

    if (macros_menu_is_active()) {
        macros_menu_tick();
        return 1;
    }

    return 0;
}

static void closed(void) {
    input_menu_reopen_port(active_port);
}

static const submenu_def port_def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .help = row_help,
    .row_count = row_count,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .row_is_action = row_is_action,
    .action = row_action,
    .child_tick = child_tick,
    .closed = closed,
    .save_title = lang.muxretro.save.input_port_title,
    .save_desc = lang.muxretro.save.input_port_desc,
};

void input_port_menu_init_all(void) {
    for (int i = 0; i < MUX_INPUT_PORT_COUNT; i++)
        submenu_init(&port_self[i], &port_def);

    button_mapping_menu_init_all();
    macros_menu_init();
}

void input_port_menu_open(const int port) {
    if (port < 0 || port >= MUX_INPUT_PORT_COUNT) return;
    active_port = port;
    submenu_open(&port_self[port]);
}

int input_port_menu_is_active(void) {
    for (int i = 0; i < MUX_INPUT_PORT_COUNT; i++)
        if (submenu_is_active(&port_self[i])) return 1;
    return 0;
}

void input_port_menu_tick(void) {
    for (int i = 0; i < MUX_INPUT_PORT_COUNT; i++) {
        if (submenu_is_active(&port_self[i])) {
            submenu_tick(&port_self[i]);
            return;
        }
    }
}

void input_port_menu_reopen_button_mapping(const int port) {
    active_port = port;
    submenu_reopen_at(&port_self[port], row_button_mapping);
}

void input_port_menu_reopen_macros(const int port) {
    active_port = port;
    submenu_reopen_at(&port_self[port], row_macros);
}
