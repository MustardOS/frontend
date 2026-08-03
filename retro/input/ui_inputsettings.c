#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

enum {
    row_hotkeys = 0,
    row_port_1,
    row_port_2,
    row_port_3,
    row_port_4,
    row_auto_assign,
    row_controller_options,
    row_reset_input,
    row_count
};

static const char *row_labels[row_count] = {
    lang.muxretro.hotkeys,
    lang.muxretro.settings_screen.port_1,      lang.muxretro.settings_screen.port_2,
    lang.muxretro.settings_screen.port_3,      lang.muxretro.settings_screen.port_4,
    lang.muxretro.settings_screen.auto_assign, lang.muxretro.settings_screen.controller_options,
    lang.muxretro.settings_screen.reset_input,
};

static const char *row_glyphs[row_count] = {"hotkeys",    "port1",      "port2",             "port3", "port4",
                                            "autoassign", "controlleroptions", "inputreset"};

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    if (index >= row_port_1 && index <= row_port_4) {
        session_settings_port_summary(index - row_port_1, buf, buf_len);
        return;
    }

    buf[0] = '\0';
}

static int row_is_action(const int index) {
    (void) index;
    return 1;
}

static submenu self;

static void row_action(const int index) {
    switch (index) {
        case row_hotkeys:
            hotkeys_menu_open();
            break;
        case row_port_1:
        case row_port_2:
        case row_port_3:
        case row_port_4:
            input_port_menu_open(index - row_port_1);
            break;
        case row_auto_assign:
            session_settings_auto_assign_controllers();
            submenu_refresh_values(&self);
            break;
        case row_controller_options:
            controller_options_menu_open();
            break;
        case row_reset_input:
            session_settings_reset_input();
            submenu_refresh_values(&self);
            break;
        default:
            break;
    }
}

static int child_tick(void) {
    if (hotkeys_menu_is_active()) {
        hotkeys_menu_tick();
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

    return 0;
}

static void closed(void) {
    settings_menu_reopen_input();
}

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .row_count = row_count,
    .value_text = row_value_text,
    .row_is_action = row_is_action,
    .action = row_action,
    .child_tick = child_tick,
    .closed = closed,
    .save_title = lang.muxretro.save.input_title,
    .save_desc = lang.muxretro.save.input_desc,
};

void input_menu_init(void) {
    submenu_init(&self, &def);
    hotkeys_menu_init();
    controller_options_menu_init();
    input_port_menu_init_all();
}

void input_menu_open(void) {
    submenu_open(&self);
}

int input_menu_is_active(void) {
    return submenu_is_active(&self);
}

void input_menu_tick(void) {
    submenu_tick(&self);
}

void input_menu_reopen_port(const int port) {
    submenu_reopen_at(&self, row_port_1 + port);
}

void input_menu_reopen_controller_options(void) {
    submenu_reopen_at(&self, row_controller_options);
}

void input_menu_reopen_hotkeys(void) {
    submenu_reopen_at(&self, row_hotkeys);
}
