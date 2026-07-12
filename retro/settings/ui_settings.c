#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "submenu.h"

enum {
    row_hotkey_controls = 0,
    row_video,
    row_display,
    row_sound,
    row_input,
    row_performance,
    row_hud,
    row_storage,
    row_count
};

static const char *row_labels[row_count] = {
    lang.muxretro.hotkeys,
    lang.muxretro.settings_screen.category_video,
    lang.muxretro.display,
    lang.muxretro.settings_screen.category_sound,
    lang.muxretro.settings_screen.category_input,
    lang.muxretro.settings_screen.category_performance,
    lang.muxretro.settings_screen.category_hud,
    lang.muxretro.settings_screen.category_storage,
};

static const char *row_glyphs[row_count] = {"hotkeys",       "videosettings", "display",    "soundsettings",
                                            "inputsettings", "performance",   "screeninfo", "storagesettings"};

static int row_is_action(const int index) {
    (void) index;
    return 1;
}

static void row_action(const int index) {
    switch (index) {
        case row_hotkey_controls:
            hotkeys_menu_open();
            break;
        case row_video:
            video_menu_open();
            break;
        case row_display:
            display_menu_open();
            break;
        case row_sound:
            sound_menu_open();
            break;
        case row_input:
            input_menu_open();
            break;
        case row_performance:
            performance_menu_open();
            break;
        case row_hud:
            hud_menu_open();
            break;
        case row_storage:
            storage_menu_open();
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

    if (video_menu_is_active()) {
        video_menu_tick();
        return 1;
    }

    if (display_menu_is_active()) {
        display_menu_tick();
        return 1;
    }

    if (sound_menu_is_active()) {
        sound_menu_tick();
        return 1;
    }

    if (input_menu_is_active()) {
        input_menu_tick();
        return 1;
    }

    if (performance_menu_is_active()) {
        performance_menu_tick();
        return 1;
    }

    if (hud_menu_is_active()) {
        hud_menu_tick();
        return 1;
    }

    if (storage_menu_is_active()) {
        storage_menu_tick();
        return 1;
    }

    return 0;
}

static void closed(void) {
    pause_menu_rebuild();
    pause_menu_focus_settings_item();
    pause_menu_show_nav_hints();

    pause_menu_sync_input_mask();
}

static submenu self;

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .row_count = row_count,
    .row_is_action = row_is_action,
    .action = row_action,
    .child_tick = child_tick,
    .closed = closed,
    .save_title = lang.muxretro.save.settings_title,
    .save_desc = lang.muxretro.save.settings_desc,
};

void settings_menu_init(void) {
    submenu_init(&self, &def);

    hotkeys_menu_init();
    video_menu_init();
    display_menu_init();
    sound_menu_init();
    input_menu_init();
    performance_menu_init();
    hud_menu_init();
    storage_menu_init();
}

void settings_menu_open(void) {
    submenu_open(&self);
}

int settings_menu_is_active(void) {
    return submenu_is_active(&self);
}

void settings_menu_tick(void) {
    submenu_tick(&self);
}

void settings_menu_reopen_hotkeys(void) {
    submenu_reopen_at(&self, row_hotkey_controls);
}

void settings_menu_reopen_video(void) {
    submenu_reopen_at(&self, row_video);
}

void settings_menu_reopen_display(void) {
    submenu_reopen_at(&self, row_display);
}

void settings_menu_reopen_sound(void) {
    submenu_reopen_at(&self, row_sound);
}

void settings_menu_reopen_input(void) {
    submenu_reopen_at(&self, row_input);
}

void settings_menu_reopen_performance(void) {
    submenu_reopen_at(&self, row_performance);
}

void settings_menu_reopen_hud(void) {
    submenu_reopen_at(&self, row_hud);
}

void settings_menu_reopen_storage(void) {
    submenu_reopen_at(&self, row_storage);
}
