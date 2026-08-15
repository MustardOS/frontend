#include "../../module/muxshare.h"
#include "../cheevo/ui_cheevo.h"
#include "../netplay/netplay.h"
#include "../core/muxretro.h"
#include "submenu.h"

typedef enum {
    settings_item_core_options = 0,
    settings_item_video,
    settings_item_sound,
    settings_item_input,
    settings_item_performance,
    settings_item_hud,
    settings_item_storage,
    settings_item_cheevo,
    settings_item_save,
    settings_item_count
} settings_item;

static const char *row_labels[settings_item_count];
static const char *row_glyphs[settings_item_count];
static const char *row_help[settings_item_count];
static settings_item row_items[settings_item_count];
static int row_count;

static void add_row(const settings_item item, const char *label, const char *glyph, const char *help) {
    row_items[row_count] = item;
    row_labels[row_count] = label;
    row_glyphs[row_count] = glyph;
    row_help[row_count] = help;
    row_count++;
}

static settings_item row_item(const int index) {
    return index >= 0 && index < row_count ? row_items[index] : settings_item_count;
}

static int row_for_item(const settings_item item) {
    for (int index = 0; index < row_count; index++)
        if (row_items[index] == item) return index;
    return -1;
}

static int row_is_action(const int index) {
    return row_item(index) != settings_item_count;
}

static int row_is_save(const int index) {
    return row_item(index) == settings_item_save;
}

static void row_action(const int index) {
    switch (row_item(index)) {
        case settings_item_core_options:
            options_menu_open();
            break;
        case settings_item_video:
            video_menu_open();
            break;
        case settings_item_sound:
            sound_menu_open();
            break;
        case settings_item_input:
            input_menu_open();
            break;
        case settings_item_performance:
            performance_menu_open();
            break;
        case settings_item_hud:
            hud_menu_open();
            break;
        case settings_item_storage:
            storage_menu_open();
            break;
        case settings_item_cheevo:
            cheevo_settings_menu_open();
            break;
        default:
            break;
    }
}

static int child_tick(void) {
    if (options_menu_is_active()) {
        options_menu_tick();
        return 1;
    }

    if (video_menu_is_active()) {
        video_menu_tick();
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

    if (cheevo_settings_menu_is_active()) {
        cheevo_settings_menu_tick();
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

static submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .help = row_help,
    .row_count = 0,
    .row_is_action = row_is_action,
    .row_is_save = row_is_save,
    .action = row_action,
    .child_tick = child_tick,
    .closed = closed,
    .save_title = lang.muxretro.save.settings_title,
    .save_desc = lang.muxretro.save.settings_desc,
    .skip_value_object_creation = 1,
};

static void build_rows(void) {
    row_count = 0;
    if (!netplay_is_active())
        add_row(
            settings_item_core_options, lang.muxretro.core_options, "core", lang.muxretro.help.settings.core_options
        );
    add_row(
        settings_item_video, lang.muxretro.settings_screen.category_video, "videosettings",
        lang.muxretro.help.settings.video
    );
    add_row(
        settings_item_sound, lang.muxretro.settings_screen.category_sound, "soundsettings",
        lang.muxretro.help.settings.sound
    );
    add_row(
        settings_item_input, lang.muxretro.settings_screen.category_input, "inputsettings",
        lang.muxretro.help.settings.input
    );
    add_row(
        settings_item_performance, lang.muxretro.settings_screen.category_performance, "performance",
        lang.muxretro.help.settings.performance
    );
    add_row(
        settings_item_hud, lang.muxretro.settings_screen.category_hud, "screeninfo", lang.muxretro.help.settings.hud
    );
    add_row(
        settings_item_storage, lang.muxretro.settings_screen.category_storage, "storagesettings",
        lang.muxretro.help.settings.storage
    );
    if (device.board.has_network)
        add_row(settings_item_cheevo, lang.muxretro.retroachievements, "trophy", lang.muxretro.help.settings.cheevo);
    add_row(settings_item_save, lang.muxretro.save_settings, "settings", lang.muxretro.help.settings.save_all);
    def.row_count = row_count;
}

void settings_menu_init(void) {
    build_rows();

    submenu_init(&self, &def);

    options_menu_init();

    video_menu_init();
    sound_menu_init();
    input_menu_init();
    performance_menu_init();
    hud_menu_init();
    storage_menu_init();
}

void settings_menu_open(void) {
    build_rows();
    submenu_open(&self);
}

void settings_menu_reopen_core_options(void) {
    submenu_reopen_at(&self, row_for_item(settings_item_core_options));
}

int settings_menu_is_active(void) {
    return submenu_is_active(&self);
}

void settings_menu_tick(void) {
    submenu_tick(&self);
}

void settings_menu_reopen_video(void) {
    submenu_reopen_at(&self, row_for_item(settings_item_video));
}

void settings_menu_reopen_sound(void) {
    submenu_reopen_at(&self, row_for_item(settings_item_sound));
}

void settings_menu_reopen_input(void) {
    submenu_reopen_at(&self, row_for_item(settings_item_input));
}

void settings_menu_reopen_performance(void) {
    submenu_reopen_at(&self, row_for_item(settings_item_performance));
}

void settings_menu_reopen_hud(void) {
    submenu_reopen_at(&self, row_for_item(settings_item_hud));
}

void settings_menu_reopen_storage(void) {
    submenu_reopen_at(&self, row_for_item(settings_item_storage));
}

void settings_menu_reopen_cheevo(void) {
    const int row = row_for_item(settings_item_cheevo);
    if (row >= 0) submenu_reopen_at(&self, row);
}
