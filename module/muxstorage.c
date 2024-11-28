#include "../lvgl/lvgl.h"
#include "../lvgl/src/drivers/fbdev.h"
#include "../lvgl/src/drivers/evdev.h"
#include "ui/ui_muxstorage.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/common.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/input.h"
#include "../common/input/list_nav.h"

char *mux_module;
static int js_fd;
static int js_fd_sys;

int turbo_mode = 0;
int msgbox_active = 0;
int SD2_found = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;

struct mux_config config;
struct mux_device device;
struct theme_config theme;

int nav_moved = 1;
int current_item_index = 0;
int ui_count = 0;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;

int progress_onscreen = -1;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

#define UI_COUNT 16
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
            {ui_lblBIOS,       TS("Location of system BIOS files")},
            {ui_lblCatalogue,  TS("Location of content images and text")},
            {ui_lblName,       TS("Location of friendly name configurations")},
            {ui_lblRetroArch,  TS("Location of the RetroArch emulator")},
            {ui_lblConfig,     TS("Location of RetroArch configurations")},
            {ui_lblCore,       TS("Location of assigned core and governor configurations")},
            {ui_lblFavourite,  TS("Location of favourites")},
            {ui_lblHistory,    TS("Location of history")},
            {ui_lblMusic,      TS("Location of background music")},
            {ui_lblSave,       TS("Location of save states and files")},
            {ui_lblScreenshot, TS("Location of screenshots")},
            {ui_lblTheme,      TS("Location of themes")},
            {ui_lblLanguage,   TS("Location of Language files")},
            {ui_lblNetwork,    TS("Location of Network Profiles")},
            {ui_lblSyncthing,  TS("Location of Syncthing configurations")},
            {ui_lblUserInit,   TS("Location of User Initialisation scripts")},
    };

    char *message = TG("No Help Information Found");
    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);

    for (int i = 0; i < num_messages; i++) {
        if (element_focused == help_messages[i].element) {
            message = help_messages[i].message;
            break;
        }
    }

    if (strlen(message) <= 1) message = TG("No Help Information Found");

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
}

void update_storage_info() {
    /*
     * Check for SD2 pathing, otherwise it should be on SD1.
     * If it's not on SD1 then you have bigger problems!
    */
    storage_path[0].path_suffix = "MUOS/bios";
    storage_path[0].ui_label = ui_lblBIOSValue;

    storage_path[1].path_suffix = "MUOS/info/catalogue";
    storage_path[1].ui_label = ui_lblCatalogueValue;

    storage_path[2].path_suffix = "MUOS/info/name";
    storage_path[2].ui_label = ui_lblNameValue;

    storage_path[3].path_suffix = "MUOS/retroarch";
    storage_path[3].ui_label = ui_lblRetroArchValue;

    storage_path[4].path_suffix = "MUOS/info/config";
    storage_path[4].ui_label = ui_lblConfigValue;

    storage_path[5].path_suffix = "MUOS/info/core";
    storage_path[5].ui_label = ui_lblCoreValue;

    storage_path[6].path_suffix = "MUOS/info/favourite";
    storage_path[6].ui_label = ui_lblFavouriteValue;

    storage_path[7].path_suffix = "MUOS/info/history";
    storage_path[7].ui_label = ui_lblHistoryValue;

    storage_path[8].path_suffix = "MUOS/music";
    storage_path[8].ui_label = ui_lblMusicValue;

    storage_path[9].path_suffix = "MUOS/save";
    storage_path[9].ui_label = ui_lblSaveValue;

    storage_path[10].path_suffix = "MUOS/screenshot";
    storage_path[10].ui_label = ui_lblScreenshotValue;

    storage_path[11].path_suffix = "MUOS/theme";
    storage_path[11].ui_label = ui_lblThemeValue;

    storage_path[12].path_suffix = "MUOS/language";
    storage_path[12].ui_label = ui_lblLanguageValue;

    storage_path[13].path_suffix = "MUOS/network";
    storage_path[13].ui_label = ui_lblNetworkValue;

    storage_path[14].path_suffix = "MUOS/syncthing";
    storage_path[14].ui_label = ui_lblSyncthingValue;

    storage_path[15].path_suffix = "MUOS/init";
    storage_path[15].ui_label = ui_lblUserInitValue;

    char dir[FILENAME_MAX];
    for (int i = 0; i < sizeof(storage_path) / sizeof(storage_path[0]); i++) {
        snprintf(dir, sizeof(dir), "%s/%s", device.STORAGE.SDCARD.MOUNT, storage_path[i].path_suffix);
        if (directory_exist(dir)) {
            lv_label_set_text(storage_path[i].ui_label, "SD2");
            lv_label_set_text(ui_lblNavX, TG("Sync to SD1"));
        } else {
            lv_label_set_text(storage_path[i].ui_label, "SD1");
            lv_label_set_text(ui_lblNavX, TG("Migrate to SD2"));
        }
    }
}

void init_navigation_groups() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlBIOS,
            ui_pnlCatalogue,
            ui_pnlName,
            ui_pnlRetroArch,
            ui_pnlConfig,
            ui_pnlCore,
            ui_pnlFavourite,
            ui_pnlHistory,
            ui_pnlMusic,
            ui_pnlSave,
            ui_pnlScreenshot,
            ui_pnlTheme,
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
    ui_objects[6] = ui_lblFavourite;
    ui_objects[7] = ui_lblHistory;
    ui_objects[8] = ui_lblMusic;
    ui_objects[9] = ui_lblSave;
    ui_objects[10] = ui_lblScreenshot;
    ui_objects[11] = ui_lblTheme;
    ui_objects[12] = ui_lblLanguage;
    ui_objects[13] = ui_lblNetwork;
    ui_objects[14] = ui_lblSyncthing;
    ui_objects[15] = ui_lblUserInit;

    lv_obj_t *ui_objects_value[] = {
            ui_lblBIOSValue,
            ui_lblCatalogueValue,
            ui_lblNameValue,
            ui_lblRetroArchValue,
            ui_lblConfigValue,
            ui_lblCoreValue,
            ui_lblFavouriteValue,
            ui_lblHistoryValue,
            ui_lblMusicValue,
            ui_lblSaveValue,
            ui_lblScreenshotValue,
            ui_lblThemeValue,
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
            ui_icoFavourite,
            ui_icoHistory,
            ui_icoMusic,
            ui_icoSave,
            ui_icoScreenshot,
            ui_icoTheme,
            ui_icoLanguage,
            ui_icoNetwork,
            ui_icoSyncthing,
            ui_icoUserInit
    };

    apply_theme_list_panel(&theme, &device, ui_pnlBIOS);
    apply_theme_list_panel(&theme, &device, ui_pnlCatalogue);
    apply_theme_list_panel(&theme, &device, ui_pnlName);
    apply_theme_list_panel(&theme, &device, ui_pnlRetroArch);
    apply_theme_list_panel(&theme, &device, ui_pnlConfig);
    apply_theme_list_panel(&theme, &device, ui_pnlCore);
    apply_theme_list_panel(&theme, &device, ui_pnlFavourite);
    apply_theme_list_panel(&theme, &device, ui_pnlHistory);
    apply_theme_list_panel(&theme, &device, ui_pnlMusic);
    apply_theme_list_panel(&theme, &device, ui_pnlSave);
    apply_theme_list_panel(&theme, &device, ui_pnlScreenshot);
    apply_theme_list_panel(&theme, &device, ui_pnlTheme);
    apply_theme_list_panel(&theme, &device, ui_pnlLanguage);
    apply_theme_list_panel(&theme, &device, ui_pnlNetwork);
    apply_theme_list_panel(&theme, &device, ui_pnlSyncthing);
    apply_theme_list_panel(&theme, &device, ui_pnlUserInit);

    apply_theme_list_item(&theme, ui_lblBIOS, TS("System BIOS"), false, true);
    apply_theme_list_item(&theme, ui_lblCatalogue, TS("Metadata Catalogue"), false, true);
    apply_theme_list_item(&theme, ui_lblName, TS("Friendly Name System"), false, true);
    apply_theme_list_item(&theme, ui_lblRetroArch, TS("RetroArch System"), false, true);
    apply_theme_list_item(&theme, ui_lblConfig, TS("RetroArch Configs"), false, true);
    apply_theme_list_item(&theme, ui_lblCore, TS("Assigned Core/Governor System"), false, true);
    apply_theme_list_item(&theme, ui_lblFavourite, TS("Favourites"), false, true);
    apply_theme_list_item(&theme, ui_lblHistory, TS("History"), false, true);
    apply_theme_list_item(&theme, ui_lblMusic, TS("Background Music"), false, true);
    apply_theme_list_item(&theme, ui_lblSave, TS("Save Games + Save States"), false, true);
    apply_theme_list_item(&theme, ui_lblScreenshot, TS("Screenshots"), false, true);
    apply_theme_list_item(&theme, ui_lblTheme, TS("Themes"), false, true);
    apply_theme_list_item(&theme, ui_lblLanguage, TS("Languages"), false, true);
    apply_theme_list_item(&theme, ui_lblNetwork, TS("Network Profiles"), false, true);
    apply_theme_list_item(&theme, ui_lblSyncthing, TS("Syncthing Configs"), false, true);
    apply_theme_list_item(&theme, ui_lblUserInit, TS("User Init Scripts"), false, true);

    apply_theme_list_glyph(&theme, ui_icoBIOS, mux_module, "bios");
    apply_theme_list_glyph(&theme, ui_icoCatalogue, mux_module, "catalogue");
    apply_theme_list_glyph(&theme, ui_icoName, mux_module, "name");
    apply_theme_list_glyph(&theme, ui_icoRetroArch, mux_module, "retroarch");
    apply_theme_list_glyph(&theme, ui_icoConfig, mux_module, "config");
    apply_theme_list_glyph(&theme, ui_icoCore, mux_module, "core");
    apply_theme_list_glyph(&theme, ui_icoFavourite, mux_module, "favourite");
    apply_theme_list_glyph(&theme, ui_icoHistory, mux_module, "history");
    apply_theme_list_glyph(&theme, ui_icoMusic, mux_module, "music");
    apply_theme_list_glyph(&theme, ui_icoSave, mux_module, "save");
    apply_theme_list_glyph(&theme, ui_icoScreenshot, mux_module, "screenshot");
    apply_theme_list_glyph(&theme, ui_icoTheme, mux_module, "theme");
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
    apply_theme_list_value(&theme, ui_lblFavouriteValue, "");
    apply_theme_list_value(&theme, ui_lblHistoryValue, "");
    apply_theme_list_value(&theme, ui_lblMusicValue, "");
    apply_theme_list_value(&theme, ui_lblSaveValue, "");
    apply_theme_list_value(&theme, ui_lblScreenshotValue, "");
    apply_theme_list_value(&theme, ui_lblThemeValue, "");
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
    static char command[MAX_BUFFER_SIZE];

    if (strcasecmp(lv_label_get_text(element_focused), "SD2") == 0) {
        snprintf(command, sizeof(command), "/opt/muos/bin/fbpad \"%s/script/mux/sync.sh\" \"%s\"",
                 INTERNAL_PATH, storage_path[current_item_index].path_suffix);
    } else {
        snprintf(command, sizeof(command), "/opt/muos/bin/fbpad \"%s/script/mux/migrate.sh\" \"%s\"",
                 INTERNAL_PATH, storage_path[current_item_index].path_suffix);
    }

    setenv("TERM", "xterm-256color", 1);
    printf("RUNNING: %s\n", command);

    if (config.VISUAL.BLACKFADE) fade_to_black(ui_screen);
    system(command);

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

    if (bar_footer) {
        lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (bar_header) {
        lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, osd_message);

    lv_label_set_text(ui_lblNavB, TG("Back"));

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
    lv_obj_set_user_data(ui_lblFavourite, "favourite");
    lv_obj_set_user_data(ui_lblHistory, "history");
    lv_obj_set_user_data(ui_lblMusic, "music");
    lv_obj_set_user_data(ui_lblSave, "save");
    lv_obj_set_user_data(ui_lblScreenshot, "screenshot");
    lv_obj_set_user_data(ui_lblTheme, "theme");
    lv_obj_set_user_data(ui_lblLanguage, "language");
    lv_obj_set_user_data(ui_lblNetwork, "network");
    lv_obj_set_user_data(ui_lblSyncthing, "syncthing");
    lv_obj_set_user_data(ui_lblUserInit, "userinit");

    if (TEST_IMAGE) display_testing_message(ui_screen);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image, theme.MISC.IMAGE_OVERLAY);
}

void glyph_task() {
    // TODO: Bluetooth connectivity!
    //update_bluetooth_status(ui_staBluetooth, &theme);

    update_network_status(ui_staNetwork, &theme);
    update_battery_capacity(ui_staCapacity, &theme);

    if (progress_onscreen > 0) {
        progress_onscreen -= 1;
    } else {
        if (!lv_obj_has_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN);
        }
        if (!lv_obj_has_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN);
        }
        if (!msgbox_active) {
            progress_onscreen = -1;
        }
    }
}

void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group_value);
        if (strcasecmp(lv_label_get_text(element_focused), "SD2") == 0) {
            lv_label_set_text(ui_lblNavX, TG("Sync to SD1"));
        } else {
            lv_label_set_text(ui_lblNavX, TG("Migrate to SD2"));
        }

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    (void) argc;

    mux_module = basename(argv[0]);
    load_device(&device);

    lv_init();
    fbdev_init(device.SCREEN.DEVICE);

    static lv_disp_draw_buf_t disp_buf;
    uint32_t disp_buf_size = device.SCREEN.WIDTH * device.SCREEN.HEIGHT;

    lv_color_t * buf1 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));
    lv_color_t * buf2 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, disp_buf_size);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res = device.SCREEN.WIDTH;
    disp_drv.ver_res = device.SCREEN.HEIGHT;
    disp_drv.sw_rotate = device.SCREEN.ROTATE;
    disp_drv.rotated = device.SCREEN.ROTATE;
    disp_drv.full_refresh = 0;
    disp_drv.direct_mode = 0;
    lv_disp_drv_register(&disp_drv);

    load_config(&config);
    load_theme(&theme, &config, &device, basename(argv[0]));
    load_language(mux_module);

    ui_common_screen_init(&theme, &device, TS("STORAGE"));
    ui_init(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);

    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND,
                   theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND, GENERAL);

    load_font_text(basename(argv[0]), ui_screen);
    load_font_section(basename(argv[0]), FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(mux_module, FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(mux_module, FONT_FOOTER_FOLDER, ui_pnlFooter);

    int sin_index = 0;
    if (file_exist(MUOS_SIN_LOAD)) {
        sin_index = read_int_from_file(MUOS_SIN_LOAD, 1);
        remove(MUOS_SIN_LOAD);
    }

    nav_sound = init_nav_sound(mux_module);
    init_navigation_groups();
    update_storage_info();

    if (ui_count > 0) {
        if (sin_index > -1 && sin_index <= ui_count && current_item_index < ui_count) {
            list_nav_next(sin_index);
        }
    }

    struct dt_task_param dt_par;
    struct bat_task_param bat_par;
    struct osd_task_param osd_par;

    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;
    osd_par.lblMessage = ui_lblMessage;
    osd_par.pnlMessage = ui_pnlMessage;
    osd_par.count = 0;

    js_fd = open(device.INPUT.EV1, O_RDONLY);
    if (js_fd < 0) {
        perror("Failed to open joystick device");
        return 1;
    }

    js_fd_sys = open(device.INPUT.EV0, O_RDONLY);
    if (js_fd_sys < 0) {
        perror("Failed to open joystick device");
        return 1;
    }

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);

    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = evdev_read;
    indev_drv.user_data = (void *) (intptr_t) js_fd;

    lv_indev_drv_register(&indev_drv);

    lv_timer_t *datetime_timer = lv_timer_create(datetime_task, UINT16_MAX / 2, &dt_par);
    lv_timer_ready(datetime_timer);

    lv_timer_t *capacity_timer = lv_timer_create(capacity_task, UINT16_MAX / 2, &bat_par);
    lv_timer_ready(capacity_timer);

    lv_timer_t *osd_timer = lv_timer_create(osd_task, UINT16_MAX / 32, &osd_par);
    lv_timer_ready(osd_timer);

    lv_timer_t *glyph_timer = lv_timer_create(glyph_task, UINT16_MAX / 64, NULL);
    lv_timer_ready(glyph_timer);

    lv_timer_t *ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
    lv_timer_ready(ui_refresh_timer);

    refresh_screen(device.SCREEN.WAIT);

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = 16 /* ~60 FPS */,
            .swap_btn = config.SETTINGS.ADVANCED.SWAP,
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .stick_nav = true,
            .press_handler = {
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_X] = handle_confirm,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    // List navigation:
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    // List navigation:
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .combo = {
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_bright,
                            .hold_handler = ui_common_handle_bright,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_bright,
                            .hold_handler = ui_common_handle_bright,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_vol,
                            .hold_handler = ui_common_handle_vol,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_vol,
                            .hold_handler = ui_common_handle_vol,
                    },
            },
            .idle_handler = ui_common_handle_idle,
    };
    mux_input_task(&input_opts);

    close(js_fd);
    close(js_fd_sys);

    return 0;
}
