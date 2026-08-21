#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/pages.h"
#include "../settings/submenu.h"
#include "history.h"

enum {
    item_auto_save = 0,
    item_timeline,
    item_timeline_count,
    item_history,
    item_trash,
    item_sram_flush,
    item_precache,
    storage_item_count
};

static const char *item_labels[storage_item_count] = {
    lang.muxretro.settings_screen.auto_save,        lang.muxretro.settings_screen.timeline_saves,
    lang.muxretro.settings_screen.timeline_count,   lang.muxretro.settings_screen.history_states,
    lang.muxretro.settings_screen.trash_count,      lang.muxretro.settings_screen.sram_flush,
    lang.muxretro.settings_screen.content_precache,
};

static const char *item_glyphs[storage_item_count] = {"autosave", "timeline", "timeline", "state",
                                                      "state",    "sram",     "content"};

static const char *item_help[storage_item_count] = {
    lang.muxretro.help.storage.auto_save,      lang.muxretro.help.storage.timeline,
    lang.muxretro.help.storage.timeline_count, lang.muxretro.help.storage.history_states,
    lang.muxretro.help.storage.trash_count,    lang.muxretro.help.storage.sram_flush,
    lang.muxretro.help.storage.precache
};

static int item_is_shown(const int item) {
    switch (item) {
        case item_auto_save:
        case item_timeline:
        case item_timeline_count:
        case item_history:
        case item_trash:
            return state_saves_supported();
        default:
            return 1;
    }
}

static const char *row_labels[storage_item_count];
static const char *row_glyphs[storage_item_count];
static const char *row_help[storage_item_count];
static int row_items[storage_item_count];
static int row_count = 0;

static int item_at_row(const int index) {
    return index >= 0 && index < row_count ? row_items[index] : -1;
}

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (item_at_row(index)) {
        case item_auto_save:
            snprintf(buf, buf_len, "%s", session_settings_auto_save_name(session_settings.auto_save));
            break;
        case item_timeline:
            snprintf(buf, buf_len, "%s", session_settings_timeline_interval_name(session_settings.timeline_interval));
            break;
        case item_timeline_count:
            snprintf(buf, buf_len, "%d", session_settings.timeline_count);
            break;
        case item_history: {
            if (session_settings.history_depth <= 0) {
                snprintf(buf, buf_len, "%s", lang.generic.disabled);
                break;
            }

            const int effective = history_effective_depth();
            if (effective != session_settings.history_depth)
                snprintf(buf, buf_len, "%d (%d)", session_settings.history_depth, effective);
            else
                snprintf(buf, buf_len, "%d", session_settings.history_depth);
            break;
        }
        case item_trash:
            snprintf(buf, buf_len, "%s", session_settings_trash_count_name(session_settings.trash_count));
            break;
        case item_sram_flush:
            snprintf(buf, buf_len, "%s", session_settings_sram_flush_name(session_settings.sram_flush_seconds));
            break;
        case item_precache:
            snprintf(buf, buf_len, "%s", session_settings_content_precache_name(session_settings.content_precache));
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (item_at_row(index)) {
        case item_auto_save:
            session_settings_cycle_auto_save(direction);
            break;
        case item_timeline:
            session_settings_cycle_timeline_interval(direction);
            break;
        case item_timeline_count:
            session_settings_cycle_timeline_count(direction);
            break;
        case item_history:
            session_settings_cycle_history_depth(direction);
            break;
        case item_trash:
            session_settings_cycle_trash_count(direction);
            break;
        case item_sram_flush:
            session_settings_cycle_sram_flush(direction);
            break;
        case item_precache:
            session_settings_cycle_content_precache(direction);
            break;
        default:
            break;
    }
}

static void closed(void) {
    settings_menu_reopen_storage();
}

static submenu self;

static submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .help = row_help,
    .row_count = 0,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .closed = closed,
    .save_title = lang.muxretro.save.storage_title,
    .save_desc = lang.muxretro.save.storage_desc,
};

static void build_rows(void) {
    row_count = 0;

    for (int item = 0; item < storage_item_count; item++) {
        if (!item_is_shown(item)) continue;

        row_labels[row_count] = item_labels[item];
        row_glyphs[row_count] = item_glyphs[item];
        row_help[row_count] = item_help[item];
        row_items[row_count] = item;
        row_count++;
    }

    def.row_count = row_count;
}

void storage_menu_init(void) {
    build_rows();
    submenu_init(&self, &def);
}

void storage_menu_open(void) {
    build_rows();
    submenu_open(&self);
}

int storage_menu_is_active(void) {
    return submenu_is_active(&self);
}

void storage_menu_tick(void) {
    submenu_tick(&self);
}

const submenu_def *storage_menu_definition(void) {
    build_rows();
    return &def;
}
