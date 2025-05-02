#include "muxshare.h"
#include "muxtweakadv.h"
#include "ui/ui_muxtweakadv.h"
#include <string.h>
#include <stdlib.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/input/list_nav.h"

static int accelerate_original, swap_original, thermal_original, volume_original, brightness_original,
        offset_original, lockdown_original, led_original, random_theme_original, retrowait_original,
        state_original, verbose_original, rumble_original, user_init_original, dpad_swap_original,
        overdrive_original, swapfile_original, zramfile_original, cardmode_original;

#define UI_COUNT 19
static lv_obj_t *ui_objects[UI_COUNT];

#define UI_PANEL 5
static lv_obj_t *ui_mux_panels[UI_PANEL];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblAccelerate_tweakadv, lang.MUXTWEAKADV.HELP.SPEED},
            {ui_lblSwap_tweakadv,       lang.MUXTWEAKADV.HELP.SWAP},
            {ui_lblThermal_tweakadv,    lang.MUXTWEAKADV.HELP.THERMAL},
            {ui_lblVolume_tweakadv,     lang.MUXTWEAKADV.HELP.VOLUME},
            {ui_lblBrightness_tweakadv, lang.MUXTWEAKADV.HELP.BRIGHT},
            {ui_lblOffset_tweakadv,     lang.MUXTWEAKADV.HELP.OFFSET},
            {ui_lblPasscode_tweakadv,   lang.MUXTWEAKADV.HELP.LOCK},
            {ui_lblLED_tweakadv,        lang.MUXTWEAKADV.HELP.LED},
            {ui_lblTheme_tweakadv,      lang.MUXTWEAKADV.HELP.RANDOM},
            {ui_lblRetroWait_tweakadv,  lang.MUXTWEAKADV.HELP.NET_WAIT},
            {ui_lblState_tweakadv,      lang.MUXTWEAKADV.HELP.STATE},
            {ui_lblVerbose_tweakadv,    lang.MUXTWEAKADV.HELP.VERBOSE},
            {ui_lblRumble_tweakadv,     lang.MUXTWEAKADV.HELP.RUMBLE},
            {ui_lblUserInit_tweakadv,   lang.MUXTWEAKADV.HELP.USER_INIT},
            {ui_lblDPADSwap_tweakadv,   lang.MUXTWEAKADV.HELP.DPAD},
            {ui_lblOverdrive_tweakadv,  lang.MUXTWEAKADV.HELP.OVERDRIVE},
            {ui_lblSwap_tweakadvfile,   lang.MUXTWEAKADV.HELP.SWAPFILE},
            {ui_lblZramfile_tweakadv,   lang.MUXTWEAKADV.HELP.ZRAMFILE},
            {ui_lblCardMode_tweakadv,   lang.MUXTWEAKADV.HELP.TUNING},
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

static void init_element_events() {
    lv_obj_t *dropdowns[] = {
            ui_droAccelerate_tweakadv,
            ui_droSwap_tweakadv,
            ui_droThermal_tweakadv,
            ui_droVolume_tweakadv,
            ui_droBrightness_tweakadv,
            ui_droOffset_tweakadv,
            ui_droPasscode_tweakadv,
            ui_droLED_tweakadv,
            ui_droTheme_tweakadv,
            ui_droRetroWait_tweakadv,
            ui_droState_tweakadv,
            ui_droVerbose_tweakadv,
            ui_droRumble_tweakadv,
            ui_droUserInit_tweakadv,
            ui_droDPADSwap_tweakadv,
            ui_droOverdrive_tweakadv,
            ui_droSwap_tweakadvfile,
            ui_droZramfile_tweakadv,
            ui_droCardMode_tweakadv
    };

    for (unsigned int i = 0; i < sizeof(dropdowns) / sizeof(dropdowns[0]); i++) {
        lv_obj_add_event_cb(dropdowns[i], dropdown_event_handler, LV_EVENT_ALL, NULL);
    }
}

static void init_dropdown_settings() {
    accelerate_original = lv_dropdown_get_selected(ui_droAccelerate_tweakadv);
    swap_original = lv_dropdown_get_selected(ui_droSwap_tweakadv);
    thermal_original = lv_dropdown_get_selected(ui_droThermal_tweakadv);
    volume_original = lv_dropdown_get_selected(ui_droVolume_tweakadv);
    brightness_original = lv_dropdown_get_selected(ui_droBrightness_tweakadv);
    offset_original = lv_dropdown_get_selected(ui_droOffset_tweakadv);
    lockdown_original = lv_dropdown_get_selected(ui_droPasscode_tweakadv);
    led_original = lv_dropdown_get_selected(ui_droLED_tweakadv);
    random_theme_original = lv_dropdown_get_selected(ui_droTheme_tweakadv);
    retrowait_original = lv_dropdown_get_selected(ui_droRetroWait_tweakadv);
    state_original = lv_dropdown_get_selected(ui_droState_tweakadv);
    verbose_original = lv_dropdown_get_selected(ui_droVerbose_tweakadv);
    rumble_original = lv_dropdown_get_selected(ui_droRumble_tweakadv);
    user_init_original = lv_dropdown_get_selected(ui_droUserInit_tweakadv);
    dpad_swap_original = lv_dropdown_get_selected(ui_droDPADSwap_tweakadv);
    overdrive_original = lv_dropdown_get_selected(ui_droOverdrive_tweakadv);
    swapfile_original = lv_dropdown_get_selected(ui_droSwap_tweakadvfile);
    zramfile_original = lv_dropdown_get_selected(ui_droZramfile_tweakadv);
    cardmode_original = lv_dropdown_get_selected(ui_droCardMode_tweakadv);
}

static void restore_tweak_options() {
    map_drop_down_to_index(ui_droAccelerate_tweakadv, config.SETTINGS.ADVANCED.ACCELERATE,
                           (int[]) {32767, 16, 32, 48, 64, 80, 96, 112, 128,
                                    144, 160, 176, 192, 208, 224, 240, 256}, 17, 6);

    lv_dropdown_set_selected(ui_droSwap_tweakadv, config.SETTINGS.ADVANCED.SWAP);
    lv_dropdown_set_selected(ui_droThermal_tweakadv, config.SETTINGS.ADVANCED.THERMAL);

    const char *volume_type = config.SETTINGS.ADVANCED.VOLUME;
    int volume_index = 0;
    if (!strcasecmp(volume_type, "previous")) {
        volume_index = 0;
    } else if (!strcasecmp(volume_type, "silent")) {
        volume_index = 1;
    } else if (!strcasecmp(volume_type, "soft")) {
        volume_index = 2;
    } else if (!strcasecmp(volume_type, "loud")) {
        volume_index = 3;
    }
    lv_dropdown_set_selected(ui_droVolume_tweakadv, volume_index);

    const char *brightness_type = config.SETTINGS.ADVANCED.BRIGHTNESS;
    int brightness_index = 0;
    if (!strcasecmp(brightness_type, "previous")) {
        brightness_index = 0;
    } else if (!strcasecmp(brightness_type, "low")) {
        brightness_index = 1;
    } else if (!strcasecmp(brightness_type, "medium")) {
        brightness_index = 2;
    } else if (!strcasecmp(brightness_type, "high")) {
        brightness_index = 3;
    }
    lv_dropdown_set_selected(ui_droBrightness_tweakadv, brightness_index);

    lv_dropdown_set_selected(ui_droOffset_tweakadv, config.SETTINGS.ADVANCED.OFFSET);
    lv_dropdown_set_selected(ui_droPasscode_tweakadv, config.SETTINGS.ADVANCED.LOCK);
    lv_dropdown_set_selected(ui_droLED_tweakadv, config.SETTINGS.ADVANCED.LED);
    lv_dropdown_set_selected(ui_droTheme_tweakadv, config.SETTINGS.ADVANCED.THEME);
    lv_dropdown_set_selected(ui_droRetroWait_tweakadv, config.SETTINGS.ADVANCED.RETROWAIT);

    const char *state_type = config.SETTINGS.ADVANCED.STATE;
    if (!strcasecmp(state_type, "freeze")) {
        lv_dropdown_set_selected(ui_droState_tweakadv, 1);
    } else {
        lv_dropdown_set_selected(ui_droState_tweakadv, 0);
    }

    lv_dropdown_set_selected(ui_droVerbose_tweakadv, config.SETTINGS.ADVANCED.VERBOSE);
    lv_dropdown_set_selected(ui_droRumble_tweakadv, config.SETTINGS.ADVANCED.RUMBLE);
    lv_dropdown_set_selected(ui_droUserInit_tweakadv, config.SETTINGS.ADVANCED.USERINIT);
    lv_dropdown_set_selected(ui_droDPADSwap_tweakadv, config.SETTINGS.ADVANCED.DPADSWAP);
    lv_dropdown_set_selected(ui_droOverdrive_tweakadv, config.SETTINGS.ADVANCED.OVERDRIVE);

    map_drop_down_to_index(ui_droSwap_tweakadvfile, config.SETTINGS.ADVANCED.SWAPFILE,
                           (int[]) {0, 64, 128, 192, 256, 320, 384, 448, 512}, 9, 0);

    map_drop_down_to_index(ui_droZramfile_tweakadv, config.SETTINGS.ADVANCED.ZRAMFILE,
                           (int[]) {0, 64, 128, 192, 256, 320, 384, 448, 512}, 9, 0);

    const char *card_type = config.SETTINGS.ADVANCED.CARDMODE;
    if (!strcasecmp(card_type, "noop")) {
        lv_dropdown_set_selected(ui_droCardMode_tweakadv, 1);
    } else {
        lv_dropdown_set_selected(ui_droCardMode_tweakadv, 0);
    }

}

static void save_tweak_options() {
    int idx_accelerate = map_drop_down_to_value(lv_dropdown_get_selected(ui_droAccelerate_tweakadv),
                                                (int[]) {32767, 16, 32, 48, 64, 80, 96, 112, 128,
                                                         144, 160, 176, 192, 208, 224, 240, 256}, 17, 6);

    char *idx_volume;
    switch (lv_dropdown_get_selected(ui_droVolume_tweakadv)) {
        case 0:
            idx_volume = "previous";
            break;
        case 1:
            idx_volume = "silent";
            break;
        case 2:
            idx_volume = "soft";
            break;
        case 3:
            idx_volume = "loud";
            break;
        default:
            idx_volume = "previous";
            break;
    }

    char *idx_brightness;
    switch (lv_dropdown_get_selected(ui_droBrightness_tweakadv)) {
        case 0:
            idx_brightness = "previous";
            break;
        case 1:
            idx_brightness = "low";
            break;
        case 2:
            idx_brightness = "medium";
            break;
        case 3:
            idx_brightness = "high";
            break;
        default:
            idx_brightness = "previous";
            break;
    }

    char *idx_state;
    switch (lv_dropdown_get_selected(ui_droState_tweakadv)) {
        case 1:
            idx_state = "freeze";
            break;
        default:
            idx_state = "mem";
            break;
    }

    int idx_swap = lv_dropdown_get_selected(ui_droSwap_tweakadv);
    int idx_thermal = lv_dropdown_get_selected(ui_droThermal_tweakadv);
    int idx_offset = lv_dropdown_get_selected(ui_droOffset_tweakadv);
    int idx_lockdown = lv_dropdown_get_selected(ui_droPasscode_tweakadv);
    int idx_led = lv_dropdown_get_selected(ui_droLED_tweakadv);
    int idx_random_theme = lv_dropdown_get_selected(ui_droTheme_tweakadv);
    int idx_retrowait = lv_dropdown_get_selected(ui_droRetroWait_tweakadv);
    int idx_verbose = lv_dropdown_get_selected(ui_droVerbose_tweakadv);
    int idx_rumble = lv_dropdown_get_selected(ui_droRumble_tweakadv);
    int idx_user_init = lv_dropdown_get_selected(ui_droUserInit_tweakadv);
    int idx_dpad_swap = lv_dropdown_get_selected(ui_droDPADSwap_tweakadv);
    int idx_overdrive = lv_dropdown_get_selected(ui_droOverdrive_tweakadv);

    int idx_swapfile = map_drop_down_to_value(lv_dropdown_get_selected(ui_droSwap_tweakadvfile),
                                              (int[]) {0, 64, 128, 192, 256, 320, 384, 448, 512}, 9, 0);

    int idx_zramfile = map_drop_down_to_value(lv_dropdown_get_selected(ui_droZramfile_tweakadv),
                                              (int[]) {0, 64, 128, 192, 256, 320, 384, 448, 512}, 9, 0);

    char *idx_cardmode;
    switch (lv_dropdown_get_selected(ui_droCardMode_tweakadv)) {
        case 1:
            idx_cardmode = "noop";
            break;
        default:
            idx_cardmode = "deadline";
            break;
    }

    int is_modified = 0;

    if (lv_dropdown_get_selected(ui_droAccelerate_tweakadv) != accelerate_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/accelerate"), "w", INT, idx_accelerate);
    }

    if (lv_dropdown_get_selected(ui_droSwap_tweakadv) != swap_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/swap"), "w", INT, idx_swap);
    }

    if (lv_dropdown_get_selected(ui_droThermal_tweakadv) != thermal_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/thermal"), "w", INT, idx_thermal);
    }

    if (lv_dropdown_get_selected(ui_droVolume_tweakadv) != volume_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/volume"), "w", CHAR, idx_volume);
    }

    if (lv_dropdown_get_selected(ui_droBrightness_tweakadv) != brightness_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/brightness"), "w", CHAR, idx_brightness);
    }

    if (lv_dropdown_get_selected(ui_droOffset_tweakadv) != offset_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/offset"), "w", INT, idx_offset);
    }

    if (lv_dropdown_get_selected(ui_droPasscode_tweakadv) != lockdown_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/lock"), "w", INT, idx_lockdown);
    }

    if (lv_dropdown_get_selected(ui_droLED_tweakadv) != led_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/led"), "w", INT, idx_led);
    }

    if (lv_dropdown_get_selected(ui_droTheme_tweakadv) != random_theme_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/random_theme"), "w", INT, idx_random_theme);
    }

    if (lv_dropdown_get_selected(ui_droRetroWait_tweakadv) != retrowait_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/retrowait"), "w", INT, idx_retrowait);
    }

    if (lv_dropdown_get_selected(ui_droState_tweakadv) != state_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/state"), "w", CHAR, idx_state);
    }

    if (lv_dropdown_get_selected(ui_droVerbose_tweakadv) != verbose_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/verbose"), "w", INT, idx_verbose);
    }

    if (lv_dropdown_get_selected(ui_droRumble_tweakadv) != rumble_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/rumble"), "w", INT, idx_rumble);
    }

    if (lv_dropdown_get_selected(ui_droUserInit_tweakadv) != user_init_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/user_init"), "w", INT, idx_user_init);
    }

    if (lv_dropdown_get_selected(ui_droDPADSwap_tweakadv) != dpad_swap_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/dpad_swap"), "w", INT, idx_dpad_swap);
    }

    if (lv_dropdown_get_selected(ui_droOverdrive_tweakadv) != overdrive_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/overdrive"), "w", INT, idx_overdrive);
    }

    if (lv_dropdown_get_selected(ui_droSwap_tweakadvfile) != swapfile_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/swapfile"), "w", INT, idx_swapfile);
    }

    if (lv_dropdown_get_selected(ui_droZramfile_tweakadv) != zramfile_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/zramfile"), "w", INT, idx_zramfile);
    }

    if (lv_dropdown_get_selected(ui_droCardMode_tweakadv) != cardmode_original) {
        is_modified++;
        write_text_to_file((RUN_GLOBAL_PATH "settings/advanced/cardmode"), "w", CHAR, idx_cardmode);
    }

    if (is_modified > 0) {
        const char *args[] = {(INTERNAL_PATH "script/mux/tweak.sh"), NULL};
        run_exec(args, A_SIZE(args), 0);
        refresh_config = 1;
    }
}

static void init_navigation_group() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlSwap_tweakadv,
            ui_pnlOverdrive_tweakadv,
            ui_pnlOffset_tweakadv,
            ui_pnlBrightness_tweakadv,
            ui_pnlRumble_tweakadv,
            ui_pnlCardMode_tweakadv,
            ui_pnlDPADSwap_tweakadv,
            ui_pnlLED_tweakadv,
            ui_pnlAccelerate_tweakadv,
            ui_pnlThermal_tweakadv,
            ui_pnlPasscode_tweakadv,
            ui_pnlTheme_tweakadv,
            ui_pnlRetroWait_tweakadv,
            ui_pnlState_tweakadv,
            ui_pnlSwap_tweakadvfile,
            ui_pnlZramfile_tweakadv,
            ui_pnlUserInit_tweakadv,
            ui_pnlVerbose_tweakadv,
            ui_pnlVolume_tweakadv
    };

    ui_objects[0] = ui_lblSwap_tweakadv;
    ui_objects[1] = ui_lblOverdrive_tweakadv;
    ui_objects[2] = ui_lblOffset_tweakadv;
    ui_objects[3] = ui_lblBrightness_tweakadv;
    ui_objects[4] = ui_lblRumble_tweakadv;
    ui_objects[5] = ui_lblCardMode_tweakadv;
    ui_objects[6] = ui_lblDPADSwap_tweakadv;
    ui_objects[7] = ui_lblLED_tweakadv;
    ui_objects[8] = ui_lblAccelerate_tweakadv;
    ui_objects[9] = ui_lblThermal_tweakadv;
    ui_objects[10] = ui_lblPasscode_tweakadv;
    ui_objects[11] = ui_lblTheme_tweakadv;
    ui_objects[12] = ui_lblRetroWait_tweakadv;
    ui_objects[13] = ui_lblState_tweakadv;
    ui_objects[14] = ui_lblSwap_tweakadvfile;
    ui_objects[15] = ui_lblZramfile_tweakadv;
    ui_objects[16] = ui_lblUserInit_tweakadv;
    ui_objects[17] = ui_lblVerbose_tweakadv;
    ui_objects[18] = ui_lblVolume_tweakadv;

    lv_obj_t *ui_objects_value[] = {
            ui_droSwap_tweakadv,
            ui_droOverdrive_tweakadv,
            ui_droOffset_tweakadv,
            ui_droBrightness_tweakadv,
            ui_droRumble_tweakadv,
            ui_droCardMode_tweakadv,
            ui_droDPADSwap_tweakadv,
            ui_droLED_tweakadv,
            ui_droAccelerate_tweakadv,
            ui_droThermal_tweakadv,
            ui_droPasscode_tweakadv,
            ui_droTheme_tweakadv,
            ui_droRetroWait_tweakadv,
            ui_droState_tweakadv,
            ui_droSwap_tweakadvfile,
            ui_droZramfile_tweakadv,
            ui_droUserInit_tweakadv,
            ui_droVerbose_tweakadv,
            ui_droVolume_tweakadv
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoSwap_tweakadv,
            ui_icoOverdrive_tweakadv,
            ui_icoOffset_tweakadv,
            ui_icoBrightness_tweakadv,
            ui_icoRumble_tweakadv,
            ui_icoCardMode_tweakadv,
            ui_icoDPADSwap_tweakadv,
            ui_icoLED_tweakadv,
            ui_icoAccelerate_tweakadv,
            ui_icoThermal_tweakadv,
            ui_icoPasscode_tweakadv,
            ui_icoTheme_tweakadv,
            ui_icoRetroWait_tweakadv,
            ui_icoState_tweakadv,
            ui_icoSwap_tweakadvfile,
            ui_icoZramfile_tweakadv,
            ui_icoUserInit_tweakadv,
            ui_icoVerbose_tweakadv,
            ui_icoVolume_tweakadv
    };

    apply_theme_list_panel(ui_pnlAccelerate_tweakadv);
    apply_theme_list_panel(ui_pnlSwap_tweakadv);
    apply_theme_list_panel(ui_pnlThermal_tweakadv);
    apply_theme_list_panel(ui_pnlVolume_tweakadv);
    apply_theme_list_panel(ui_pnlBrightness_tweakadv);
    apply_theme_list_panel(ui_pnlOffset_tweakadv);
    apply_theme_list_panel(ui_pnlPasscode_tweakadv);
    apply_theme_list_panel(ui_pnlLED_tweakadv);
    apply_theme_list_panel(ui_pnlTheme_tweakadv);
    apply_theme_list_panel(ui_pnlRetroWait_tweakadv);
    apply_theme_list_panel(ui_pnlState_tweakadv);
    apply_theme_list_panel(ui_pnlVerbose_tweakadv);
    apply_theme_list_panel(ui_pnlRumble_tweakadv);
    apply_theme_list_panel(ui_pnlUserInit_tweakadv);
    apply_theme_list_panel(ui_pnlDPADSwap_tweakadv);
    apply_theme_list_panel(ui_pnlOverdrive_tweakadv);
    apply_theme_list_panel(ui_pnlSwap_tweakadvfile);
    apply_theme_list_panel(ui_pnlZramfile_tweakadv);
    apply_theme_list_panel(ui_pnlCardMode_tweakadv);

    apply_theme_list_item(&theme, ui_lblAccelerate_tweakadv, lang.MUXTWEAKADV.SPEED);
    apply_theme_list_item(&theme, ui_lblSwap_tweakadv, lang.MUXTWEAKADV.SWAP.TITLE);
    apply_theme_list_item(&theme, ui_lblThermal_tweakadv, lang.MUXTWEAKADV.THERMAL);
    apply_theme_list_item(&theme, ui_lblVolume_tweakadv, lang.MUXTWEAKADV.VOLUME.TITLE);
    apply_theme_list_item(&theme, ui_lblBrightness_tweakadv, lang.MUXTWEAKADV.BRIGHT.TITLE);
    apply_theme_list_item(&theme, ui_lblOffset_tweakadv, lang.MUXTWEAKADV.OFFSET);
    apply_theme_list_item(&theme, ui_lblPasscode_tweakadv, lang.MUXTWEAKADV.LOCK);
    apply_theme_list_item(&theme, ui_lblLED_tweakadv, lang.MUXTWEAKADV.LED);
    apply_theme_list_item(&theme, ui_lblTheme_tweakadv, lang.MUXTWEAKADV.RANDOM);
    apply_theme_list_item(&theme, ui_lblRetroWait_tweakadv, lang.MUXTWEAKADV.NET_WAIT);
    apply_theme_list_item(&theme, ui_lblState_tweakadv, lang.MUXTWEAKADV.STATE);
    apply_theme_list_item(&theme, ui_lblVerbose_tweakadv, lang.MUXTWEAKADV.VERBOSE);
    apply_theme_list_item(&theme, ui_lblRumble_tweakadv, lang.MUXTWEAKADV.RUMBLE.TITLE);
    apply_theme_list_item(&theme, ui_lblUserInit_tweakadv, lang.MUXTWEAKADV.USER_INIT);
    apply_theme_list_item(&theme, ui_lblDPADSwap_tweakadv, lang.MUXTWEAKADV.DPAD);
    apply_theme_list_item(&theme, ui_lblOverdrive_tweakadv, lang.MUXTWEAKADV.OVERDRIVE);
    apply_theme_list_item(&theme, ui_lblSwap_tweakadvfile, lang.MUXTWEAKADV.SWAPFILE);
    apply_theme_list_item(&theme, ui_lblZramfile_tweakadv, lang.MUXTWEAKADV.ZRAMFILE);
    apply_theme_list_item(&theme, ui_lblCardMode_tweakadv, lang.MUXTWEAKADV.TUNING);

    apply_theme_list_glyph(&theme, ui_icoAccelerate_tweakadv, mux_module, "accelerate");
    apply_theme_list_glyph(&theme, ui_icoSwap_tweakadv, mux_module, "swap");
    apply_theme_list_glyph(&theme, ui_icoThermal_tweakadv, mux_module, "thermal");
    apply_theme_list_glyph(&theme, ui_icoVolume_tweakadv, mux_module, "volume");
    apply_theme_list_glyph(&theme, ui_icoBrightness_tweakadv, mux_module, "brightness");
    apply_theme_list_glyph(&theme, ui_icoOffset_tweakadv, mux_module, "offset");
    apply_theme_list_glyph(&theme, ui_icoPasscode_tweakadv, mux_module, "lock");
    apply_theme_list_glyph(&theme, ui_icoLED_tweakadv, mux_module, "led");
    apply_theme_list_glyph(&theme, ui_icoTheme_tweakadv, mux_module, "theme");
    apply_theme_list_glyph(&theme, ui_icoRetroWait_tweakadv, mux_module, "retrowait");
    apply_theme_list_glyph(&theme, ui_icoState_tweakadv, mux_module, "state");
    apply_theme_list_glyph(&theme, ui_icoVerbose_tweakadv, mux_module, "verbose");
    apply_theme_list_glyph(&theme, ui_icoRumble_tweakadv, mux_module, "rumble");
    apply_theme_list_glyph(&theme, ui_icoUserInit_tweakadv, mux_module, "userinit");
    apply_theme_list_glyph(&theme, ui_icoDPADSwap_tweakadv, mux_module, "dpadswap");
    apply_theme_list_glyph(&theme, ui_icoOverdrive_tweakadv, mux_module, "overdrive");
    apply_theme_list_glyph(&theme, ui_icoSwap_tweakadvfile, mux_module, "swapfile");
    apply_theme_list_glyph(&theme, ui_icoZramfile_tweakadv, mux_module, "zramfile");
    apply_theme_list_glyph(&theme, ui_icoCardMode_tweakadv, mux_module, "cardmode");

    char *accelerate_string = generate_number_string(16, 256, 16, lang.GENERIC.DISABLED, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droAccelerate_tweakadv, accelerate_string);
    free(accelerate_string);

    apply_theme_list_drop_down(&theme, ui_droSwap_tweakadv, NULL);
    apply_theme_list_drop_down(&theme, ui_droThermal_tweakadv, NULL);
    apply_theme_list_drop_down(&theme, ui_droVolume_tweakadv, NULL);
    apply_theme_list_drop_down(&theme, ui_droBrightness_tweakadv, NULL);

    char *offset_string = generate_number_string(-50, 50, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droOffset_tweakadv, offset_string);
    free(offset_string);

    apply_theme_list_drop_down(&theme, ui_droPasscode_tweakadv, NULL);
    apply_theme_list_drop_down(&theme, ui_droLED_tweakadv, NULL);
    apply_theme_list_drop_down(&theme, ui_droTheme_tweakadv, NULL);
    apply_theme_list_drop_down(&theme, ui_droRetroWait_tweakadv, NULL);
    apply_theme_list_drop_down(&theme, ui_droState_tweakadv, NULL);
    apply_theme_list_drop_down(&theme, ui_droVerbose_tweakadv, NULL);
    apply_theme_list_drop_down(&theme, ui_droRumble_tweakadv, NULL);
    apply_theme_list_drop_down(&theme, ui_droUserInit_tweakadv, NULL);
    apply_theme_list_drop_down(&theme, ui_droDPADSwap_tweakadv, NULL);
    apply_theme_list_drop_down(&theme, ui_droOverdrive_tweakadv, NULL);

    char *swapfile_string = generate_number_string(64, 512, 64, lang.GENERIC.DISABLED, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droSwap_tweakadvfile, swapfile_string);
    free(swapfile_string);

    // We could reuse from above but just incase we want to increase it we'll do it separately!
    char *zramfile_string = generate_number_string(64, 512, 64, lang.GENERIC.DISABLED, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droZramfile_tweakadv, zramfile_string);
    free(zramfile_string);

    apply_theme_list_drop_down(&theme, ui_droCardMode_tweakadv, NULL);

    char *disabled_enabled[] = {lang.GENERIC.DISABLED, lang.GENERIC.ENABLED};
    add_drop_down_options(ui_droSwap_tweakadv, (char *[]) {
            lang.MUXTWEAKADV.SWAP.RETRO,
            lang.MUXTWEAKADV.SWAP.MODERN}, 2);

    add_drop_down_options(ui_droThermal_tweakadv, disabled_enabled, 2);

    add_drop_down_options(ui_droVolume_tweakadv, (char *[]) {
            lang.GENERIC.PREVIOUS,
            lang.MUXTWEAKADV.VOLUME.SILENT,
            lang.MUXTWEAKADV.VOLUME.SOFT,
            lang.MUXTWEAKADV.VOLUME.LOUD}, 3);

    add_drop_down_options(ui_droBrightness_tweakadv, (char *[]) {
            lang.GENERIC.PREVIOUS,
            lang.MUXTWEAKADV.BRIGHT.LOW,
            lang.MUXTWEAKADV.BRIGHT.MEDIUM,
            lang.MUXTWEAKADV.BRIGHT.HIGH}, 3);

    add_drop_down_options(ui_droPasscode_tweakadv, disabled_enabled, 2);
    add_drop_down_options(ui_droLED_tweakadv, disabled_enabled, 2);
    add_drop_down_options(ui_droTheme_tweakadv, disabled_enabled, 2);
    add_drop_down_options(ui_droRetroWait_tweakadv, disabled_enabled, 2);
    add_drop_down_options(ui_droState_tweakadv, (char *[]) {"mem", "freeze"}, 2);
    add_drop_down_options(ui_droVerbose_tweakadv, disabled_enabled, 2);

    add_drop_down_options(ui_droRumble_tweakadv, (char *[]) {
            lang.GENERIC.DISABLED,
            lang.MUXTWEAKADV.RUMBLE.ST,
            lang.MUXTWEAKADV.RUMBLE.SH,
            lang.MUXTWEAKADV.RUMBLE.SL,
            lang.MUXTWEAKADV.RUMBLE.STSH,
            lang.MUXTWEAKADV.RUMBLE.STSL,
            lang.MUXTWEAKADV.RUMBLE.SHSL}, 7);

    add_drop_down_options(ui_droUserInit_tweakadv, disabled_enabled, 2);
    add_drop_down_options(ui_droDPADSwap_tweakadv, disabled_enabled, 2);
    add_drop_down_options(ui_droOverdrive_tweakadv, disabled_enabled, 2);
    add_drop_down_options(ui_droCardMode_tweakadv, (char *[]) {"deadline", "noop"}, 2);

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

static void list_nav_move(int steps, int direction) {
    play_sound(SND_NAVIGATE, nav_sound, 0);

    for (int step = 0; step < steps; ++step) {
        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_value, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    play_sound(SND_NAVIGATE, nav_sound, 0);
    decrease_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    play_sound(SND_NAVIGATE, nav_sound, 0);
    increase_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_confirm(void) {
    if (msgbox_active) return;

    handle_option_next();
}

static void handle_back(void) {
    if (msgbox_active) {
        play_sound(SND_CONFIRM, nav_sound, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    play_sound(SND_BACK, nav_sound, 0);

    save_tweak_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "advanced");

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM, nav_sound, 0);
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

    lv_label_set_text(ui_lblNavB, lang.GENERIC.SAVE);

    lv_obj_t *nav_hide[] = {
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblAccelerate_tweakadv, "accelerate");
    lv_obj_set_user_data(ui_lblSwap_tweakadv, "swap");
    lv_obj_set_user_data(ui_lblThermal_tweakadv, "thermal");
    lv_obj_set_user_data(ui_lblVolume_tweakadv, "volume");
    lv_obj_set_user_data(ui_lblBrightness_tweakadv, "brightness");
    lv_obj_set_user_data(ui_lblOffset_tweakadv, "offset");
    lv_obj_set_user_data(ui_lblPasscode_tweakadv, "lock");
    lv_obj_set_user_data(ui_lblLED_tweakadv, "led");
    lv_obj_set_user_data(ui_lblTheme_tweakadv, "theme");
    lv_obj_set_user_data(ui_lblRetroWait_tweakadv, "retrowait");
    lv_obj_set_user_data(ui_lblState_tweakadv, "state");
    lv_obj_set_user_data(ui_lblVerbose_tweakadv, "verbose");
    lv_obj_set_user_data(ui_lblRumble_tweakadv, "rumble");
    lv_obj_set_user_data(ui_lblUserInit_tweakadv, "userinit");
    lv_obj_set_user_data(ui_lblDPADSwap_tweakadv, "dpadswap");
    lv_obj_set_user_data(ui_lblOverdrive_tweakadv, "overdrive");
    lv_obj_set_user_data(ui_lblSwap_tweakadvfile, "swapfile");
    lv_obj_set_user_data(ui_lblZramfile_tweakadv, "zramfile");
    lv_obj_set_user_data(ui_lblCardMode_tweakadv, "cardmode");

    if (!device.DEVICE.HAS_NETWORK) {
        lv_obj_add_flag(ui_pnlRetroWait_tweakadv, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlRetroWait_tweakadv, LV_OBJ_FLAG_FLOATING);
        ui_count--;
    }

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

int muxtweakadv_main() {
    init_module("muxtweakadv");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXTWEAKADV.TITLE);
    init_muxtweakadv(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    init_element_events();

    restore_tweak_options();
    init_dropdown_settings();

    load_kiosk(&kiosk);
    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);

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
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
