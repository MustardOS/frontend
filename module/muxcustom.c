#include "../lvgl/lvgl.h"
#include "ui/ui_muxcustom.h"
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <libgen.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/collection.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/input.h"
#include "../common/input/list_nav.h"

char *mux_module;

static int joy_general;
static int joy_power;
static int joy_volume;
static int joy_extra;

int turbo_mode = 0;
int msgbox_active = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int nav_moved = 1;
int current_item_index = 0;
int ui_count = 0;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;
lv_obj_t *kiosk_image = NULL;

int progress_onscreen = -1;

char theme_alt_original[MAX_BUFFER_SIZE];
int boxart_original, bgm_original, sound_original, boxartalign_original, background_animation_original, 
    font_original, launch_splash_original, black_fade_original;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

#define UI_COUNT 12
lv_obj_t *ui_objects[UI_COUNT];
lv_obj_t *ui_objects_value[UI_COUNT];
lv_obj_t *ui_icons[UI_COUNT];

lv_obj_t *ui_mux_panels[5];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblBackgroundAnimation, lang.MUXCUSTOM.HELP.ANIMATION},
            {ui_lblBGM,                 lang.MUXCUSTOM.HELP.MUSIC},
            {ui_lblSound,               lang.MUXCUSTOM.HELP.SOUND},
            {ui_lblBlackFade,           lang.MUXCUSTOM.HELP.FADE},
            {ui_lblBoxArt,              lang.MUXCUSTOM.HELP.BOX_ART},
            {ui_lblBoxArtAlign,         lang.MUXCUSTOM.HELP.BOX_ALIGN},
            {ui_lblFont,                lang.MUXCUSTOM.HELP.FONT},
            {ui_lblTheme,               lang.MUXCUSTOM.THEME},
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

void populate_theme_alternates() {
    lv_dropdown_clear_options(ui_droThemeAlternate);
    char alt_path[MAX_BUFFER_SIZE];
    snprintf(alt_path, sizeof(alt_path), "%s/alternate", STORAGE_THEME);

   struct dirent *entry;
    DIR *dir = opendir(alt_path);

    if (dir == NULL) {
        perror("Unable to open directory");
    } else {
        size_t item_count = 0;
        content_item *items = NULL;

        while ((entry = readdir(dir)) != NULL) {
            char *filename = entry->d_name;
            size_t len = strlen(filename);

            if (len > 4 && strcmp(str_tolower(filename + len - 4), ".ini") == 0) {
                char name_without_ext[MAX_BUFFER_SIZE];
                strncpy(name_without_ext, filename, len - 4);
                name_without_ext[len - 4] = '\0';
                add_item(&items, &item_count, "", name_without_ext, "", ROM);
            }
        }

        closedir(dir);
        sort_items(items, item_count);
        for (int i = 0; i < item_count; i++) {
            lv_dropdown_add_option(ui_droThemeAlternate, items[i].display_name, LV_DROPDOWN_POS_LAST);
        }
        free_items(items, item_count);
    }

    if (lv_dropdown_get_option_cnt(ui_droThemeAlternate) == 0) {
        lv_obj_add_flag(ui_pnlThemeAlternate, LV_OBJ_FLAG_HIDDEN);
        ui_count--;
    }
}

void init_dropdown_settings() {
    bgm_original = lv_dropdown_get_selected(ui_droBGM);
    sound_original = lv_dropdown_get_selected(ui_droSound);
    boxart_original = lv_dropdown_get_selected(ui_droBoxArt);
    boxartalign_original = lv_dropdown_get_selected(ui_droBoxArtAlign);
    background_animation_original = lv_dropdown_get_selected(ui_droBackgroundAnimation);
    launch_splash_original = lv_dropdown_get_selected(ui_droLaunchSplash);
    black_fade_original = lv_dropdown_get_selected(ui_droBlackFade);
    font_original = lv_dropdown_get_selected(ui_droFont);
}

void init_navigation_group() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlCatalogue,
            ui_pnlTheme,
            ui_pnlConfig,
            ui_pnlBackgroundAnimation,
            ui_pnlBGM,
            ui_pnlBlackFade,
            ui_pnlBoxArt,
            ui_pnlBoxArtAlign,
            ui_pnlLaunchSplash,
            ui_pnlFont,
            ui_pnlThemeAlternate,
            ui_pnlSound
    };

    ui_objects[0] =  ui_lblCatalogue;
    ui_objects[1] =  ui_lblTheme;
    ui_objects[2] =  ui_lblConfig;
    ui_objects[3] =  ui_lblBackgroundAnimation;
    ui_objects[4] =  ui_lblBGM;
    ui_objects[5] =  ui_lblBlackFade;
    ui_objects[6] =  ui_lblBoxArt;
    ui_objects[7] =  ui_lblBoxArtAlign;
    ui_objects[8] =  ui_lblLaunchSplash;
    ui_objects[9] =  ui_lblFont;
    ui_objects[10] = ui_lblThemeAlternate;
    ui_objects[11] = ui_lblSound;

    ui_icons[0] =  ui_icoCatalogue;
    ui_icons[1] =  ui_icoTheme;
    ui_icons[2] =  ui_icoConfig;
    ui_icons[3] =  ui_icoBackgroundAnimation;
    ui_icons[4] =  ui_icoBGM;
    ui_icons[5] =  ui_icoBlackFade;
    ui_icons[6] =  ui_icoBoxArt;
    ui_icons[7] =  ui_icoBoxArtAlign;
    ui_icons[8] =  ui_icoLaunchSplash;
    ui_icons[9] =  ui_icoFont;
    ui_icons[10] = ui_icoThemeAlternate;
    ui_icons[11] = ui_icoSound;

    ui_objects_value[0] =  ui_droCatalogue;
    ui_objects_value[1] =  ui_droTheme;
    ui_objects_value[2] =  ui_droConfig;
    ui_objects_value[3] =  ui_droBackgroundAnimation;
    ui_objects_value[4] =  ui_droBGM;
    ui_objects_value[5] =  ui_droBlackFade;
    ui_objects_value[6] =  ui_droBoxArt;
    ui_objects_value[7] =  ui_droBoxArtAlign;
    ui_objects_value[8] =  ui_droLaunchSplash;
    ui_objects_value[9] =  ui_droFont;
    ui_objects_value[10] = ui_droThemeAlternate;
    ui_objects_value[11] = ui_droSound;

    apply_theme_list_panel(ui_pnlBackgroundAnimation);
    apply_theme_list_panel(ui_pnlBGM);
    apply_theme_list_panel(ui_pnlBlackFade);
    apply_theme_list_panel(ui_pnlCatalogue);
    apply_theme_list_panel(ui_pnlBoxArt);
    apply_theme_list_panel(ui_pnlBoxArtAlign);
    apply_theme_list_panel(ui_pnlLaunchSplash);
    apply_theme_list_panel(ui_pnlFont);
    apply_theme_list_panel(ui_pnlTheme);
    apply_theme_list_panel(ui_pnlThemeAlternate);
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

        apply_size_to_content(&theme, ui_pnlContent, ui_objects[i], ui_icons[i], lv_label_get_text(ui_objects[i]));
        apply_text_long_dot(&theme, ui_pnlContent, ui_objects[i], lv_label_get_text(ui_objects[i]));
    }
    populate_theme_alternates();
}

void list_nav_prev(int steps) {
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

void list_nav_next(int steps) {
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

void handle_option_prev(void) {
    if (msgbox_active) return;

    play_sound("navigate", nav_sound, 0, 0);
    decrease_option_value(lv_group_get_focused(ui_group_value));
}

void handle_option_next(void) {
    if (msgbox_active) return;

    play_sound("navigate", nav_sound, 0, 0);
    increase_option_value(lv_group_get_focused(ui_group_value));
}

void restore_options() {
    snprintf(theme_alt_original, sizeof(theme_alt_original), "%s", str_replace(read_line_from_file((STORAGE_THEME "/active.txt"),1), "\r", ""));
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
}

void save_options() {
    int idx_boxart = lv_dropdown_get_selected(ui_droBoxArt);
    int idx_boxartalign = lv_dropdown_get_selected(ui_droBoxArtAlign) + 1;
    int idx_backgroundanimation = lv_dropdown_get_selected(ui_droBackgroundAnimation);
    int idx_launchsplash = lv_dropdown_get_selected(ui_droLaunchSplash);
    int idx_blackfade = lv_dropdown_get_selected(ui_droBlackFade);
    int idx_font = lv_dropdown_get_selected(ui_droFont);
    int idx_bgm = lv_dropdown_get_selected(ui_droBGM);
    int idx_sound = lv_dropdown_get_selected(ui_droSound);

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

    if (!lv_obj_has_flag(ui_pnlThemeAlternate, LV_OBJ_FLAG_HIDDEN)) {
        char theme_alt[MAX_BUFFER_SIZE];
        lv_dropdown_get_selected_str(ui_droThemeAlternate, theme_alt, sizeof(theme_alt));
        printf("attempt updating file: %s\n", (STORAGE_THEME "/active.txt"));
        if (strcasecmp(theme_alt, theme_alt_original) != 0) {
            write_text_to_file((STORAGE_THEME "/active.txt"), "w", CHAR, theme_alt);
        }
    }

    if (is_modified > 0) run_exec((const char *[]) {(char *) INTERNAL_PATH "script/mux/tweak.sh", NULL});
}

void handle_confirm() {
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
            mux_input_stop();
            break;
        }
    }
    
    handle_option_next();
}

void handle_back() {
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
    mux_input_stop();
}

void handle_help() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 0, 0);
        show_help(lv_group_get_focused(ui_group));
    }
}

void init_elements() {
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
            ui_lblNavCGlyph,
            ui_lblNavC,
            ui_lblNavXGlyph,
            ui_lblNavX,
            ui_lblNavYGlyph,
            ui_lblNavY,
            ui_lblNavZGlyph,
            ui_lblNavZ,
            ui_lblNavMenuGlyph,
            ui_lblNavMenu
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
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
    lv_obj_set_user_data(ui_lblThemeAlternate, "themealternate");
    lv_obj_set_user_data(ui_lblSound, "sound");
    lv_obj_set_user_data(ui_lblConfig, "config");

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image, theme.MISC.IMAGE_OVERLAY);
}

void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    (void) argc;

    mux_module = basename(argv[0]);
    setup_background_process();

    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    init_display();
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXCUSTOM.TITLE);
    init_mux(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    init_navigation_sound(&nav_sound, mux_module);
    restore_options();
    init_dropdown_settings();

    init_input(&joy_general, &joy_power, &joy_volume, &joy_extra);
    init_timer(ui_refresh_task, NULL);

    load_kiosk(&kiosk);
    list_nav_next(direct_to_previous(ui_objects, UI_COUNT, &nav_moved));

    mux_input_options input_opts = {
            .general_fd = joy_general,
            .power_fd = joy_power,
            .volume_fd = joy_volume,
            .extra_fd = joy_extra,
            .max_idle_ms = IDLE_MS,
            .swap_btn = config.SETTINGS.ADVANCED.SWAP,
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .stick_nav = true,
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
            },
            .combo = {
                    COMBO_BRIGHT(BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_UP)),
                    COMBO_BRIGHT(BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_DOWN)),
                    COMBO_VOLUME(BIT(MUX_INPUT_VOL_UP)),
                    COMBO_VOLUME(BIT(MUX_INPUT_VOL_DOWN)),
            },
            .idle_handler = ui_common_handle_idle,
    };
    mux_input_task(&input_opts);
    safe_quit();

    close(joy_general);
    close(joy_power);
    close(joy_volume);
    close(joy_extra);

    return 0;
}
