#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

enum { row_adjustment = 0, row_cropping, row_reset, row_count };

static const char *row_labels[row_count] = {
    lang.muxretro.viewport_screen.adjustment, lang.muxretro.viewport_screen.cropping,
    lang.muxretro.viewport_screen.reset
};

static const char *row_glyphs[row_count] = {"viewport", "centrecrop", "viewportreset"};

static int row_is_action(const int index) {
    (void) index;
    return 1;
}

static void row_action(const int index) {
    switch (index) {
        case row_adjustment:
            viewport_adjust_menu_open();
            break;
        case row_cropping:
            viewport_crop_menu_open();
            break;
        case row_reset:
            session_settings_reset_viewport();
            break;
        default:
            break;
    }
}

static submenu self;

static int child_tick(void) {
    if (viewport_adjust_menu_is_active()) {
        viewport_adjust_menu_tick();
        return 1;
    }

    if (viewport_crop_menu_is_active()) {
        viewport_crop_menu_tick();
        return 1;
    }

    return 0;
}

static void closed(void) {
    video_menu_reopen_viewport();
}

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .row_count = row_count,
    .row_is_action = row_is_action,
    .action = row_action,
    .child_tick = child_tick,
    .closed = closed,
    .save_title = lang.muxretro.save.viewport_title,
    .save_desc = lang.muxretro.save.viewport_desc,
};

void viewport_menu_init(void) {
    submenu_init(&self, &def);

    viewport_adjust_menu_init();
    viewport_crop_menu_init();
}

void viewport_menu_open(void) {
    submenu_open(&self);
}

int viewport_menu_is_active(void) {
    return submenu_is_active(&self);
}

void viewport_menu_tick(void) {
    submenu_tick(&self);
}

void viewport_menu_reopen_adjustment(void) {
    submenu_reopen_at(&self, row_adjustment);
}

void viewport_menu_reopen_cropping(void) {
    submenu_reopen_at(&self, row_cropping);
}
