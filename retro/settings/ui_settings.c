#include "../../module/muxshare.h"
#include "../cheevo/ui_cheevo.h"
#include "../netplay/netplay.h"
#include "../core/muxretro.h"
#include "submenu.h"

typedef enum {
    settings_item_video = 0,
    settings_item_sound,
    settings_item_input,
    settings_item_storage,
    settings_item_cheevo,
    settings_item_advanced,
    settings_item_save,
    settings_item_count
} settings_item;

typedef enum {
    advanced_item_core_options = 0,
    advanced_item_performance,
    advanced_item_hud,
    advanced_item_count
} advanced_item;

static const char *row_labels[settings_item_count];
static const char *row_glyphs[settings_item_count];
static const char *row_help[settings_item_count];
static settings_item row_items[settings_item_count];
static int row_count;

static const char *advanced_labels[advanced_item_count];
static const char *advanced_glyphs[advanced_item_count];
static const char *advanced_help[advanced_item_count];
static advanced_item advanced_items[advanced_item_count];
static int advanced_count;

static submenu self;
static submenu advanced_self;

static void add_row(const settings_item item, const char *label, const char *glyph, const char *help) {
    row_items[row_count] = item;
    row_labels[row_count] = label;
    row_glyphs[row_count] = glyph;
    row_help[row_count] = help;
    row_count++;
}

static void add_advanced_row(const advanced_item item, const char *label, const char *glyph, const char *help) {
    advanced_items[advanced_count] = item;
    advanced_labels[advanced_count] = label;
    advanced_glyphs[advanced_count] = glyph;
    advanced_help[advanced_count] = help;
    advanced_count++;
}

static settings_item row_item(const int index) {
    return index >= 0 && index < row_count ? row_items[index] : settings_item_count;
}

static advanced_item advanced_row_item(const int index) {
    return index >= 0 && index < advanced_count ? advanced_items[index] : advanced_item_count;
}

static int row_for_item(const settings_item item) {
    for (int index = 0; index < row_count; index++)
        if (row_items[index] == item) return index;
    return -1;
}

static int advanced_row_for_item(const advanced_item item) {
    for (int index = 0; index < advanced_count; index++)
        if (advanced_items[index] == item) return index;
    return -1;
}

static int row_is_action(const int index) {
    return row_item(index) != settings_item_count;
}

static int row_is_save(const int index) {
    return row_item(index) == settings_item_save;
}

static int advanced_row_is_action(const int index) {
    return advanced_row_item(index) != advanced_item_count;
}

static void build_advanced_rows(void);

static void advanced_menu_open(void) {
    build_advanced_rows();
    submenu_open(&advanced_self);
}

static void row_action(const int index) {
    switch (row_item(index)) {
        case settings_item_video:
            video_menu_open();
            break;
        case settings_item_sound:
            sound_menu_open();
            break;
        case settings_item_input:
            input_menu_open();
            break;
        case settings_item_storage:
            storage_menu_open();
            break;
        case settings_item_cheevo:
            cheevo_settings_menu_open();
            break;
        case settings_item_advanced:
            advanced_menu_open();
            break;
        default:
            break;
    }
}

static void advanced_row_action(const int index) {
    switch (advanced_row_item(index)) {
        case advanced_item_core_options:
            options_menu_open();
            break;
        case advanced_item_performance:
            performance_menu_open();
            break;
        case advanced_item_hud:
            hud_menu_open();
            break;
        default:
            break;
    }
}

static int advanced_child_tick(void) {
    if (options_menu_is_active()) {
        options_menu_tick();
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

    return 0;
}

static int child_tick(void) {
    if (submenu_is_active(&advanced_self)) {
        submenu_tick(&advanced_self);
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

static void advanced_closed(void) {
    submenu_reopen_at(&self, row_for_item(settings_item_advanced));
}

static void closed(void) {
    pause_menu_rebuild();
    pause_menu_focus_settings_item();
    pause_menu_show_nav_hints();
    pause_menu_sync_input_mask();
}

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

static submenu_def advanced_def = {
    .labels = advanced_labels,
    .glyphs = advanced_glyphs,
    .help = advanced_help,
    .row_count = 0,
    .row_is_action = advanced_row_is_action,
    .action = advanced_row_action,
    .child_tick = advanced_child_tick,
    .closed = advanced_closed,
    .save_title = lang.muxretro.save.settings_title,
    .save_desc = lang.muxretro.save.settings_desc,
    .skip_value_object_creation = 1,
};

static void build_rows(void) {
    row_count = 0;
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
        settings_item_storage, lang.muxretro.settings_screen.category_storage, "storagesettings",
        lang.muxretro.help.settings.storage
    );
    if (device.board.has_network)
        add_row(settings_item_cheevo, lang.muxretro.retroachievements, "trophy", lang.muxretro.help.settings.cheevo);
    add_row(
        settings_item_advanced, lang.muxretro.settings_screen.category_advanced, "settings",
        lang.muxretro.help.settings.advanced
    );
    add_row(settings_item_save, lang.muxretro.save_settings, "settings", lang.muxretro.help.settings.save_all);
    def.row_count = row_count;
}

static void build_advanced_rows(void) {
    advanced_count = 0;
    if (!netplay_is_active())
        add_advanced_row(
            advanced_item_core_options, lang.muxretro.core_options, "core", lang.muxretro.help.settings.core_options
        );
    add_advanced_row(
        advanced_item_performance, lang.muxretro.settings_screen.category_performance, "performance",
        lang.muxretro.help.settings.performance
    );
    add_advanced_row(
        advanced_item_hud, lang.muxretro.settings_screen.category_hud, "screeninfo",
        lang.muxretro.help.settings.hud
    );
    advanced_def.row_count = advanced_count;
}

void settings_menu_init(void) {
    build_rows();
    build_advanced_rows();

    submenu_init(&self, &def);
    submenu_init(&advanced_self, &advanced_def);

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
    build_advanced_rows();
    submenu_reopen_at(&advanced_self, advanced_row_for_item(advanced_item_core_options));
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
    submenu_reopen_at(&advanced_self, advanced_row_for_item(advanced_item_performance));
}

void settings_menu_reopen_hud(void) {
    submenu_reopen_at(&advanced_self, advanced_row_for_item(advanced_item_hud));
}

void settings_menu_reopen_storage(void) {
    submenu_reopen_at(&self, row_for_item(settings_item_storage));
}

void settings_menu_reopen_cheevo(void) {
    const int row = row_for_item(settings_item_cheevo);
    if (row >= 0) submenu_reopen_at(&self, row);
}
