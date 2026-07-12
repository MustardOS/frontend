#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

enum {
    row_rumble = 0,
    row_analog_deadzone,
    row_analog_anti_deadzone,
    row_analog_sensitivity,
    row_analog_invert_y,
    row_count
};

static const char *row_labels[row_count] = {
    lang.muxretro.settings_screen.rumble, lang.muxretro.settings_screen.analog_deadzone,
    lang.muxretro.settings_screen.analog_anti_deadzone, lang.muxretro.settings_screen.analog_sensitivity,
    lang.muxretro.settings_screen.analog_invert_y
};

static const char *row_glyphs[row_count] = {
    "rumble", "analogdeadzone", "analogantideadzone", "analogsensitivity", "analoginverty"
};

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_rumble:
            snprintf(
                buf, buf_len, "%s", session_settings.rumble_enabled ? lang.generic.enabled : lang.generic.disabled
            );
            break;
        case row_analog_deadzone:
            snprintf(buf, buf_len, "%s", session_settings_analog_deadzone_name(session_settings.analog_deadzone));
            break;
        case row_analog_anti_deadzone:
            snprintf(
                buf, buf_len, "%s", session_settings_analog_anti_deadzone_name(session_settings.analog_anti_deadzone)
            );
            break;
        case row_analog_sensitivity:
            snprintf(buf, buf_len, "%s", session_settings_analog_sensitivity_name(session_settings.analog_sensitivity));
            break;
        case row_analog_invert_y:
            snprintf(
                buf, buf_len, "%s", session_settings.analog_invert_y ? lang.generic.enabled : lang.generic.disabled
            );
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (index) {
        case row_rumble:
            session_settings_cycle_rumble(direction);
            break;
        case row_analog_deadzone:
            session_settings_cycle_analog_deadzone(direction);
            break;
        case row_analog_anti_deadzone:
            session_settings_cycle_analog_anti_deadzone(direction);
            break;
        case row_analog_sensitivity:
            session_settings_cycle_analog_sensitivity(direction);
            break;
        case row_analog_invert_y:
            session_settings_cycle_analog_invert_y(direction);
            break;
        default:
            break;
    }
}

static void closed(void) {
    settings_menu_reopen_input();
}

static submenu self;

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .row_count = row_count,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .closed = closed,
    .save_title = lang.muxretro.save.input_title,
    .save_desc = lang.muxretro.save.input_desc,
};

void input_menu_init(void) {
    submenu_init(&self, &def);
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
