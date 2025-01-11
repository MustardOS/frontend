#include "../lvgl/lvgl.h"
#include "../lvgl/src/drivers/fbdev.h"
#include "../lvgl/src/drivers/evdev.h"
#include "ui/ui_muxtweakgen.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
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
static int js_fd;
static int js_fd_sys;

int turbo_mode = 0;
int msgbox_active = 0;
int SD2_found = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;

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

int hidden_original, bgm_original, sound_original, startup_original, colour_original, brightness_original;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

#define UI_COUNT 10
lv_obj_t *ui_objects[UI_COUNT];

lv_obj_t *ui_mux_panels[5];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblHidden,     lang.MUXTWEAKGEN.HELP.HIDDEN},
            {ui_lblBGM,        lang.MUXTWEAKGEN.HELP.MUSIC},
            {ui_lblSound,      lang.MUXTWEAKGEN.HELP.SOUND},
            {ui_lblStartup,    lang.MUXTWEAKGEN.HELP.STARTUP},
            {ui_lblColour,     lang.MUXTWEAKGEN.HELP.TEMP},
            {ui_lblBrightness, lang.MUXTWEAKGEN.HELP.BRIGHT},
            {ui_lblHDMI,       lang.MUXTWEAKGEN.HELP.HDMI},
            {ui_lblPower,      lang.MUXTWEAKGEN.HELP.POWER},
            {ui_lblInterface,  lang.MUXTWEAKGEN.HELP.VISUAL},
            {ui_lblAdvanced,   lang.MUXTWEAKGEN.HELP.ADVANCED},
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

static void dropdown_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        char buf[MAX_BUFFER_SIZE];
        lv_dropdown_get_selected_str(obj, buf, sizeof(buf));
    }
}

void elements_events_init() {
    lv_obj_t *dropdowns[] = {
            ui_droHidden,
            ui_droBGM,
            ui_droSound,
            ui_droStartup,
            ui_droColour,
            ui_droBrightness
    };

    for (unsigned int i = 0; i < sizeof(dropdowns) / sizeof(dropdowns[0]); i++) {
        lv_obj_add_event_cb(dropdowns[i], dropdown_event_handler, LV_EVENT_ALL, NULL);
    }
}

void init_dropdown_settings() {
    hidden_original = lv_dropdown_get_selected(ui_droHidden);
    bgm_original = lv_dropdown_get_selected(ui_droBGM);
    sound_original = lv_dropdown_get_selected(ui_droSound);
    startup_original = lv_dropdown_get_selected(ui_droStartup);
    colour_original = lv_dropdown_get_selected(ui_droColour);
    brightness_original = lv_dropdown_get_selected(ui_droBrightness);
}

void restore_tweak_options() {
    lv_dropdown_set_selected(ui_droHidden, config.SETTINGS.GENERAL.HIDDEN);
    lv_dropdown_set_selected(ui_droBGM, config.SETTINGS.GENERAL.BGM);
    lv_dropdown_set_selected(ui_droSound, config.SETTINGS.GENERAL.SOUND);
    lv_dropdown_set_selected(ui_droBrightness, config.SETTINGS.GENERAL.BRIGHTNESS + 1);
    lv_dropdown_set_selected(ui_droColour, config.SETTINGS.GENERAL.COLOUR + 255);

    const char *startup_type = config.SETTINGS.GENERAL.STARTUP;
    if (strcasecmp(startup_type, "explore") == 0) {
        lv_dropdown_set_selected(ui_droStartup, 1);
    } else if (strcasecmp(startup_type, "collection") == 0) {
        lv_dropdown_set_selected(ui_droStartup, 2);
    } else if (strcasecmp(startup_type, "history") == 0) {
        lv_dropdown_set_selected(ui_droStartup, 3);
    } else if (strcasecmp(startup_type, "last") == 0) {
        lv_dropdown_set_selected(ui_droStartup, 4);
    } else if (strcasecmp(startup_type, "resume") == 0) {
        lv_dropdown_set_selected(ui_droStartup, 5);
    } else {
        lv_dropdown_set_selected(ui_droStartup, 0);
    }
}

void save_tweak_options() {
    char *idx_startup;
    switch (lv_dropdown_get_selected(ui_droStartup)) {
        case 0:
            idx_startup = "launcher";
            break;
        case 1:
            idx_startup = "explore";
            break;
        case 2:
            idx_startup = "collection";
            break;
        case 3:
            idx_startup = "history";
            break;
        case 4:
            idx_startup = "last";
            break;
        case 5:
            idx_startup = "resume";
            break;
        default:
            idx_startup = "launcher";
            break;
    }

    int idx_hidden = lv_dropdown_get_selected(ui_droHidden);
    int idx_bgm = lv_dropdown_get_selected(ui_droBGM);
    int idx_sound = lv_dropdown_get_selected(ui_droSound);
    int idx_brightness = lv_dropdown_get_selected(ui_droBrightness);
    int idx_colour = lv_dropdown_get_selected(ui_droColour) - 255;

    int is_modified = 0;

    if (lv_dropdown_get_selected(ui_droHidden) != hidden_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/general/hidden"), "w", INT, idx_hidden);
    }

    if (lv_dropdown_get_selected(ui_droBGM) != bgm_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/general/bgm"), "w", INT, idx_bgm);
    }

    if (lv_dropdown_get_selected(ui_droSound) != sound_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/general/sound"), "w", INT, idx_sound);
    }

    if (lv_dropdown_get_selected(ui_droStartup) != startup_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/general/startup"), "w", CHAR, idx_startup);
    }

    if (lv_dropdown_get_selected(ui_droColour) != colour_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/general/colour"), "w", INT, idx_colour);
    }

    if (lv_dropdown_get_selected(ui_droBrightness) != brightness_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/general/brightness"), "w", INT, idx_brightness);

        char bright_value[8];
        snprintf(bright_value, sizeof(bright_value), "%d", idx_brightness + 1);
        run_exec((const char *[]) {(char *) INTERNAL_PATH "device/current/input/combo/bright.sh", bright_value, NULL});
    }

    if (is_modified > 0) run_exec((const char *[]) {(char *) INTERNAL_PATH "script/mux/tweak.sh", NULL});
}

void init_navigation_groups() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlHidden,
            ui_pnlBGM,
            ui_pnlSound,
            ui_pnlStartup,
            ui_pnlColour,
            ui_pnlBrightness,
            ui_pnlHDMI,
            ui_pnlPower,
            ui_pnlInterface,
            ui_pnlAdvanced,
    };

    ui_objects[0] = ui_lblHidden;
    ui_objects[1] = ui_lblBGM;
    ui_objects[2] = ui_lblSound;
    ui_objects[3] = ui_lblStartup;
    ui_objects[4] = ui_lblColour;
    ui_objects[5] = ui_lblBrightness;
    ui_objects[6] = ui_lblHDMI;
    ui_objects[7] = ui_lblPower;
    ui_objects[8] = ui_lblInterface;
    ui_objects[9] = ui_lblAdvanced;

    lv_obj_t *ui_objects_value[] = {
            ui_droHidden,
            ui_droBGM,
            ui_droSound,
            ui_droStartup,
            ui_droColour,
            ui_droBrightness,
            ui_droHDMI,
            ui_droPower,
            ui_droInterface,
            ui_droAdvanced
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoHidden,
            ui_icoBGM,
            ui_icoSound,
            ui_icoStartup,
            ui_icoColour,
            ui_icoBrightness,
            ui_icoHDMI,
            ui_icoPower,
            ui_icoInterface,
            ui_icoAdvanced
    };

    apply_theme_list_panel(ui_pnlHidden);
    apply_theme_list_panel(ui_pnlBGM);
    apply_theme_list_panel(ui_pnlSound);
    apply_theme_list_panel(ui_pnlStartup);
    apply_theme_list_panel(ui_pnlColour);
    apply_theme_list_panel(ui_pnlBrightness);
    apply_theme_list_panel(ui_pnlHDMI);
    apply_theme_list_panel(ui_pnlPower);
    apply_theme_list_panel(ui_pnlInterface);
    apply_theme_list_panel(ui_pnlAdvanced);

    apply_theme_list_item(&theme, ui_lblHidden, lang.MUXTWEAKGEN.HIDDEN);
    apply_theme_list_item(&theme, ui_lblBGM, lang.MUXTWEAKGEN.MUSIC.TITLE);
    apply_theme_list_item(&theme, ui_lblSound, lang.MUXTWEAKGEN.SOUND);
    apply_theme_list_item(&theme, ui_lblStartup, lang.MUXTWEAKGEN.STARTUP.TITLE);
    apply_theme_list_item(&theme, ui_lblColour, lang.MUXTWEAKGEN.TEMP);
    apply_theme_list_item(&theme, ui_lblBrightness, lang.MUXTWEAKGEN.BRIGHT);
    apply_theme_list_item(&theme, ui_lblHDMI, lang.MUXTWEAKGEN.HDMI);
    apply_theme_list_item(&theme, ui_lblPower, lang.MUXTWEAKGEN.POWER);
    apply_theme_list_item(&theme, ui_lblInterface, lang.MUXTWEAKGEN.VISUAL);
    apply_theme_list_item(&theme, ui_lblAdvanced, lang.MUXTWEAKGEN.ADVANCED);

    apply_theme_list_glyph(&theme, ui_icoHidden, mux_module, "hidden");
    apply_theme_list_glyph(&theme, ui_icoBGM, mux_module, "bgm");
    apply_theme_list_glyph(&theme, ui_icoSound, mux_module, "sound");
    apply_theme_list_glyph(&theme, ui_icoStartup, mux_module, "startup");
    apply_theme_list_glyph(&theme, ui_icoColour, mux_module, "colour");
    apply_theme_list_glyph(&theme, ui_icoBrightness, mux_module, "brightness");
    apply_theme_list_glyph(&theme, ui_icoHDMI, mux_module, "hdmi");
    apply_theme_list_glyph(&theme, ui_icoPower, mux_module, "power");
    apply_theme_list_glyph(&theme, ui_icoInterface, mux_module, "interface");
    apply_theme_list_glyph(&theme, ui_icoAdvanced, mux_module, "advanced");

    apply_theme_list_drop_down(&theme, ui_droHidden, NULL);
    apply_theme_list_drop_down(&theme, ui_droBGM, NULL);
    apply_theme_list_drop_down(&theme, ui_droSound, NULL);
    apply_theme_list_drop_down(&theme, ui_droStartup, NULL);
    apply_theme_list_drop_down(&theme, ui_droColour, NULL);

    char *brightness_string = generate_number_string(1, device.SCREEN.BRIGHT, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droBrightness, brightness_string);
    free(brightness_string);

    char *colour_string = generate_number_string(-255, 255, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droColour, colour_string);
    free(colour_string);

    apply_theme_list_drop_down(&theme, ui_droHDMI, "");
    apply_theme_list_drop_down(&theme, ui_droPower, "");
    apply_theme_list_drop_down(&theme, ui_droInterface, "");
    apply_theme_list_drop_down(&theme, ui_droAdvanced, "");

    char *disabled_enabled[] = {lang.GENERIC.DISABLED, lang.GENERIC.ENABLED};
    add_drop_down_options(ui_droHidden, disabled_enabled, 2);
    add_drop_down_options(ui_droBGM, (char *[]) {
            lang.GENERIC.DISABLED, lang.MUXTWEAKGEN.MUSIC.GLOBAL, lang.MUXTWEAKGEN.MUSIC.THEME}, 3);
    add_drop_down_options(ui_droSound, disabled_enabled, 2);
    add_drop_down_options(ui_droStartup, (char *[]) {
            lang.MUXTWEAKGEN.STARTUP.MENU, lang.MUXTWEAKGEN.STARTUP.EXPLORE, lang.MUXTWEAKGEN.STARTUP.COLLECTION,
            lang.MUXTWEAKGEN.STARTUP.HISTORY, lang.MUXTWEAKGEN.STARTUP.LAST, lang.MUXTWEAKGEN.STARTUP.RESUME}, 6);

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

void handle_confirm(void) {
    if (msgbox_active) return;

    struct {
        const char *glyph_name;
        const char *mux_name;
        int16_t *kiosk_flag;
    } elements[] = {
            {"hdmi",      "hdmi",     &kiosk.SETTING.HDMI},
            {"power",     "power",    &kiosk.SETTING.POWER},
            {"interface", "visual",   &kiosk.SETTING.VISUAL},
            {"advanced",  "tweakadv", &kiosk.SETTING.ADVANCED}
    };

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    const char *u_data = lv_obj_get_user_data(element_focused);

    for (size_t i = 0; i < sizeof(elements) / sizeof(elements[0]); i++) {
        if (strcasecmp(u_data, elements[i].glyph_name) == 0) {
            if (elements[i].kiosk_flag && *elements[i].kiosk_flag) {
                toast_message(kiosk_nope(), 1000, 1000);
                return;
            }

            play_sound("confirm", nav_sound, 0, 1);
            save_tweak_options();
            load_mux(elements[i].mux_name);
            mux_input_stop();
            break;
        }
    }

    handle_option_next();
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

    save_tweak_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "general");
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

    lv_label_set_text(ui_lblMessage, osd_message);

    lv_label_set_text(ui_lblNavB, lang.GENERIC.SAVE);

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
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

    lv_obj_set_user_data(ui_lblHidden, "hidden");
    lv_obj_set_user_data(ui_lblBGM, "bgm");
    lv_obj_set_user_data(ui_lblSound, "sound");
    lv_obj_set_user_data(ui_lblStartup, "startup");
    lv_obj_set_user_data(ui_lblColour, "colour");
    lv_obj_set_user_data(ui_lblBrightness, "brightness");
    lv_obj_set_user_data(ui_lblHDMI, "hdmi");
    lv_obj_set_user_data(ui_lblPower, "power");
    lv_obj_set_user_data(ui_lblInterface, "interface");
    lv_obj_set_user_data(ui_lblAdvanced, "advanced");

    if (!device.DEVICE.HAS_HDMI) {
        lv_obj_add_flag(ui_pnlHDMI, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlHDMI, LV_OBJ_FLAG_FLOATING);
        ui_count--;
    }

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

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
    if (file_exist("/tmp/hdmi_out")) {
        remove("/tmp/hdmi_out");
        handle_list_nav_down();
        handle_list_nav_up();
    }

    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

void direct_to_previous() {
    if (file_exist(MUOS_PDI_LOAD)) {
        char *prev = read_text_from_file(MUOS_PDI_LOAD);
        int text_hit = 0;

        for (unsigned int i = 0; i < sizeof(ui_objects) / sizeof(ui_objects[0]); i++) {
            const char *u_data = lv_obj_get_user_data(ui_objects[i]);

            if (strcasecmp(u_data, prev) == 0) {
                text_hit = i;
                break;
            }
        }

        if (text_hit != 0) {
            list_nav_next(text_hit - !device.DEVICE.HAS_HDMI);
            nav_moved = 1;
        }
    }
}

void theme_init() {
    load_theme(&theme, &config, &device, mux_module);

    init_panel_style(&theme);
    init_item_style(&theme);
    init_glyph_style(&theme);
}

int main(int argc, char *argv[]) {
    (void) argc;

    mux_module = basename(argv[0]);
    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    struct screen_dimension dims = get_device_dimensions();

    lv_init();
    fbdev_init(device.SCREEN.DEVICE);

    static lv_disp_draw_buf_t disp_buf;
    uint32_t disp_buf_size = dims.WIDTH * dims.HEIGHT;

    lv_color_t *buf1 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));
    lv_color_t *buf2 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, disp_buf_size);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res = dims.WIDTH;
    disp_drv.ver_res = dims.HEIGHT;
    disp_drv.sw_rotate = device.SCREEN.ROTATE;
    disp_drv.rotated = device.SCREEN.ROTATE;
    disp_drv.full_refresh = 0;
    disp_drv.direct_mode = 0;
    disp_drv.antialiasing = 1;
    disp_drv.color_chroma_key = lv_color_hex(0xFF00FF);
    lv_disp_drv_register(&disp_drv);
    lv_disp_flush_ready(&disp_drv);

    theme_init();

    ui_common_screen_init(&theme, &device, &lang, lang.MUXTWEAKGEN.TITLE);
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

    nav_sound = init_nav_sound(mux_module);
    init_navigation_groups();
    elements_events_init();

    restore_tweak_options();
    init_dropdown_settings();

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
        perror(lang.SYSTEM.NO_JOY);
        return 1;
    }

    js_fd_sys = open(device.INPUT.EV0, O_RDONLY);
    if (js_fd_sys < 0) {
        perror(lang.SYSTEM.NO_JOY);
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

    direct_to_previous();

    refresh_screen(device.SCREEN.WAIT);
    load_kiosk(&kiosk);

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
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
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
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
