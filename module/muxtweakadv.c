#include "../lvgl/lvgl.h"
#include "../lvgl/src/drivers/fbdev.h"
#include "../lvgl/src/drivers/evdev.h"
#include "ui/ui_muxtweakadv.h"
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

int accelerate_original, swap_original, thermal_original, font_original, volume_original, brightness_original,
        offset_original, lockdown_original, led_original, random_theme_original, retrowait_original,
        usbfunction_original, state_original, verbose_original, rumble_original, user_init_original,
        dpad_swap_original, overdrive_original, swapfile_original, cardmode_original;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

#define UI_COUNT 20
lv_obj_t *ui_objects[UI_COUNT];

lv_obj_t *ui_mux_panels[5];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblAccelerate,  lang.MUXTWEAKADV.HELP.SPEED},
            {ui_lblSwap,        lang.MUXTWEAKADV.HELP.SWAP},
            {ui_lblThermal,     lang.MUXTWEAKADV.HELP.THERMAL},
            {ui_lblFont,        lang.MUXTWEAKADV.HELP.FONT},
            {ui_lblVolume,      lang.MUXTWEAKADV.HELP.VOLUME},
            {ui_lblBrightness,  lang.MUXTWEAKADV.HELP.BRIGHT},
            {ui_lblOffset,      lang.MUXTWEAKADV.HELP.OFFSET},
            {ui_lblPasscode,    lang.MUXTWEAKADV.HELP.LOCK},
            {ui_lblLED,         lang.MUXTWEAKADV.HELP.LED},
            {ui_lblTheme,       lang.MUXTWEAKADV.HELP.RANDOM},
            {ui_lblRetroWait,   lang.MUXTWEAKADV.HELP.NET_WAIT},
            {ui_lblUSBFunction, lang.MUXTWEAKADV.HELP.USB},
            {ui_lblState,       lang.MUXTWEAKADV.HELP.STATE},
            {ui_lblVerbose,     lang.MUXTWEAKADV.HELP.VERBOSE},
            {ui_lblRumble,      lang.MUXTWEAKADV.HELP.RUMBLE},
            {ui_lblUserInit,    lang.MUXTWEAKADV.HELP.USER_INIT},
            {ui_lblDPADSwap,    lang.MUXTWEAKADV.HELP.DPAD},
            {ui_lblOverdrive,   lang.MUXTWEAKADV.HELP.OVERDRIVE},
            {ui_lblSwapfile,    lang.MUXTWEAKADV.HELP.SWAPFILE},
            {ui_lblCardMode,    lang.MUXTWEAKADV.HELP.TUNING},
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
            ui_droAccelerate,
            ui_droSwap,
            ui_droThermal,
            ui_droFont,
            ui_droVolume,
            ui_droBrightness,
            ui_droOffset,
            ui_droPasscode,
            ui_droLED,
            ui_droTheme,
            ui_droRetroWait,
            ui_droUSBFunction,
            ui_droState,
            ui_droVerbose,
            ui_droRumble,
            ui_droUserInit,
            ui_droDPADSwap,
            ui_droOverdrive,
            ui_droSwapfile,
            ui_droCardMode
    };

    for (unsigned int i = 0; i < sizeof(dropdowns) / sizeof(dropdowns[0]); i++) {
        lv_obj_add_event_cb(dropdowns[i], dropdown_event_handler, LV_EVENT_ALL, NULL);
    }
}

void init_dropdown_settings() {
    accelerate_original = lv_dropdown_get_selected(ui_droAccelerate);
    swap_original = lv_dropdown_get_selected(ui_droSwap);
    thermal_original = lv_dropdown_get_selected(ui_droThermal);
    font_original = lv_dropdown_get_selected(ui_droFont);
    volume_original = lv_dropdown_get_selected(ui_droVolume);
    brightness_original = lv_dropdown_get_selected(ui_droBrightness);
    offset_original = lv_dropdown_get_selected(ui_droOffset);
    lockdown_original = lv_dropdown_get_selected(ui_droPasscode);
    led_original = lv_dropdown_get_selected(ui_droLED);
    random_theme_original = lv_dropdown_get_selected(ui_droTheme);
    retrowait_original = lv_dropdown_get_selected(ui_droRetroWait);
    usbfunction_original = lv_dropdown_get_selected(ui_droUSBFunction);
    state_original = lv_dropdown_get_selected(ui_droState);
    verbose_original = lv_dropdown_get_selected(ui_droVerbose);
    rumble_original = lv_dropdown_get_selected(ui_droRumble);
    user_init_original = lv_dropdown_get_selected(ui_droUserInit);
    dpad_swap_original = lv_dropdown_get_selected(ui_droDPADSwap);
    overdrive_original = lv_dropdown_get_selected(ui_droOverdrive);
    swapfile_original = lv_dropdown_get_selected(ui_droSwapfile);
    cardmode_original = lv_dropdown_get_selected(ui_droCardMode);
}

void restore_tweak_options() {
    map_drop_down_to_index(ui_droAccelerate, config.SETTINGS.ADVANCED.ACCELERATE,
                           (int[]) {32767, 16, 32, 48, 64, 80, 96, 112, 128,
                                    144, 160, 176, 192, 208, 224, 240, 256}, 17, 6);

    lv_dropdown_set_selected(ui_droSwap, config.SETTINGS.ADVANCED.SWAP);
    lv_dropdown_set_selected(ui_droThermal, config.SETTINGS.ADVANCED.THERMAL);
    lv_dropdown_set_selected(ui_droFont, config.SETTINGS.ADVANCED.FONT);

    const char *volume_type = config.SETTINGS.ADVANCED.VOLUME;
    int volume_index = 0;
    if (!strcasecmp(volume_type, "previous")) {
        volume_index = 0;
    } else if (!strcasecmp(volume_type, "quiet")) {
        volume_index = 1;
    } else if (!strcasecmp(volume_type, "loud")) {
        volume_index = 2;
    }
    lv_dropdown_set_selected(ui_droVolume, volume_index);

    const char *brightness_type = config.SETTINGS.ADVANCED.BRIGHTNESS;
    int brightness_index = 0;
    if (!strcasecmp(brightness_type, "previous")) {
        brightness_index = 0;
    } else if (!strcasecmp(brightness_type, "low")) {
        brightness_index = 1;
    } else if (!strcasecmp(brightness_type, "high")) {
        brightness_index = 2;
    }
    lv_dropdown_set_selected(ui_droBrightness, brightness_index);

    lv_dropdown_set_selected(ui_droOffset, config.SETTINGS.ADVANCED.OFFSET);
    lv_dropdown_set_selected(ui_droPasscode, config.SETTINGS.ADVANCED.LOCK);
    lv_dropdown_set_selected(ui_droLED, config.SETTINGS.ADVANCED.LED);
    lv_dropdown_set_selected(ui_droTheme, config.SETTINGS.ADVANCED.THEME);
    lv_dropdown_set_selected(ui_droRetroWait, config.SETTINGS.ADVANCED.RETROWAIT);

    const char *usb_type = config.SETTINGS.ADVANCED.USBFUNCTION;
    if (!strcasecmp(usb_type, "adb")) {
        lv_dropdown_set_selected(ui_droUSBFunction, 1);
    } else if (!strcasecmp(usb_type, "mtp")) {
        lv_dropdown_set_selected(ui_droUSBFunction, 2);
    } else {
        lv_dropdown_set_selected(ui_droUSBFunction, 0);
    }

    const char *state_type = config.SETTINGS.ADVANCED.STATE;
    if (!strcasecmp(state_type, "freeze")) {
        lv_dropdown_set_selected(ui_droState, 1);
    } else {
        lv_dropdown_set_selected(ui_droState, 0);
    }

    lv_dropdown_set_selected(ui_droVerbose, config.SETTINGS.ADVANCED.VERBOSE);
    lv_dropdown_set_selected(ui_droRumble, config.SETTINGS.ADVANCED.RUMBLE);
    lv_dropdown_set_selected(ui_droUserInit, config.SETTINGS.ADVANCED.USERINIT);
    lv_dropdown_set_selected(ui_droDPADSwap, config.SETTINGS.ADVANCED.DPADSWAP);
    lv_dropdown_set_selected(ui_droOverdrive, config.SETTINGS.ADVANCED.OVERDRIVE);

    map_drop_down_to_index(ui_droSwapfile, config.SETTINGS.ADVANCED.SWAPFILE,
                           (int[]) {0, 64, 128, 192, 256, 320, 384, 448, 512}, 9, 0);

    const char *card_type = config.SETTINGS.ADVANCED.CARDMODE;
    if (!strcasecmp(card_type, "noop")) {
        lv_dropdown_set_selected(ui_droCardMode, 1);
    } else {
        lv_dropdown_set_selected(ui_droCardMode, 0);
    }

}

void save_tweak_options() {
    int idx_accelerate = map_drop_down_to_value(lv_dropdown_get_selected(ui_droAccelerate),
                                                (int[]) {32767, 16, 32, 48, 64, 80, 96, 112, 128,
                                                         144, 160, 176, 192, 208, 224, 240, 256}, 17, 6);

    char *idx_volume;
    switch (lv_dropdown_get_selected(ui_droVolume)) {
        case 0:
            idx_volume = "previous";
            break;
        case 1:
            idx_volume = "quiet";
            break;
        case 2:
            idx_volume = "loud";
            break;
        default:
            idx_volume = "previous";
            break;
    }

    char *idx_brightness;
    switch (lv_dropdown_get_selected(ui_droBrightness)) {
        case 0:
            idx_brightness = "previous";
            break;
        case 1:
            idx_brightness = "low";
            break;
        case 2:
            idx_brightness = "high";
            break;
        default:
            idx_brightness = "previous";
            break;
    }

    char *idx_state;
    switch (lv_dropdown_get_selected(ui_droState)) {
        case 1:
            idx_state = "freeze";
            break;
        default:
            idx_state = "mem";
            break;
    }

    char *idx_usbfunction;
    switch (lv_dropdown_get_selected(ui_droUSBFunction)) {
        case 1:
            idx_usbfunction = "adb";
            break;
        case 2:
            idx_usbfunction = "mtp";
            break;
        default:
            idx_usbfunction = "none";
            break;
    }

    int idx_swap = lv_dropdown_get_selected(ui_droSwap);
    int idx_thermal = lv_dropdown_get_selected(ui_droThermal);
    int idx_font = lv_dropdown_get_selected(ui_droFont);
    int idx_offset = lv_dropdown_get_selected(ui_droOffset);
    int idx_lockdown = lv_dropdown_get_selected(ui_droPasscode);
    int idx_led = lv_dropdown_get_selected(ui_droLED);
    int idx_random_theme = lv_dropdown_get_selected(ui_droTheme);
    int idx_retrowait = lv_dropdown_get_selected(ui_droRetroWait);
    int idx_verbose = lv_dropdown_get_selected(ui_droVerbose);
    int idx_rumble = lv_dropdown_get_selected(ui_droRumble);
    int idx_user_init = lv_dropdown_get_selected(ui_droUserInit);
    int idx_dpad_swap = lv_dropdown_get_selected(ui_droDPADSwap);
    int idx_overdrive = lv_dropdown_get_selected(ui_droOverdrive);

    int idx_swapfile = map_drop_down_to_value(lv_dropdown_get_selected(ui_droSwapfile),
                                              (int[]) {0, 64, 128, 192, 256, 320, 384, 448, 512}, 9, 0);

    char *idx_cardmode;
    switch (lv_dropdown_get_selected(ui_droCardMode)) {
        case 1:
            idx_cardmode = "noop";
            break;
        default:
            idx_cardmode = "deadline";
            break;
    }

    int is_modified = 0;

    if (lv_dropdown_get_selected(ui_droAccelerate) != accelerate_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/accelerate"), "w", INT, idx_accelerate);
    }

    if (lv_dropdown_get_selected(ui_droSwap) != swap_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/swap"), "w", INT, idx_swap);
    }

    if (lv_dropdown_get_selected(ui_droThermal) != thermal_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/thermal"), "w", INT, idx_thermal);
    }

    if (lv_dropdown_get_selected(ui_droFont) != font_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/font"), "w", INT, idx_font);
    }

    if (lv_dropdown_get_selected(ui_droVolume) != volume_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/volume"), "w", CHAR, idx_volume);
    }

    if (lv_dropdown_get_selected(ui_droBrightness) != brightness_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/brightness"), "w", CHAR, idx_brightness);
    }

    if (lv_dropdown_get_selected(ui_droOffset) != offset_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/offset"), "w", INT, idx_offset);
    }

    if (lv_dropdown_get_selected(ui_droPasscode) != lockdown_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/lock"), "w", INT, idx_lockdown);
    }

    if (lv_dropdown_get_selected(ui_droLED) != led_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/led"), "w", INT, idx_led);
    }

    if (lv_dropdown_get_selected(ui_droTheme) != random_theme_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/random_theme"), "w", INT, idx_random_theme);
    }

    if (lv_dropdown_get_selected(ui_droRetroWait) != retrowait_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/retrowait"), "w", INT, idx_retrowait);
    }

    if (lv_dropdown_get_selected(ui_droUSBFunction) != usbfunction_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/usb_function"), "w", CHAR, idx_usbfunction);
    }

    if (lv_dropdown_get_selected(ui_droState) != state_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/state"), "w", CHAR, idx_state);
    }

    if (lv_dropdown_get_selected(ui_droVerbose) != verbose_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/verbose"), "w", INT, idx_verbose);
    }

    if (lv_dropdown_get_selected(ui_droRumble) != rumble_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/rumble"), "w", INT, idx_rumble);
    }

    if (lv_dropdown_get_selected(ui_droUserInit) != user_init_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/user_init"), "w", INT, idx_user_init);
    }

    if (lv_dropdown_get_selected(ui_droDPADSwap) != dpad_swap_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/dpad_swap"), "w", INT, idx_dpad_swap);
    }

    if (lv_dropdown_get_selected(ui_droOverdrive) != overdrive_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/overdrive"), "w", INT, idx_overdrive);
    }

    if (lv_dropdown_get_selected(ui_droSwapfile) != swapfile_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/swapfile"), "w", INT, idx_swapfile);
    }

    if (lv_dropdown_get_selected(ui_droCardMode) != cardmode_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/cardmode"), "w", CHAR, idx_cardmode);
    }

    if (is_modified > 0) run_exec((const char *[]) {(char *) INTERNAL_PATH "script/mux/tweak.sh", NULL});
}

void init_navigation_groups() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlAccelerate,
            ui_pnlSwap,
            ui_pnlThermal,
            ui_pnlFont,
            ui_pnlVolume,
            ui_pnlBrightness,
            ui_pnlOffset,
            ui_pnlPasscode,
            ui_pnlLED,
            ui_pnlTheme,
            ui_pnlRetroWait,
            ui_pnlUSBFunction,
            ui_pnlState,
            ui_pnlVerbose,
            ui_pnlRumble,
            ui_pnlUserInit,
            ui_pnlDPADSwap,
            ui_pnlOverdrive,
            ui_pnlSwapfile,
            ui_pnlCardMode
    };

    ui_objects[0] = ui_lblAccelerate;
    ui_objects[1] = ui_lblSwap;
    ui_objects[2] = ui_lblThermal;
    ui_objects[3] = ui_lblFont;
    ui_objects[4] = ui_lblVolume;
    ui_objects[5] = ui_lblBrightness;
    ui_objects[6] = ui_lblOffset;
    ui_objects[7] = ui_lblPasscode;
    ui_objects[8] = ui_lblLED;
    ui_objects[9] = ui_lblTheme;
    ui_objects[10] = ui_lblRetroWait;
    ui_objects[11] = ui_lblUSBFunction;
    ui_objects[12] = ui_lblState;
    ui_objects[13] = ui_lblVerbose;
    ui_objects[14] = ui_lblRumble;
    ui_objects[15] = ui_lblUserInit;
    ui_objects[16] = ui_lblDPADSwap;
    ui_objects[17] = ui_lblOverdrive;
    ui_objects[18] = ui_lblSwapfile;
    ui_objects[19] = ui_lblCardMode;

    lv_obj_t *ui_objects_value[] = {
            ui_droAccelerate,
            ui_droSwap,
            ui_droThermal,
            ui_droFont,
            ui_droVolume,
            ui_droBrightness,
            ui_droOffset,
            ui_droPasscode,
            ui_droLED,
            ui_droTheme,
            ui_droRetroWait,
            ui_droUSBFunction,
            ui_droState,
            ui_droVerbose,
            ui_droRumble,
            ui_droUserInit,
            ui_droDPADSwap,
            ui_droOverdrive,
            ui_droSwapfile,
            ui_droCardMode
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoAccelerate,
            ui_icoSwap,
            ui_icoThermal,
            ui_icoFont,
            ui_icoVolume,
            ui_icoBrightness,
            ui_icoOffset,
            ui_icoPasscode,
            ui_icoLED,
            ui_icoTheme,
            ui_icoRetroWait,
            ui_icoUSBFunction,
            ui_icoState,
            ui_icoVerbose,
            ui_icoRumble,
            ui_icoUserInit,
            ui_icoDPADSwap,
            ui_icoOverdrive,
            ui_icoSwapfile,
            ui_icoCardMode
    };

    apply_theme_list_panel(&theme, &device, ui_pnlAccelerate);
    apply_theme_list_panel(&theme, &device, ui_pnlSwap);
    apply_theme_list_panel(&theme, &device, ui_pnlThermal);
    apply_theme_list_panel(&theme, &device, ui_pnlFont);
    apply_theme_list_panel(&theme, &device, ui_pnlVolume);
    apply_theme_list_panel(&theme, &device, ui_pnlBrightness);
    apply_theme_list_panel(&theme, &device, ui_pnlOffset);
    apply_theme_list_panel(&theme, &device, ui_pnlPasscode);
    apply_theme_list_panel(&theme, &device, ui_pnlLED);
    apply_theme_list_panel(&theme, &device, ui_pnlTheme);
    apply_theme_list_panel(&theme, &device, ui_pnlRetroWait);
    apply_theme_list_panel(&theme, &device, ui_pnlUSBFunction);
    apply_theme_list_panel(&theme, &device, ui_pnlState);
    apply_theme_list_panel(&theme, &device, ui_pnlVerbose);
    apply_theme_list_panel(&theme, &device, ui_pnlRumble);
    apply_theme_list_panel(&theme, &device, ui_pnlUserInit);
    apply_theme_list_panel(&theme, &device, ui_pnlDPADSwap);
    apply_theme_list_panel(&theme, &device, ui_pnlOverdrive);
    apply_theme_list_panel(&theme, &device, ui_pnlSwapfile);
    apply_theme_list_panel(&theme, &device, ui_pnlCardMode);

    apply_theme_list_item(&theme, ui_lblAccelerate, lang.MUXTWEAKADV.SPEED, false, true);
    apply_theme_list_item(&theme, ui_lblSwap, lang.MUXTWEAKADV.SWAP.TITLE, false, true);
    apply_theme_list_item(&theme, ui_lblThermal, lang.MUXTWEAKADV.THERMAL, false, true);
    apply_theme_list_item(&theme, ui_lblFont, lang.MUXTWEAKADV.FONT.TITLE, false, true);
    apply_theme_list_item(&theme, ui_lblVolume, lang.MUXTWEAKADV.VOLUME.TITLE, false, true);
    apply_theme_list_item(&theme, ui_lblBrightness, lang.MUXTWEAKADV.BRIGHT.TITLE, false, true);
    apply_theme_list_item(&theme, ui_lblOffset, lang.MUXTWEAKADV.OFFSET, false, true);
    apply_theme_list_item(&theme, ui_lblPasscode, lang.MUXTWEAKADV.LOCK, false, true);
    apply_theme_list_item(&theme, ui_lblLED, lang.MUXTWEAKADV.LED, false, true);
    apply_theme_list_item(&theme, ui_lblTheme, lang.MUXTWEAKADV.RANDOM, false, true);
    apply_theme_list_item(&theme, ui_lblRetroWait, lang.MUXTWEAKADV.NET_WAIT, false, true);
    apply_theme_list_item(&theme, ui_lblUSBFunction, lang.MUXTWEAKADV.USB, false, true);
    apply_theme_list_item(&theme, ui_lblState, lang.MUXTWEAKADV.STATE, false, true);
    apply_theme_list_item(&theme, ui_lblVerbose, lang.MUXTWEAKADV.VERBOSE, false, true);
    apply_theme_list_item(&theme, ui_lblRumble, lang.MUXTWEAKADV.RUMBLE.TITLE, false, true);
    apply_theme_list_item(&theme, ui_lblUserInit, lang.MUXTWEAKADV.USER_INIT, false, true);
    apply_theme_list_item(&theme, ui_lblDPADSwap, lang.MUXTWEAKADV.DPAD, false, true);
    apply_theme_list_item(&theme, ui_lblOverdrive, lang.MUXTWEAKADV.OVERDRIVE, false, true);
    apply_theme_list_item(&theme, ui_lblSwapfile, lang.MUXTWEAKADV.SWAPFILE, false, true);
    apply_theme_list_item(&theme, ui_lblCardMode, lang.MUXTWEAKADV.TUNING, false, true);

    apply_theme_list_glyph(&theme, ui_icoAccelerate, mux_module, "accelerate");
    apply_theme_list_glyph(&theme, ui_icoSwap, mux_module, "swap");
    apply_theme_list_glyph(&theme, ui_icoThermal, mux_module, "thermal");
    apply_theme_list_glyph(&theme, ui_icoFont, mux_module, "font");
    apply_theme_list_glyph(&theme, ui_icoVolume, mux_module, "volume");
    apply_theme_list_glyph(&theme, ui_icoBrightness, mux_module, "brightness");
    apply_theme_list_glyph(&theme, ui_icoOffset, mux_module, "offset");
    apply_theme_list_glyph(&theme, ui_icoPasscode, mux_module, "lock");
    apply_theme_list_glyph(&theme, ui_icoLED, mux_module, "led");
    apply_theme_list_glyph(&theme, ui_icoTheme, mux_module, "theme");
    apply_theme_list_glyph(&theme, ui_icoRetroWait, mux_module, "retrowait");
    apply_theme_list_glyph(&theme, ui_icoUSBFunction, mux_module, "usbfunction");
    apply_theme_list_glyph(&theme, ui_icoState, mux_module, "state");
    apply_theme_list_glyph(&theme, ui_icoVerbose, mux_module, "verbose");
    apply_theme_list_glyph(&theme, ui_icoRumble, mux_module, "rumble");
    apply_theme_list_glyph(&theme, ui_icoUserInit, mux_module, "userinit");
    apply_theme_list_glyph(&theme, ui_icoDPADSwap, mux_module, "dpadswap");
    apply_theme_list_glyph(&theme, ui_icoOverdrive, mux_module, "overdrive");
    apply_theme_list_glyph(&theme, ui_icoSwapfile, mux_module, "swapfile");
    apply_theme_list_glyph(&theme, ui_icoCardMode, mux_module, "cardmode");

    char *accelerate_string = generate_number_string(16, 256, 16, lang.GENERIC.DISABLED, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droAccelerate, accelerate_string);
    free(accelerate_string);

    apply_theme_list_drop_down(&theme, ui_droSwap, NULL);
    apply_theme_list_drop_down(&theme, ui_droThermal, NULL);
    apply_theme_list_drop_down(&theme, ui_droFont, NULL);
    apply_theme_list_drop_down(&theme, ui_droVolume, NULL);
    apply_theme_list_drop_down(&theme, ui_droBrightness, NULL);

    char *offset_string = generate_number_string(-50, 50, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droOffset, offset_string);
    free(offset_string);

    apply_theme_list_drop_down(&theme, ui_droPasscode, NULL);
    apply_theme_list_drop_down(&theme, ui_droLED, NULL);
    apply_theme_list_drop_down(&theme, ui_droTheme, NULL);
    apply_theme_list_drop_down(&theme, ui_droRetroWait, NULL);
    apply_theme_list_drop_down(&theme, ui_droUSBFunction, NULL);
    apply_theme_list_drop_down(&theme, ui_droState, NULL);
    apply_theme_list_drop_down(&theme, ui_droVerbose, NULL);
    apply_theme_list_drop_down(&theme, ui_droRumble, NULL);
    apply_theme_list_drop_down(&theme, ui_droUserInit, NULL);
    apply_theme_list_drop_down(&theme, ui_droDPADSwap, NULL);
    apply_theme_list_drop_down(&theme, ui_droOverdrive, NULL);

    char *swapfile_string = generate_number_string(64, 512, 64, lang.GENERIC.DISABLED, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droSwapfile, swapfile_string);
    free(swapfile_string);

    apply_theme_list_drop_down(&theme, ui_droCardMode, NULL);

    char *disabled_enabled[] = {lang.GENERIC.DISABLED, lang.GENERIC.ENABLED};
    add_drop_down_options(ui_droSwap, (char *[]) {
            lang.MUXTWEAKADV.SWAP.RETRO,
            lang.MUXTWEAKADV.SWAP.MODERN}, 2);

    add_drop_down_options(ui_droThermal, disabled_enabled, 2);

    add_drop_down_options(ui_droFont, (char *[]) {
            lang.MUXTWEAKADV.FONT.LANG,
            lang.MUXTWEAKADV.FONT.THEME}, 2);

    add_drop_down_options(ui_droVolume, (char *[]) {
            lang.GENERIC.PREVIOUS,
            lang.MUXTWEAKADV.VOLUME.QUIET,
            lang.MUXTWEAKADV.VOLUME.LOUD}, 3);

    add_drop_down_options(ui_droBrightness, (char *[]) {
            lang.GENERIC.PREVIOUS,
            lang.MUXTWEAKADV.BRIGHT.LOW,
            lang.MUXTWEAKADV.BRIGHT.HIGH}, 3);

    add_drop_down_options(ui_droPasscode, disabled_enabled, 2);
    add_drop_down_options(ui_droLED, disabled_enabled, 2);
    add_drop_down_options(ui_droTheme, disabled_enabled, 2);
    add_drop_down_options(ui_droRetroWait, disabled_enabled, 2);
    add_drop_down_options(ui_droUSBFunction, (char *[]) {lang.GENERIC.DISABLED, "ADB", "MTP"}, 3);
    add_drop_down_options(ui_droState, (char *[]) {"mem", "freeze"}, 2);
    add_drop_down_options(ui_droVerbose, disabled_enabled, 2);

    add_drop_down_options(ui_droRumble, (char *[]) {
            lang.GENERIC.DISABLED,
            lang.MUXTWEAKADV.RUMBLE.ST,
            lang.MUXTWEAKADV.RUMBLE.SH,
            lang.MUXTWEAKADV.RUMBLE.SL,
            lang.MUXTWEAKADV.RUMBLE.STSH,
            lang.MUXTWEAKADV.RUMBLE.STSL,
            lang.MUXTWEAKADV.RUMBLE.SHSL}, 7);

    add_drop_down_options(ui_droUserInit, disabled_enabled, 2);
    add_drop_down_options(ui_droDPADSwap, disabled_enabled, 2);
    add_drop_down_options(ui_droOverdrive, disabled_enabled, 2);
    add_drop_down_options(ui_droCardMode, (char *[]) {"deadline", "noop"}, 2);

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
        current_item_index = (!current_item_index) ? ui_count - 1 : current_item_index - 1;
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

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "advanced");
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

    lv_obj_set_user_data(ui_lblAccelerate, "accelerate");
    lv_obj_set_user_data(ui_lblSwap, "swap");
    lv_obj_set_user_data(ui_lblThermal, "thermal");
    lv_obj_set_user_data(ui_lblFont, "font");
    lv_obj_set_user_data(ui_lblVolume, "volume");
    lv_obj_set_user_data(ui_lblBrightness, "brightness");
    lv_obj_set_user_data(ui_lblOffset, "offset");
    lv_obj_set_user_data(ui_lblPasscode, "lock");
    lv_obj_set_user_data(ui_lblLED, "led");
    lv_obj_set_user_data(ui_lblTheme, "theme");
    lv_obj_set_user_data(ui_lblRetroWait, "retrowait");
    lv_obj_set_user_data(ui_lblUSBFunction, "usbfunction");
    lv_obj_set_user_data(ui_lblState, "state");
    lv_obj_set_user_data(ui_lblVerbose, "verbose");
    lv_obj_set_user_data(ui_lblRumble, "rumble");
    lv_obj_set_user_data(ui_lblUserInit, "userinit");
    lv_obj_set_user_data(ui_lblDPADSwap, "dpadswap");
    lv_obj_set_user_data(ui_lblOverdrive, "overdrive");
    lv_obj_set_user_data(ui_lblSwapfile, "swapfile");
    lv_obj_set_user_data(ui_lblCardMode, "cardmode");

    if (!device.DEVICE.HAS_NETWORK) {
        lv_obj_add_flag(ui_pnlRetroWait, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlRetroWait, LV_OBJ_FLAG_FLOATING);
        ui_count--;
    }

    if (TEST_IMAGE) display_testing_message(ui_screen);

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

            if (!strcasecmp(u_data, prev)) {
                text_hit = i;
                break;
            }
        }

        if (text_hit != 0) {
            list_nav_next(text_hit - !device.DEVICE.HAS_NETWORK);
            nav_moved = 1;
        }
    }
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

    load_theme(&theme, &config, &device, basename(argv[0]));

    ui_common_screen_init(&theme, &device, &lang, lang.MUXTWEAKADV.TITLE);
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
