#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/img/nothing.h"
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
int input_disable = 0;
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

int progress_onscreen = -1;

#define UI_COUNT 15
lv_obj_t *ui_objects[UI_COUNT];

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

struct help_msg {
    lv_obj_t *element;
    char *message;
};

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
    struct {
        const char *path_suffix;
        lv_obj_t *ui_label;
    } storage[] = {
            {"MUOS/bios", ui_lblBIOSValue},
            {"MUOS/info/catalogue", ui_lblCatalogueValue},
            {"MUOS/info/name", ui_lblNameValue},
            {"MUOS/retroarch", ui_lblRetroArchValue},
            {"MUOS/info/config", ui_lblConfigValue},
            {"MUOS/info/core", ui_lblCoreValue},
            {"MUOS/info/favourite", ui_lblFavouriteValue},
            {"MUOS/info/history", ui_lblHistoryValue},
            {"MUOS/music", ui_lblMusicValue},
            {"MUOS/save", ui_lblSaveValue},
            {"MUOS/screenshot", ui_lblScreenshotValue},
            {"MUOS/theme", ui_lblThemeValue},
            {"MUOS/language", ui_lblLanguageValue},
            {"MUOS/network", ui_lblNetworkValue},
            {"MUOS/syncthing", ui_lblSyncthingValue}
    };

    char dir[FILENAME_MAX];
    for (int i = 0; i < sizeof(storage) / sizeof(storage[0]); i++) {
        snprintf(dir, sizeof(dir), "%s/%s", device.STORAGE.SDCARD.MOUNT, storage[i].path_suffix);
        lv_label_set_text(storage[i].ui_label, directory_exist(dir) ? "SD2" : "SD1");
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
            ui_pnlSyncthing
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
            ui_lblSyncthingValue
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
            ui_icoSyncthing
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
    play_sound("navigate", nav_sound, 0);
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
    play_sound("navigate", nav_sound, 0);
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
        play_sound("confirm", nav_sound, 1);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound("back", nav_sound, 1);
    input_disable = 1;

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "storage");
    mux_input_stop();
}

void handle_migrate(void) {
    if (msgbox_active) {
        return;
    }

    play_sound("confirm", nav_sound, 1);
    input_disable = 1;

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group_value);
    if (strcasecmp(lv_label_get_text(element_focused), "SD2") == 0) {
        toast_message(TS("This storage element is already on SD2"), 1000, 1000);
    } else {
        toast_message(TS("Doing the migrate stuff yay! - not really..."), 1000, 1000);
    }
}

void handle_help(void) {
    if (msgbox_active) {
        return;
    }

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 1);
        show_help(lv_group_get_focused(ui_group));
    }
}

void init_elements() {
    lv_obj_move_foreground(ui_pnlFooter);
    lv_obj_move_foreground(ui_pnlHeader);
    lv_obj_move_foreground(ui_pnlHelp);
    lv_obj_move_foreground(ui_pnlProgressBrightness);
    lv_obj_move_foreground(ui_pnlProgressVolume);

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
    lv_label_set_text(ui_lblNavX, TG("Migrate to SD2"));

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

    if (TEST_IMAGE) display_testing_message(ui_screen);
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
    update_bars(ui_barProgressBrightness, ui_barProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            load_wallpaper(ui_screen, ui_group, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND, 
                    theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND);

            static char static_image[MAX_BUFFER_SIZE];
            snprintf(static_image, sizeof(static_image), "%s",
                     load_static_image(ui_screen, ui_group));

            if (strlen(static_image) > 0) {
                printf("LOADING STATIC IMAGE: %s\n", static_image);

                switch (theme.MISC.STATIC_ALIGNMENT) {
                    case 0: // Bottom + Front
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                    case 1: // Middle + Front
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_RIGHT_MID);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                    case 2: // Top + Front
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_TOP_RIGHT);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                    case 3: // Fullscreen + Behind
                        lv_obj_set_height(ui_pnlBox, device.MUX.HEIGHT);
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_background(ui_pnlBox);
                        lv_obj_move_background(ui_pnlWall);
                        break;
                    case 4: // Fullscreen + Front
                        lv_obj_set_height(ui_pnlBox, device.MUX.HEIGHT);
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                }

                lv_img_set_src(ui_imgBox, static_image);
            } else {
                lv_img_set_src(ui_imgBox, &ui_image_Nothing);
            }
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

    ui_common_screen_init(&theme, &device, TS("STORAGE PREFERENCE"));
    ui_init(ui_pnlContent);
    init_elements();
    load_overlay_image(ui_screen, theme.MISC.IMAGE_OVERLAY);

    lv_obj_set_user_data(ui_screen, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND, 
            theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND);

    load_font_text(basename(argv[0]), ui_screen);
    load_font_section(basename(argv[0]), FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(mux_module, FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(mux_module, FONT_FOOTER_FOLDER, ui_pnlFooter);

    nav_sound = init_nav_sound(mux_module);
    init_navigation_groups();
    update_storage_info();

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

    refresh_screen();

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = 16 /* ~60 FPS */,
            .swap_btn = config.SETTINGS.ADVANCED.SWAP,
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .stick_nav = true,
            .press_handler = {
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_X] = handle_migrate,
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
