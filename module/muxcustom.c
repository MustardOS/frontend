#include "muxshare.h"
#include "muxcustom.h"
#include "ui/ui_muxcustom.h"
#include <string.h>
#include <dirent.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/input/list_nav.h"

static char theme_alt_original[MAX_BUFFER_SIZE];
static int boxart_original, bgm_original, sound_original, boxartalign_original, background_animation_original,
        font_original, launch_splash_original, black_fade_original, theme_resolution_original;

#define UI_COUNT 13
static lv_obj_t *ui_objects[UI_COUNT];
lv_obj_t *ui_objects_value[UI_COUNT];
static lv_obj_t *ui_icons[UI_COUNT];

static lv_obj_t *ui_mux_panels[5];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

struct theme_resolution {
    char *resolution;
    int value;
};

struct theme_resolution theme_resolutions[] = {
        {"640x480",  1},
        {"720x480",  2},
        {"720x576",  3},
        {"720x720",  4},
        {"1024x768", 5},
        {"1280x720", 6}
};

static int get_theme_resolution_value(char *resolution) {
    for (size_t i = 0; i < sizeof(theme_resolutions) / sizeof(theme_resolutions[0]); i++) {
        if (strcmp(resolution, theme_resolutions[i].resolution) == 0) return theme_resolutions[i].value;
    }

    return 0;
}

static void restore_theme_resolution() {
    for (size_t i = 0; i < sizeof(theme_resolutions) / sizeof(theme_resolutions[0]); i++) {
        if (theme_resolutions[i].value == config.SETTINGS.GENERAL.THEME_RESOLUTION) {
            int index = lv_dropdown_get_option_index(ui_droThemeResolution, theme_resolutions[i].resolution);
            lv_dropdown_set_selected(ui_droThemeResolution, index <= 0 ? 0 : index);
        }
    }
}

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblBackgroundAnimation, lang.MUXCUSTOM.HELP.ANIMATION},
            {ui_lblBGM,                 lang.MUXCUSTOM.HELP.MUSIC},
            {ui_lblSound,               lang.MUXCUSTOM.HELP.SOUND},
            {ui_lblBlackFade,           lang.MUXCUSTOM.HELP.FADE},
            {ui_lblBoxArt,              lang.MUXCUSTOM.HELP.BOX_ART},
            {ui_lblBoxArtAlign,         lang.MUXCUSTOM.HELP.BOX_ALIGN},
            {ui_lblFont,                lang.MUXCUSTOM.HELP.FONT},
            {ui_lblTheme,               lang.MUXCUSTOM.THEME},
            {ui_lblThemeResolution,     lang.MUXCUSTOM.HELP.THEME_RESOLUTION},
            {ui_lblThemeAlternate,      lang.MUXCUSTOM.THEME_ALTERNATE},
            {ui_lblCatalogue,           lang.MUXCUSTOM.CATALOGUE},
            {ui_lblConfig,              lang.MUXCUSTOM.CONFIG},
            {ui_lblLaunchSplash,        lang.MUXCUSTOM.HELP.SPLASH},
    };

    char *message = lang.GENERIC.NO_HELP;
    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);

    for (int i = 0; i < num_messages; i++) {
        if (element_focused == help_messages[i].element) {
            message = help_messages[i].message;
            break;
        }
    }

    if (strlen(message) <= 1) message = lang.GENERIC.NO_HELP;

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
}

static void populate_theme_alternates() {
    lv_dropdown_clear_options(ui_droThemeAlternate);
    char alt_path[MAX_BUFFER_SIZE];
    snprintf(alt_path, sizeof(alt_path), "%s/alternate", STORAGE_THEME);

    struct dirent *entry;
    DIR *dir = opendir(alt_path);

    if (dir == NULL) {
        perror("Unable to open directory");
    } else {

        while ((entry = readdir(dir)) != NULL) {
            char *filename = entry->d_name;
            size_t len = strlen(filename);

            if ((len > 4 && strcmp(str_tolower(filename + len - 4), ".ini") == 0) ||
                (len > 7 && strcmp(str_tolower(filename + len - 7), ".muxzip") == 0)) {
                char *name_without_ext = strip_ext(filename);
                if (!item_exists(items, item_count, name_without_ext)) {
                    add_item(&items, &item_count, name_without_ext, name_without_ext, "", ROM);
                }
            }
        }

        closedir(dir);
        sort_items(items, item_count);
        for (int i = 0; i < item_count; i++) {
            lv_dropdown_add_option(ui_droThemeAlternate, items[i].display_name, LV_DROPDOWN_POS_LAST);
        }
        free_items(&items, &item_count);
    }

    if (lv_dropdown_get_option_cnt(ui_droThemeAlternate) == 0) {
        lv_obj_add_flag(ui_pnlThemeAlternate, LV_OBJ_FLAG_HIDDEN);
        ui_count--;
    }
}

static void init_dropdown_settings() {
    bgm_original = lv_dropdown_get_selected(ui_droBGM);
    sound_original = lv_dropdown_get_selected(ui_droSound);
    boxart_original = lv_dropdown_get_selected(ui_droBoxArt);
    boxartalign_original = lv_dropdown_get_selected(ui_droBoxArtAlign);
    background_animation_original = lv_dropdown_get_selected(ui_droBackgroundAnimation);
    launch_splash_original = lv_dropdown_get_selected(ui_droLaunchSplash);
    black_fade_original = lv_dropdown_get_selected(ui_droBlackFade);
    font_original = lv_dropdown_get_selected(ui_droFont);
    theme_resolution_original = lv_dropdown_get_selected(ui_droThemeResolution);
}

static void init_navigation_group() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlCatalogue,
            ui_pnlConfig,
            ui_pnlTheme,
            ui_pnlThemeResolution,
            ui_pnlThemeAlternate,
            ui_pnlBackgroundAnimation,
            ui_pnlBGM,
            ui_pnlBlackFade,
            ui_pnlBoxArt,
            ui_pnlBoxArtAlign,
            ui_pnlLaunchSplash,
            ui_pnlFont,
            ui_pnlSound
    };

    ui_objects[0] = ui_lblCatalogue;
    ui_objects[1] = ui_lblConfig;
    ui_objects[2] = ui_lblTheme;
    ui_objects[3] = ui_lblThemeResolution;
    ui_objects[4] = ui_lblThemeAlternate;
    ui_objects[5] = ui_lblBackgroundAnimation;
    ui_objects[6] = ui_lblBGM;
    ui_objects[7] = ui_lblBlackFade;
    ui_objects[8] = ui_lblBoxArt;
    ui_objects[9] = ui_lblBoxArtAlign;
    ui_objects[10] = ui_lblLaunchSplash;
    ui_objects[11] = ui_lblFont;
    ui_objects[12] = ui_lblSound;

    ui_icons[0] = ui_icoCatalogue;
    ui_icons[1] = ui_icoConfig;
    ui_icons[2] = ui_icoTheme;
    ui_icons[3] = ui_icoThemeResolution;
    ui_icons[4] = ui_icoThemeAlternate;
    ui_icons[5] = ui_icoBackgroundAnimation;
    ui_icons[6] = ui_icoBGM;
    ui_icons[7] = ui_icoBlackFade;
    ui_icons[8] = ui_icoBoxArt;
    ui_icons[9] = ui_icoBoxArtAlign;
    ui_icons[10] = ui_icoLaunchSplash;
    ui_icons[11] = ui_icoFont;
    ui_icons[12] = ui_icoSound;

    ui_objects_value[0] = ui_droCatalogue;
    ui_objects_value[1] = ui_droConfig;
    ui_objects_value[2] = ui_droTheme;
    ui_objects_value[3] = ui_droThemeResolution;
    ui_objects_value[4] = ui_droThemeAlternate;
    ui_objects_value[5] = ui_droBackgroundAnimation;
    ui_objects_value[6] = ui_droBGM;
    ui_objects_value[7] = ui_droBlackFade;
    ui_objects_value[8] = ui_droBoxArt;
    ui_objects_value[9] = ui_droBoxArtAlign;
    ui_objects_value[10] = ui_droLaunchSplash;
    ui_objects_value[11] = ui_droFont;
    ui_objects_value[12] = ui_droSound;

    apply_theme_list_panel(ui_pnlThemeAlternate);
    apply_theme_list_panel(ui_pnlBackgroundAnimation);
    apply_theme_list_panel(ui_pnlBGM);
    apply_theme_list_panel(ui_pnlBlackFade);
    apply_theme_list_panel(ui_pnlCatalogue);
    apply_theme_list_panel(ui_pnlBoxArt);
    apply_theme_list_panel(ui_pnlBoxArtAlign);
    apply_theme_list_panel(ui_pnlLaunchSplash);
    apply_theme_list_panel(ui_pnlFont);
    apply_theme_list_panel(ui_pnlTheme);
    apply_theme_list_panel(ui_pnlThemeResolution);
    apply_theme_list_panel(ui_pnlSound);
    apply_theme_list_panel(ui_pnlConfig);

    apply_theme_list_item(&theme, ui_lblBackgroundAnimation, lang.MUXCUSTOM.ANIMATION);
    apply_theme_list_item(&theme, ui_lblBGM, lang.MUXCUSTOM.MUSIC.TITLE);
    apply_theme_list_item(&theme, ui_lblBlackFade, lang.MUXCUSTOM.FADE);
    apply_theme_list_item(&theme, ui_lblCatalogue, lang.MUXCUSTOM.CATALOGUE);
    apply_theme_list_item(&theme, ui_lblBoxArt, lang.MUXCUSTOM.BOX_ART.TITLE);
    apply_theme_list_item(&theme, ui_lblBoxArtAlign, lang.MUXCUSTOM.BOX_ART.ALIGN.TITLE);
    apply_theme_list_item(&theme, ui_lblLaunchSplash, lang.MUXCUSTOM.SPLASH);
    apply_theme_list_item(&theme, ui_lblFont, lang.MUXCUSTOM.FONT.TITLE);
    apply_theme_list_item(&theme, ui_lblTheme, lang.MUXCUSTOM.THEME);
    apply_theme_list_item(&theme, ui_lblThemeResolution, lang.MUXCUSTOM.THEME_RESOLUTION);
    apply_theme_list_item(&theme, ui_lblThemeAlternate, lang.MUXCUSTOM.THEME_ALTERNATE);
    apply_theme_list_item(&theme, ui_lblSound, lang.MUXCUSTOM.SOUND);
    apply_theme_list_item(&theme, ui_lblConfig, lang.MUXCUSTOM.CONFIG);

    apply_theme_list_glyph(&theme, ui_icoBackgroundAnimation, mux_module, "backgroundanimation");
    apply_theme_list_glyph(&theme, ui_icoBGM, mux_module, "bgm");
    apply_theme_list_glyph(&theme, ui_icoBlackFade, mux_module, "blackfade");
    apply_theme_list_glyph(&theme, ui_icoCatalogue, mux_module, "catalogue");
    apply_theme_list_glyph(&theme, ui_icoBoxArt, mux_module, "boxart");
    apply_theme_list_glyph(&theme, ui_icoBoxArtAlign, mux_module, "boxartalign");
    apply_theme_list_glyph(&theme, ui_icoLaunchSplash, mux_module, "launchsplash");
    apply_theme_list_glyph(&theme, ui_icoFont, mux_module, "font");
    apply_theme_list_glyph(&theme, ui_icoTheme, mux_module, "theme");
    apply_theme_list_glyph(&theme, ui_icoThemeResolution, mux_module, "theme_resolution");
    apply_theme_list_glyph(&theme, ui_icoThemeAlternate, mux_module, "themealternate");
    apply_theme_list_glyph(&theme, ui_icoSound, mux_module, "sound");
    apply_theme_list_glyph(&theme, ui_icoConfig, mux_module, "config");

    apply_theme_list_drop_down(&theme, ui_droBackgroundAnimation, NULL);
    apply_theme_list_drop_down(&theme, ui_droBGM, NULL);
    apply_theme_list_drop_down(&theme, ui_droBlackFade, NULL);
    apply_theme_list_drop_down(&theme, ui_droCatalogue, "");
    apply_theme_list_drop_down(&theme, ui_droBoxArt, NULL);
    apply_theme_list_drop_down(&theme, ui_droBoxArtAlign, NULL);
    apply_theme_list_drop_down(&theme, ui_droLaunchSplash, NULL);
    apply_theme_list_drop_down(&theme, ui_droFont, NULL);
    apply_theme_list_drop_down(&theme, ui_droTheme, "");
    apply_theme_list_drop_down(&theme, ui_droThemeResolution, NULL);
    apply_theme_list_drop_down(&theme, ui_droThemeAlternate, "");
    apply_theme_list_drop_down(&theme, ui_droSound, NULL);
    apply_theme_list_drop_down(&theme, ui_droConfig, "");

    add_drop_down_options(ui_droBGM, (char *[]) {
            lang.GENERIC.DISABLED, lang.MUXCUSTOM.MUSIC.GLOBAL, lang.MUXCUSTOM.MUSIC.THEME}, 3);

    add_drop_down_options(ui_droBoxArt, (char *[]) {
            lang.MUXCUSTOM.BOX_ART.BEHIND,
            lang.MUXCUSTOM.BOX_ART.FRONT,
            lang.MUXCUSTOM.BOX_ART.FS_BEHIND,
            lang.MUXCUSTOM.BOX_ART.FS_FRONT,
            lang.GENERIC.DISABLED}, 5);

    add_drop_down_options(ui_droBoxArtAlign, (char *[]) {
            lang.MUXCUSTOM.BOX_ART.ALIGN.T_LEFT,
            lang.MUXCUSTOM.BOX_ART.ALIGN.T_MID,
            lang.MUXCUSTOM.BOX_ART.ALIGN.T_RIGHT,
            lang.MUXCUSTOM.BOX_ART.ALIGN.B_LEFT,
            lang.MUXCUSTOM.BOX_ART.ALIGN.B_MID,
            lang.MUXCUSTOM.BOX_ART.ALIGN.B_RIGHT,
            lang.MUXCUSTOM.BOX_ART.ALIGN.M_LEFT,
            lang.MUXCUSTOM.BOX_ART.ALIGN.M_RIGHT,
            lang.MUXCUSTOM.BOX_ART.ALIGN.M_MID}, 9);

    add_drop_down_options(ui_droFont, (char *[]) {
            lang.MUXCUSTOM.FONT.LANG,
            lang.MUXCUSTOM.FONT.THEME}, 2);

    char *disabled_enabled[] = {lang.GENERIC.DISABLED, lang.GENERIC.ENABLED};
    add_drop_down_options(ui_droBackgroundAnimation, disabled_enabled, 2);
    add_drop_down_options(ui_droLaunchSplash, disabled_enabled, 2);
    add_drop_down_options(ui_droBlackFade, disabled_enabled, 2);
    add_drop_down_options(ui_droSound, disabled_enabled, 2);

    lv_dropdown_clear_options(ui_droThemeResolution);
    lv_dropdown_add_option(ui_droThemeResolution, lang.MUXCUSTOM.SCREEN, LV_DROPDOWN_POS_LAST);
    char theme_device_folder[MAX_BUFFER_SIZE];
    for (size_t i = 0; i < sizeof(theme_resolutions) / sizeof(theme_resolutions[0]); i++) {
        snprintf(theme_device_folder, sizeof(theme_device_folder), "%s/%s", STORAGE_THEME,
                 theme_resolutions[i].resolution);
        if (directory_exist(theme_device_folder)) {
            lv_dropdown_add_option(ui_droThemeResolution, theme_resolutions[i].resolution, LV_DROPDOWN_POS_LAST);
        }
    }

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    ui_count = sizeof(ui_objects) / sizeof(ui_objects[0]);
    for (unsigned int i = 0; i < ui_count; i++) {
        lv_obj_set_user_data(ui_objects_panel[i], strdup(lv_label_get_text(ui_objects[i])));
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_icons[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);

        apply_text_long_dot(&theme, ui_pnlContent, ui_objects[i], lv_label_get_text(ui_objects[i]));
    }

    populate_theme_alternates();
}

static void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_value, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    nav_moved = 1;
}

static void list_nav_next(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_value, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    nav_moved = 1;
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    play_sound("navigate", nav_sound, 0, 0);
    decrease_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    play_sound("navigate", nav_sound, 0, 0);
    increase_option_value(lv_group_get_focused(ui_group_value));
}

static void restore_options() {
    snprintf(theme_alt_original, sizeof(theme_alt_original), "%s",
             str_replace(read_line_from_file((STORAGE_THEME "/active.txt"), 1), "\r", ""));
    int32_t option_index = lv_dropdown_get_option_index(ui_droThemeAlternate, theme_alt_original);
    if (option_index > 0)
        lv_dropdown_set_selected(ui_droThemeAlternate, option_index);
    lv_dropdown_set_selected(ui_droBoxArt, config.VISUAL.BOX_ART);
    lv_dropdown_set_selected(ui_droBoxArtAlign, config.VISUAL.BOX_ART_ALIGN - 1);
    lv_dropdown_set_selected(ui_droBackgroundAnimation, config.VISUAL.BACKGROUNDANIMATION);
    lv_dropdown_set_selected(ui_droLaunchSplash, config.VISUAL.LAUNCHSPLASH);
    lv_dropdown_set_selected(ui_droBlackFade, config.VISUAL.BLACKFADE);
    lv_dropdown_set_selected(ui_droFont, config.SETTINGS.ADVANCED.FONT);
    lv_dropdown_set_selected(ui_droBGM, config.SETTINGS.GENERAL.BGM);
    lv_dropdown_set_selected(ui_droSound, config.SETTINGS.GENERAL.SOUND);
    restore_theme_resolution();
}

static void save_options() {
    int idx_boxart = lv_dropdown_get_selected(ui_droBoxArt);
    int idx_boxartalign = lv_dropdown_get_selected(ui_droBoxArtAlign) + 1;
    int idx_backgroundanimation = lv_dropdown_get_selected(ui_droBackgroundAnimation);
    int idx_launchsplash = lv_dropdown_get_selected(ui_droLaunchSplash);
    int idx_blackfade = lv_dropdown_get_selected(ui_droBlackFade);
    int idx_font = lv_dropdown_get_selected(ui_droFont);
    int idx_bgm = lv_dropdown_get_selected(ui_droBGM);
    int idx_sound = lv_dropdown_get_selected(ui_droSound);
    char theme_resolution[MAX_BUFFER_SIZE];
    lv_dropdown_get_selected_str(ui_droThemeResolution, theme_resolution, sizeof(theme_resolution));
    int idx_theme_resolution = get_theme_resolution_value(theme_resolution);

    int is_modified = 0;

    if (lv_dropdown_get_selected(ui_droBGM) != bgm_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/general/bgm"), "w", INT, idx_bgm);
    }

    if (lv_dropdown_get_selected(ui_droSound) != sound_original) {
        write_text_to_file((RUN_GLOBAL_PATH "settings/general/sound"), "w", INT, idx_sound);
    }

    if (lv_dropdown_get_selected(ui_droBoxArt) != boxart_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/boxart"), "w", INT, idx_boxart);
    }

    if (lv_dropdown_get_selected(ui_droBoxArtAlign) != boxartalign_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/boxartalign"), "w", INT, idx_boxartalign);
    }

    if (lv_dropdown_get_selected(ui_droBackgroundAnimation) != background_animation_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/backgroundanimation"), "w", INT, idx_backgroundanimation);
    }

    if (lv_dropdown_get_selected(ui_droLaunchSplash) != launch_splash_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/launchsplash"), "w", INT, idx_launchsplash);
    }

    if (lv_dropdown_get_selected(ui_droBlackFade) != black_fade_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/blackfade"), "w", INT, idx_blackfade);
    }

    if (lv_dropdown_get_selected(ui_droFont) != font_original) {
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/font"), "w", INT, idx_font);
    }

    if (lv_dropdown_get_selected(ui_droThemeResolution) != theme_resolution_original) {
        write_text_to_file((RUN_GLOBAL_PATH "settings/general/theme_resolution"), "w", INT, idx_theme_resolution);
        refresh_resolution = 1;
    }

    if (!lv_obj_has_flag(ui_pnlThemeAlternate, LV_OBJ_FLAG_HIDDEN)) {
        char theme_alt[MAX_BUFFER_SIZE];
        lv_dropdown_get_selected_str(ui_droThemeAlternate, theme_alt, sizeof(theme_alt));
        printf("attempt updating file: %s\n", (STORAGE_THEME "/active.txt"));
        if (strcasecmp(theme_alt, theme_alt_original) != 0) {
            write_text_to_file((STORAGE_THEME "/active.txt"), "w", CHAR, theme_alt);

            char theme_alt_archive[MAX_BUFFER_SIZE];
            snprintf(theme_alt_archive, sizeof(theme_alt_archive), "%s/alternate/%s.muxzip", STORAGE_THEME, theme_alt);
            if (file_exist(theme_alt_archive)) {
                extract_archive(theme_alt_archive);
                update_bootlogo();
            }

            static char rgb_script[MAX_BUFFER_SIZE];
            snprintf(rgb_script, sizeof(rgb_script),
                     "%s/alternate/rgb/%s/rgbconf.sh", STORAGE_THEME, theme_alt);
            if (file_exist(rgb_script)) {
                const char *args[] = {rgb_script, NULL};
                run_exec(args, A_SIZE(args));

                static char rgb_script_dest[MAX_BUFFER_SIZE];
                snprintf(rgb_script_dest, sizeof(rgb_script_dest), "%s/rgb/rgbconf.sh", STORAGE_THEME);
                create_directories(strip_dir(rgb_script_dest));
                write_text_to_file(rgb_script_dest, "w", CHAR, read_text_from_file(rgb_script));
            }
        }
    }

    refresh_config = 1;

    if (is_modified > 0) {
        const char *args[] = {INTERNAL_PATH "script/mux/tweak.sh", NULL};
        run_exec(args, A_SIZE(args));
    }
}

static void handle_confirm() {
    if (msgbox_active) return;

    struct {
        const char *mux_name;
        const char *launch;
        int16_t *kiosk_flag;
    } elements[] = {
            {"theme",     "theme",             &kiosk.CUSTOM.THEME},
            {"catalogue", "package/catalogue", &kiosk.CUSTOM.CATALOGUE},
            {"config",    "package/config",    &kiosk.CUSTOM.CONFIGURATION}
    };

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    const char *u_data = lv_obj_get_user_data(element_focused);

    save_options();
    for (size_t i = 0; i < sizeof(elements) / sizeof(elements[0]); i++) {
        if (strcasecmp(u_data, "themealternate") == 0) {
            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "themealternate");
            load_mux("custom");

            close_input();
            mux_input_stop();

            return;
        } else if (strcasecmp(u_data, elements[i].mux_name) == 0) {
            if (elements[i].kiosk_flag && *elements[i].kiosk_flag) {
                toast_message(kiosk_nope(), 1000, 1000);
                return;
            }

            write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, elements[i].mux_name);
            write_text_to_file(MUOS_PIK_LOAD, "w", CHAR, elements[i].launch);

            play_sound("confirm", nav_sound, 0, 1);
            load_mux("picker");

            close_input();
            mux_input_stop();

            break;
        }
    }

    handle_option_next();
}

static void handle_back() {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound("back", nav_sound, 0, 1);

    save_options();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "custom");

    close_input();
    mux_input_stop();
}

static void handle_help() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 0, 0);
        show_help(lv_group_get_focused(ui_group));
    }
}

static void init_elements() {
    ui_mux_panels[0] = ui_pnlFooter;
    ui_mux_panels[1] = ui_pnlHeader;
    ui_mux_panels[2] = ui_pnlHelp;
    ui_mux_panels[3] = ui_pnlProgressBrightness;
    ui_mux_panels[4] = ui_pnlProgressVolume;

    adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

    if (bar_footer) lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, "");

    lv_label_set_text(ui_lblNavA, lang.GENERIC.SELECT);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblBackgroundAnimation, "backgroundanimation");
    lv_obj_set_user_data(ui_lblBGM, "bgm");
    lv_obj_set_user_data(ui_lblBlackFade, "blackfade");
    lv_obj_set_user_data(ui_lblCatalogue, "catalogue");
    lv_obj_set_user_data(ui_lblBoxArt, "boxart");
    lv_obj_set_user_data(ui_lblBoxArtAlign, "boxartalign");
    lv_obj_set_user_data(ui_lblLaunchSplash, "launchsplash");
    lv_obj_set_user_data(ui_lblFont, "font");
    lv_obj_set_user_data(ui_lblTheme, "theme");
    lv_obj_set_user_data(ui_lblThemeResolution, "theme_resolution");
    lv_obj_set_user_data(ui_lblThemeAlternate, "themealternate");
    lv_obj_set_user_data(ui_lblSound, "sound");
    lv_obj_set_user_data(ui_lblConfig, "config");

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

static void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxcustom_main() {

    init_module("muxcustom");

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXCUSTOM.TITLE);
    init_muxcustom(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    init_navigation_sound(&nav_sound, mux_module);
    restore_options();
    init_dropdown_settings();

    load_kiosk(&kiosk);
    list_nav_next(direct_to_previous(ui_objects, UI_COUNT, &nav_moved));

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
