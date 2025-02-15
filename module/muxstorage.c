#include "../lvgl/lvgl.h"
#include "ui/ui_muxstorage.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
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

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

#define UI_COUNT 18
lv_obj_t *ui_objects[UI_COUNT];

lv_obj_t *ui_mux_panels[5];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

struct storage {
    const char *path_suffix;
    lv_obj_t *ui_label;
};

struct storage storage_path[UI_COUNT];

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblBIOS,             lang.MUXSTORAGE.HELP.BIOS},
            {ui_lblCatalogue,        lang.MUXSTORAGE.HELP.CATALOGUE},
            {ui_lblName,             lang.MUXSTORAGE.HELP.FRIENDLY},
            {ui_lblRetroArch,        lang.MUXSTORAGE.HELP.RA_SYSTEM},
            {ui_lblConfig,           lang.MUXSTORAGE.HELP.RA_CONFIG},
            {ui_lblCore,             lang.MUXSTORAGE.HELP.ASSIGNED},
            {ui_lblCollection,       lang.MUXSTORAGE.HELP.COLLECTION},
            {ui_lblHistory,          lang.MUXSTORAGE.HELP.HISTORY},
            {ui_lblMusic,            lang.MUXSTORAGE.HELP.MUSIC},
            {ui_lblSave,             lang.MUXSTORAGE.HELP.SAVE},
            {ui_lblScreenshot,       lang.MUXSTORAGE.HELP.SCREENSHOT},
            {ui_lblTheme,            lang.MUXSTORAGE.HELP.PACKAGE.THEME},
            {ui_lblCataloguePackage, lang.MUXSTORAGE.HELP.PACKAGE.CATALOGUE},
            {ui_lblConfigPackage,    lang.MUXSTORAGE.HELP.PACKAGE.RA_CONFIG},
            {ui_lblLanguage,         lang.MUXSTORAGE.HELP.LANGUAGE},
            {ui_lblNetwork,          lang.MUXSTORAGE.HELP.NET_PROFILE},
            {ui_lblSyncthing,        lang.MUXSTORAGE.HELP.SYNCTHING},
            {ui_lblUserInit,         lang.MUXSTORAGE.HELP.USER_INIT},
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

void update_storage_info() {
    /*
     * Check for SD2 pathing, otherwise it should be on SD1.
     * If it's not on SD1 then you have bigger problems!
    */
    storage_path[0].path_suffix = STORE_LOC_BIOS;
    storage_path[0].ui_label = ui_lblBIOSValue;

    storage_path[1].path_suffix = STORE_LOC_CLOG;
    storage_path[1].ui_label = ui_lblCatalogueValue;

    storage_path[2].path_suffix = STORE_LOC_NAME;
    storage_path[2].ui_label = ui_lblNameValue;

    storage_path[3].path_suffix = STORE_LOC_RARC;
    storage_path[3].ui_label = ui_lblRetroArchValue;

    storage_path[4].path_suffix = STORE_LOC_CONF;
    storage_path[4].ui_label = ui_lblConfigValue;

    storage_path[5].path_suffix = STORE_LOC_CORE;
    storage_path[5].ui_label = ui_lblCoreValue;

    storage_path[6].path_suffix = STORE_LOC_COLL;
    storage_path[6].ui_label = ui_lblCollectionValue;

    storage_path[7].path_suffix = STORE_LOC_HIST;
    storage_path[7].ui_label = ui_lblHistoryValue;

    storage_path[8].path_suffix = STORE_LOC_MUSC;
    storage_path[8].ui_label = ui_lblMusicValue;

    storage_path[9].path_suffix = STORE_LOC_SAVE;
    storage_path[9].ui_label = ui_lblSaveValue;

    storage_path[10].path_suffix = STORE_LOC_SCRS;
    storage_path[10].ui_label = ui_lblScreenshotValue;

    storage_path[11].path_suffix = STORE_LOC_THEM;
    storage_path[11].ui_label = ui_lblThemeValue;

    storage_path[12].path_suffix = STORE_LOC_PCAT;
    storage_path[12].ui_label = ui_lblCataloguePackageValue;

    storage_path[13].path_suffix = STORE_LOC_PCON;
    storage_path[13].ui_label = ui_lblConfigPackageValue;

    storage_path[14].path_suffix = STORE_LOC_LANG;
    storage_path[14].ui_label = ui_lblLanguageValue;

    storage_path[15].path_suffix = STORE_LOC_NETW;
    storage_path[15].ui_label = ui_lblNetworkValue;

    storage_path[16].path_suffix = STORE_LOC_SYCT;
    storage_path[16].ui_label = ui_lblSyncthingValue;

    storage_path[17].path_suffix = STORE_LOC_INIT;
    storage_path[17].ui_label = ui_lblUserInitValue;

    char dir[FILENAME_MAX];
    for (int i = 0; i < sizeof(storage_path) / sizeof(storage_path[0]); i++) {
        snprintf(dir, sizeof(dir), "%s/%s", device.STORAGE.SDCARD.MOUNT, storage_path[i].path_suffix);
        if (directory_exist(dir)) {
            lv_label_set_text(storage_path[i].ui_label, "SD2");
            lv_label_set_text(ui_lblNavX, lang.GENERIC.SYNC);
        } else {
            lv_label_set_text(storage_path[i].ui_label, "SD1");
            lv_label_set_text(ui_lblNavX, lang.GENERIC.MIGRATE);
        }
    }
}

void init_navigation_group() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlBIOS,
            ui_pnlCatalogue,
            ui_pnlName,
            ui_pnlRetroArch,
            ui_pnlConfig,
            ui_pnlCore,
            ui_pnlCollection,
            ui_pnlHistory,
            ui_pnlMusic,
            ui_pnlSave,
            ui_pnlScreenshot,
            ui_pnlTheme,
            ui_pnlCataloguePackage,
            ui_pnlConfigPackage,
            ui_pnlLanguage,
            ui_pnlNetwork,
            ui_pnlSyncthing,
            ui_pnlUserInit
    };

    ui_objects[0] = ui_lblBIOS;
    ui_objects[1] = ui_lblCatalogue;
    ui_objects[2] = ui_lblName;
    ui_objects[3] = ui_lblRetroArch;
    ui_objects[4] = ui_lblConfig;
    ui_objects[5] = ui_lblCore;
    ui_objects[6] = ui_lblCollection;
    ui_objects[7] = ui_lblHistory;
    ui_objects[8] = ui_lblMusic;
    ui_objects[9] = ui_lblSave;
    ui_objects[10] = ui_lblScreenshot;
    ui_objects[11] = ui_lblTheme;
    ui_objects[12] = ui_lblCataloguePackage;
    ui_objects[13] = ui_lblConfigPackage;
    ui_objects[14] = ui_lblLanguage;
    ui_objects[15] = ui_lblNetwork;
    ui_objects[16] = ui_lblSyncthing;
    ui_objects[17] = ui_lblUserInit;

    lv_obj_t *ui_objects_value[] = {
            ui_lblBIOSValue,
            ui_lblCatalogueValue,
            ui_lblNameValue,
            ui_lblRetroArchValue,
            ui_lblConfigValue,
            ui_lblCoreValue,
            ui_lblCollectionValue,
            ui_lblHistoryValue,
            ui_lblMusicValue,
            ui_lblSaveValue,
            ui_lblScreenshotValue,
            ui_lblThemeValue,
            ui_lblCataloguePackageValue,
            ui_lblConfigPackageValue,
            ui_lblLanguageValue,
            ui_lblNetworkValue,
            ui_lblSyncthingValue,
            ui_lblUserInitValue,
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoBIOS,
            ui_icoCatalogue,
            ui_icoName,
            ui_icoRetroArch,
            ui_icoConfig,
            ui_icoCore,
            ui_icoCollection,
            ui_icoHistory,
            ui_icoMusic,
            ui_icoSave,
            ui_icoScreenshot,
            ui_icoTheme,
            ui_icoCataloguePackage,
            ui_icoConfigPackage,
            ui_icoLanguage,
            ui_icoNetwork,
            ui_icoSyncthing,
            ui_icoUserInit
    };

    apply_theme_list_panel(ui_pnlBIOS);
    apply_theme_list_panel(ui_pnlCatalogue);
    apply_theme_list_panel(ui_pnlName);
    apply_theme_list_panel(ui_pnlRetroArch);
    apply_theme_list_panel(ui_pnlConfig);
    apply_theme_list_panel(ui_pnlCore);
    apply_theme_list_panel(ui_pnlCollection);
    apply_theme_list_panel(ui_pnlHistory);
    apply_theme_list_panel(ui_pnlMusic);
    apply_theme_list_panel(ui_pnlSave);
    apply_theme_list_panel(ui_pnlScreenshot);
    apply_theme_list_panel(ui_pnlTheme);
    apply_theme_list_panel(ui_pnlCataloguePackage);
    apply_theme_list_panel(ui_pnlConfigPackage);
    apply_theme_list_panel(ui_pnlLanguage);
    apply_theme_list_panel(ui_pnlNetwork);
    apply_theme_list_panel(ui_pnlSyncthing);
    apply_theme_list_panel(ui_pnlUserInit);

    apply_theme_list_item(&theme, ui_lblBIOS, lang.MUXSTORAGE.BIOS);
    apply_theme_list_item(&theme, ui_lblCatalogue, lang.MUXSTORAGE.CATALOGUE);
    apply_theme_list_item(&theme, ui_lblName, lang.MUXSTORAGE.FRIENDLY);
    apply_theme_list_item(&theme, ui_lblRetroArch, lang.MUXSTORAGE.RA_SYSTEM);
    apply_theme_list_item(&theme, ui_lblConfig, lang.MUXSTORAGE.RA_CONFIG);
    apply_theme_list_item(&theme, ui_lblCore, lang.MUXSTORAGE.ASSIGNED);
    apply_theme_list_item(&theme, ui_lblCollection, lang.MUXSTORAGE.COLLECTION);
    apply_theme_list_item(&theme, ui_lblHistory, lang.MUXSTORAGE.HISTORY);
    apply_theme_list_item(&theme, ui_lblMusic, lang.MUXSTORAGE.MUSIC);
    apply_theme_list_item(&theme, ui_lblSave, lang.MUXSTORAGE.SAVE);
    apply_theme_list_item(&theme, ui_lblScreenshot, lang.MUXSTORAGE.SCREENSHOT);
    apply_theme_list_item(&theme, ui_lblTheme, lang.MUXSTORAGE.PACKAGE.THEME);
    apply_theme_list_item(&theme, ui_lblCataloguePackage, lang.MUXSTORAGE.PACKAGE.CATALOGUE);
    apply_theme_list_item(&theme, ui_lblConfigPackage, lang.MUXSTORAGE.PACKAGE.RA_CONFIG);
    apply_theme_list_item(&theme, ui_lblLanguage, lang.MUXSTORAGE.LANGUAGE);
    apply_theme_list_item(&theme, ui_lblNetwork, lang.MUXSTORAGE.NET_PROFILE);
    apply_theme_list_item(&theme, ui_lblSyncthing, lang.MUXSTORAGE.SYNCTHING);
    apply_theme_list_item(&theme, ui_lblUserInit, lang.MUXSTORAGE.USER_INIT);

    apply_theme_list_glyph(&theme, ui_icoBIOS, mux_module, "bios");
    apply_theme_list_glyph(&theme, ui_icoCatalogue, mux_module, "catalogue");
    apply_theme_list_glyph(&theme, ui_icoName, mux_module, "name");
    apply_theme_list_glyph(&theme, ui_icoRetroArch, mux_module, "retroarch");
    apply_theme_list_glyph(&theme, ui_icoConfig, mux_module, "config");
    apply_theme_list_glyph(&theme, ui_icoCore, mux_module, "core");
    apply_theme_list_glyph(&theme, ui_icoCollection, mux_module, "collection");
    apply_theme_list_glyph(&theme, ui_icoHistory, mux_module, "history");
    apply_theme_list_glyph(&theme, ui_icoMusic, mux_module, "music");
    apply_theme_list_glyph(&theme, ui_icoSave, mux_module, "save");
    apply_theme_list_glyph(&theme, ui_icoScreenshot, mux_module, "screenshot");
    apply_theme_list_glyph(&theme, ui_icoTheme, mux_module, "theme");
    apply_theme_list_glyph(&theme, ui_icoCataloguePackage, mux_module, "pack-catalogue");
    apply_theme_list_glyph(&theme, ui_icoConfigPackage, mux_module, "pack-config");
    apply_theme_list_glyph(&theme, ui_icoLanguage, mux_module, "language");
    apply_theme_list_glyph(&theme, ui_icoNetwork, mux_module, "network");
    apply_theme_list_glyph(&theme, ui_icoSyncthing, mux_module, "syncthing");
    apply_theme_list_glyph(&theme, ui_icoUserInit, mux_module, "userinit");

    apply_theme_list_value(&theme, ui_lblBIOSValue, "");
    apply_theme_list_value(&theme, ui_lblCatalogueValue, "");
    apply_theme_list_value(&theme, ui_lblNameValue, "");
    apply_theme_list_value(&theme, ui_lblRetroArchValue, "");
    apply_theme_list_value(&theme, ui_lblConfigValue, "");
    apply_theme_list_value(&theme, ui_lblCoreValue, "");
    apply_theme_list_value(&theme, ui_lblCollectionValue, "");
    apply_theme_list_value(&theme, ui_lblHistoryValue, "");
    apply_theme_list_value(&theme, ui_lblMusicValue, "");
    apply_theme_list_value(&theme, ui_lblSaveValue, "");
    apply_theme_list_value(&theme, ui_lblScreenshotValue, "");
    apply_theme_list_value(&theme, ui_lblThemeValue, "");
    apply_theme_list_value(&theme, ui_lblCataloguePackageValue, "");
    apply_theme_list_value(&theme, ui_lblConfigPackageValue, "");
    apply_theme_list_value(&theme, ui_lblLanguageValue, "");
    apply_theme_list_value(&theme, ui_lblNetworkValue, "");
    apply_theme_list_value(&theme, ui_lblSyncthingValue, "");
    apply_theme_list_value(&theme, ui_lblUserInitValue, "");

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    ui_count = sizeof(ui_objects) / sizeof(ui_objects[0]);
    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_value, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_value, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void handle_back(void) {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound("back", nav_sound, 0, 1);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "storage");
    mux_input_stop();
}

void handle_confirm(void) {
    if (msgbox_active) return;

    play_sound("confirm", nav_sound, 0, 1);

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group_value);

    static char storage_script[MAX_BUFFER_SIZE];
    if (strcasecmp(lv_label_get_text(element_focused), "SD2") == 0) {
        snprintf(storage_script, sizeof(storage_script), "%s/script/mux/sync.sh", INTERNAL_PATH);
    } else {
        snprintf(storage_script, sizeof(storage_script), "%s/script/mux/migrate.sh", INTERNAL_PATH);
    }

    const char *args[] = {
            (INTERNAL_PATH "bin/fbpad"),
            "-bg", theme.TERMINAL.BACKGROUND,
            "-fg", theme.TERMINAL.FOREGROUND,
            storage_script, storage_path[current_item_index].path_suffix,
            NULL
    };

    setenv("TERM", "xterm-256color", 1);

    if (config.VISUAL.BLACKFADE) {
        fade_to_black(ui_screen);
    } else {
        unload_image_animation();
    }

    run_exec(args);

    write_text_to_file(MUOS_SIN_LOAD, "w", INT, current_item_index);

    load_mux("storage");
    mux_input_stop();
}

void handle_help(void) {
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

    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavCGlyph,
            ui_lblNavC,
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

    lv_obj_set_user_data(ui_lblBIOS, "bios");
    lv_obj_set_user_data(ui_lblCatalogue, "catalogue");
    lv_obj_set_user_data(ui_lblName, "name");
    lv_obj_set_user_data(ui_lblRetroArch, "retroarch");
    lv_obj_set_user_data(ui_lblConfig, "config");
    lv_obj_set_user_data(ui_lblCore, "core");
    lv_obj_set_user_data(ui_lblCollection, "collection");
    lv_obj_set_user_data(ui_lblHistory, "history");
    lv_obj_set_user_data(ui_lblMusic, "music");
    lv_obj_set_user_data(ui_lblSave, "save");
    lv_obj_set_user_data(ui_lblScreenshot, "screenshot");
    lv_obj_set_user_data(ui_lblTheme, "theme");
    lv_obj_set_user_data(ui_lblCataloguePackage, "pack-catalogue");
    lv_obj_set_user_data(ui_lblConfigPackage, "pack-config");
    lv_obj_set_user_data(ui_lblLanguage, "language");
    lv_obj_set_user_data(ui_lblNetwork, "network");
    lv_obj_set_user_data(ui_lblSyncthing, "syncthing");
    lv_obj_set_user_data(ui_lblUserInit, "userinit");

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

        struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group_value);
        if (strcasecmp(lv_label_get_text(element_focused), "SD2") == 0) {
            lv_label_set_text(ui_lblNavX, lang.GENERIC.SYNC);
        } else {
            lv_label_set_text(ui_lblNavX, lang.GENERIC.MIGRATE);
        }

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
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXSTORAGE.TITLE);
    init_mux(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    update_storage_info();
    init_navigation_sound(&nav_sound, mux_module);

    int sin_index = 0;
    if (file_exist(MUOS_SIN_LOAD)) {
        sin_index = read_int_from_file(MUOS_SIN_LOAD, 1);
        remove(MUOS_SIN_LOAD);
    }

    if (ui_count > 0) {
        if (sin_index > -1 && sin_index <= ui_count && current_item_index < ui_count) {
            list_nav_next(sin_index);
        }
    }

    init_input(&joy_general, &joy_power, &joy_volume, &joy_extra);
    init_timer(ui_refresh_task, NULL);

    load_kiosk(&kiosk);

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
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_X] = handle_confirm,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
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
