#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

enum { row_auto_save = 0, row_sram_flush, row_count };

static const char *row_labels[row_count] = {
    lang.muxretro.settings_screen.auto_save, lang.muxretro.settings_screen.sram_flush
};

static const char *row_glyphs[row_count] = {"autosave", "sram"};

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_auto_save:
            snprintf(buf, buf_len, "%s", session_settings_auto_save_name(session_settings.auto_save));
            break;
        case row_sram_flush:
            snprintf(buf, buf_len, "%s", session_settings_sram_flush_name(session_settings.sram_flush_seconds));
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (index) {
        case row_auto_save:
            session_settings_cycle_auto_save(direction);
            break;
        case row_sram_flush:
            session_settings_cycle_sram_flush(direction);
            break;
        default:
            break;
    }
}

static void closed(void) {
    settings_menu_reopen_storage();
}

static submenu self;

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .row_count = row_count,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .closed = closed,
    .save_title = lang.muxretro.save.storage_title,
    .save_desc = lang.muxretro.save.storage_desc,
};

void storage_menu_init(void) {
    submenu_init(&self, &def);
}

void storage_menu_open(void) {
    submenu_open(&self);
}

int storage_menu_is_active(void) {
    return submenu_is_active(&self);
}

void storage_menu_tick(void) {
    submenu_tick(&self);
}
