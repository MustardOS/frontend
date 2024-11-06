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
lv_obj_t *overlay_image = NULL;

int progress_onscreen = -1;

int accelerate_original, swap_original, thermal_original, font_original, volume_original, brightness_original;
int offset_original, lockdown_original, led_original, random_theme_original, retrowait_original;
int usbfunction_original, state_original, verbose_original, rumble_original, hdmi_output_original;

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

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblAccelerate,  TS("Adjust the rate of speed when holding navigation keys down")},
            {ui_lblSwap,        TS("Change how the device buttons work globally")},
            {ui_lblThermal,     TS("Toggle the system ability to automatically shut the device down due high temperature")},
            {ui_lblFont,        TS("Change how the font type works in the frontend - 'Theme' will ensure frontend will "
                                   "use fonts within themes with a fallback to language fonts - 'Language' will "
                                   "specifically use language based font")},
            {ui_lblVolume,      TS("Change the default audio level that the device will use each time it starts up")},
            {ui_lblBrightness,  TS("Change the default brightness level that the device will use each time it starts up")},
            {ui_lblOffset,      TS("Change the displayed battery percentage to improve accuracy based on calibration "
                                   "or known deviations in the battery capacity reading")},
            {ui_lblPasscode,    TS("Toggle the passcode lock - More information can be found on the muOS website")},
            {ui_lblLED,         TS("Toggle the power LED during content launch")},
            {ui_lblTheme,       TS("Change the default theme used for the next device launch")},
            {ui_lblRetroWait,   TS("Toggle a delayed start of RetroArch until a network connection is established")},
            {ui_lblUSBFunction, TS("Toggle between ADB and MTP USB functionality")},
            {ui_lblState,       TS("Switch between system sleep suspend states")},
            {ui_lblVerbose,     TS("Toggle startup and shutdown verbose messages used for debugging faults")},
            {ui_lblRumble,      TS("Toggle vibration for device startup, sleep, and shutdown")},
            {ui_lblHDMIOutput,  TS("Switch between device speaker or external monitor audio via HDMI connection")},
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
            ui_droHDMIOutput
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
    hdmi_output_original = lv_dropdown_get_selected(ui_droHDMIOutput);
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
    if (strcasecmp(volume_type, "previous") == 0) {
        volume_index = 0;
    } else if (strcasecmp(volume_type, "quiet") == 0) {
        volume_index = 1;
    } else if (strcasecmp(volume_type, "loud") == 0) {
        volume_index = 2;
    }
    lv_dropdown_set_selected(ui_droVolume, volume_index);

    const char *brightness_type = config.SETTINGS.ADVANCED.BRIGHTNESS;
    int brightness_index = 0;
    if (strcasecmp(brightness_type, "previous") == 0) {
        brightness_index = 0;
    } else if (strcasecmp(brightness_type, "low") == 0) {
        brightness_index = 1;
    } else if (strcasecmp(brightness_type, "high") == 0) {
        brightness_index = 2;
    }
    lv_dropdown_set_selected(ui_droBrightness, brightness_index);

    lv_dropdown_set_selected(ui_droOffset, config.SETTINGS.ADVANCED.OFFSET);
    lv_dropdown_set_selected(ui_droPasscode, config.SETTINGS.ADVANCED.LOCK);
    lv_dropdown_set_selected(ui_droLED, config.SETTINGS.ADVANCED.LED);
    lv_dropdown_set_selected(ui_droTheme, config.SETTINGS.ADVANCED.THEME);
    lv_dropdown_set_selected(ui_droRetroWait, config.SETTINGS.ADVANCED.RETROWAIT);

    const char *usb_type = config.SETTINGS.ADVANCED.USBFUNCTION;
    if (strcasecmp(usb_type, "adb") == 0) {
        lv_dropdown_set_selected(ui_droUSBFunction, 1);
    } else if (strcasecmp(usb_type, "mtp") == 0) {
        lv_dropdown_set_selected(ui_droUSBFunction, 2);
    } else {
        lv_dropdown_set_selected(ui_droUSBFunction, 0);
    }

    const char *state_type = config.SETTINGS.ADVANCED.STATE;
    if (strcasecmp(state_type, "freeze") == 0) {
        lv_dropdown_set_selected(ui_droState, 1);
    } else {
        lv_dropdown_set_selected(ui_droState, 0);
    }

    lv_dropdown_set_selected(ui_droVerbose, config.SETTINGS.ADVANCED.VERBOSE);
    lv_dropdown_set_selected(ui_droRumble, config.SETTINGS.ADVANCED.RUMBLE);
    lv_dropdown_set_selected(ui_droHDMIOutput, config.SETTINGS.ADVANCED.HDMIOUTPUT);
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
    int idx_hdmi_output = lv_dropdown_get_selected(ui_droHDMIOutput);

    int is_modified = 0;

    if (lv_dropdown_get_selected(ui_droAccelerate) != accelerate_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/advanced/accelerate", "w", INT, idx_accelerate);
    }

    if (lv_dropdown_get_selected(ui_droSwap) != swap_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/advanced/swap", "w", INT, idx_swap);
    }

    if (lv_dropdown_get_selected(ui_droThermal) != thermal_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/advanced/thermal", "w", INT, idx_thermal);
    }

    if (lv_dropdown_get_selected(ui_droFont) != font_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/advanced/font", "w", INT, idx_font);
    }

    if (lv_dropdown_get_selected(ui_droVolume) != volume_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/advanced/volume", "w", CHAR, idx_volume);
    }

    if (lv_dropdown_get_selected(ui_droBrightness) != brightness_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/advanced/brightness", "w", CHAR, idx_brightness);
    }

    if (lv_dropdown_get_selected(ui_droOffset) != offset_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/advanced/offset", "w", INT, idx_offset);
    }

    if (lv_dropdown_get_selected(ui_droPasscode) != lockdown_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/advanced/lock", "w", INT, idx_lockdown);
    }

    if (lv_dropdown_get_selected(ui_droLED) != led_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/advanced/led", "w", INT, idx_led);
    }

    if (lv_dropdown_get_selected(ui_droTheme) != random_theme_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/advanced/random_theme", "w", INT, idx_random_theme);
    }

    if (lv_dropdown_get_selected(ui_droRetroWait) != retrowait_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/advanced/retrowait", "w", INT, idx_retrowait);
    }

    if (lv_dropdown_get_selected(ui_droUSBFunction) != usbfunction_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/advanced/usb_function", "w", CHAR, idx_usbfunction);
    }

    if (lv_dropdown_get_selected(ui_droState) != state_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/advanced/state", "w", CHAR, idx_state);
    }

    if (lv_dropdown_get_selected(ui_droVerbose) != verbose_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/advanced/verbose", "w", INT, idx_verbose);
    }

    if (lv_dropdown_get_selected(ui_droRumble) != rumble_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/advanced/rumble", "w", INT, idx_rumble);
    }

    if (lv_dropdown_get_selected(ui_droHDMIOutput) != hdmi_output_original) {
        is_modified++;
        write_text_to_file("/run/muos/global/settings/advanced/hdmi_output", "w", INT, idx_hdmi_output);
    }

    if (is_modified > 0) {
        static char tweak_script[MAX_BUFFER_SIZE];
        snprintf(tweak_script, sizeof(tweak_script),
                 "%s/script/mux/tweak.sh", INTERNAL_PATH);
        system(tweak_script);
    }
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
            ui_pnlHDMIOutput
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
    ui_objects[15] = ui_lblHDMIOutput;

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
            ui_droHDMIOutput
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
            ui_icoHDMIOutput
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
    apply_theme_list_panel(&theme, &device, ui_pnlHDMIOutput);

    apply_theme_list_item(&theme, ui_lblAccelerate, TS("Menu Acceleration"), false, true);
    apply_theme_list_item(&theme, ui_lblSwap, TS("Button Swap"), false, true);
    apply_theme_list_item(&theme, ui_lblThermal, TS("Thermal Zone Control"), false, true);
    apply_theme_list_item(&theme, ui_lblFont, TS("Interface Font Type"), false, true);
    apply_theme_list_item(&theme, ui_lblVolume, TS("Volume On Boot"), false, true);
    apply_theme_list_item(&theme, ui_lblBrightness, TS("Brightness On Boot"), false, true);
    apply_theme_list_item(&theme, ui_lblOffset, TS("Battery Offset"), false, true);
    apply_theme_list_item(&theme, ui_lblPasscode, TS("Passcode Lock"), false, true);
    apply_theme_list_item(&theme, ui_lblLED, TS("LED During Play"), false, true);
    apply_theme_list_item(&theme, ui_lblTheme, TS("Random Theme on Boot"), false, true);
    apply_theme_list_item(&theme, ui_lblRetroWait, TS("RetroArch Network Wait"), false, true);
    apply_theme_list_item(&theme, ui_lblUSBFunction, TS("USB Function"), false, true);
    apply_theme_list_item(&theme, ui_lblState, TS("Suspend Power State"), false, true);
    apply_theme_list_item(&theme, ui_lblVerbose, TS("Verbose Messages"), false, true);
    apply_theme_list_item(&theme, ui_lblRumble, TS("Device Rumble"), false, true);
    apply_theme_list_item(&theme, ui_lblHDMIOutput, TS("HDMI Audio Output"), false, true);

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
    apply_theme_list_glyph(&theme, ui_icoHDMIOutput, mux_module, "hdmi");

    char *accelerate_string = generate_number_string(16, 256, 16, TG("Disabled"), NULL, NULL, 0);
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
    apply_theme_list_drop_down(&theme, ui_droHDMIOutput, NULL);

    char *disabled_enabled[] = {TG("Disabled"), TG("Enabled")};
    add_drop_down_options(ui_droSwap, (char *[]) {TS("Retro"), TS("Modern")}, 2);
    add_drop_down_options(ui_droThermal, disabled_enabled, 2);
    add_drop_down_options(ui_droFont, (char *[]) {TS("Language"), TS("Theme")}, 2);
    add_drop_down_options(ui_droVolume, (char *[]) {TG("Previous"), TS("Quiet"), TS("Loud")}, 3);
    add_drop_down_options(ui_droBrightness, (char *[]) {TG("Previous"), TS("Low"), TS("High")}, 3);
    add_drop_down_options(ui_droPasscode, disabled_enabled, 2);
    add_drop_down_options(ui_droLED, disabled_enabled, 2);
    add_drop_down_options(ui_droTheme, disabled_enabled, 2);
    add_drop_down_options(ui_droRetroWait, disabled_enabled, 2);
    add_drop_down_options(ui_droUSBFunction, (char *[]) {TG("Disabled"), "ADB", "MTP"}, 3);
    add_drop_down_options(ui_droState, (char *[]) {"mem", "freeze"}, 2);
    add_drop_down_options(ui_droVerbose, disabled_enabled, 2);
    add_drop_down_options(ui_droRumble, (char *[]) {TG("Disabled"), TS("Startup"), TS("Shutdown"), TS("Sleep"),
                                                    TS("Startup + Shutdown"), TS("Startup + Sleep"),
                                                    TS("Shutdown + Sleep")}, 7);
    add_drop_down_options(ui_droHDMIOutput, (char *[]) {TG("External"), TG("Internal")}, 2);

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

void handle_option_prev(void) {
    if (msgbox_active) {
        return;
    }

    play_sound("navigate", nav_sound, 0);
    decrease_option_value(lv_group_get_focused(ui_group_value));
}

void handle_option_next(void) {
    if (msgbox_active) {
        return;
    }

    play_sound("navigate", nav_sound, 0);
    increase_option_value(lv_group_get_focused(ui_group_value));
}

void handle_confirm(void) {
    if (msgbox_active) {
        return;
    }

    handle_option_next();
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

    osd_message = TG("Saving Changes");
    lv_label_set_text(ui_lblMessage, osd_message);
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);

    save_tweak_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "advanced");
    mux_input_stop();
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

    lv_label_set_text(ui_lblNavB, TG("Save"));

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
    lv_obj_set_user_data(ui_lblHDMIOutput, "hdmi");

    if (!device.DEVICE.HAS_NETWORK) {
        lv_obj_add_flag(ui_pnlRetroWait, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlRetroWait, LV_OBJ_FLAG_FLOATING);
        ui_count--;
    }

    if (!device.DEVICE.HAS_HDMI) {
        lv_obj_add_flag(ui_pnlHDMIOutput, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlHDMIOutput, LV_OBJ_FLAG_FLOATING);
        ui_count--;
    }

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
            list_nav_next(text_hit - !device.DEVICE.HAS_NETWORK);
            nav_moved = 1;
        }
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

    ui_common_screen_init(&theme, &device, TS("ADVANCED SETTINGS"));
    ui_init(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, basename(argv[0]));

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

    direct_to_previous();

    refresh_screen(device.SCREEN.WAIT);
    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = 16 /* ~60 FPS */,
            .swap_btn = config.SETTINGS.ADVANCED.SWAP,
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .stick_nav = true,
            .press_handler = {
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    // List navigation:
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
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
