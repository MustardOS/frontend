#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <libgen.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "../common/img/nothing.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"

__thread uint64_t start_ms = 0;

char *mux_prog;
static int js_fd;
static int js_fd_sys;

int NAV_DPAD_HOR;
int NAV_ANLG_HOR;
int NAV_DPAD_VER;
int NAV_ANLG_VER;
int NAV_A;
int NAV_B;

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
char *current_wall = "";
int current_item_index = 0;
int ui_count = 0;

lv_obj_t *msgbox_element = NULL;

int progress_onscreen = -1;

int accelerate_total, accelerate_current;
int swap_total, swap_current;
int thermal_total, thermal_current;
int font_total, font_current;
int volume_total, volume_current;
int brightness_total, brightness_current;
int offset_total, offset_current;
int lockdown_total, lockdown_current;
int led_total, led_current;
int random_theme_total, random_theme_current;
int retrowait_total, retrowait_current;
int usbfunction_total, usbfunction_current;
int state_total, state_current;
int verbose_total, verbose_current;
int hdmi_output_total, hdmi_output_current;

typedef struct {
    int *total;
    int *current;
} Tweak;

Tweak accelerate, swap, thermal, font, volume, brightness, offset, lockdown,
        led, random_theme, retrowait, usbfunction, state, verbose, hdmi_output;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

#define UI_COUNT 16
lv_obj_t *ui_objects[UI_COUNT];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblAccelerate,  "HELP.ACCELERATE"},
            {ui_lblSwap,        "HELP.SWAP"},
            {ui_lblThermal,     "HELP.THERMAL"},
            {ui_lblFont,        "HELP.FONT"},
            {ui_lblVolume,      "HELP.VOLUME"},
            {ui_lblBrightness,  "HELP.BRIGHTNESS"},
            {ui_lblOffset,      "HELP.OFFSET"},
            {ui_lblPasscode,    "HELP.PASSCODE"},
            {ui_lblLED,         "HELP.LED"},
            {ui_lblTheme,       "HELP.THEME"},
            {ui_lblRetroWait,   "HELP.RETROWAIT"},
            {ui_lblUSBFunction, "HELP.USBFUNCTION"},
            {ui_lblState,       "HELP.STATE"},
            {ui_lblVerbose,     "HELP.VERBOSE"},
            {ui_lblHDMIOutput,  "HELP.HDMI_OUTPUT"},
            {ui_lblStorage,     "HELP.STORAGE"},
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
                     TS(lv_label_get_text(element_focused)), TS(message));
}

void init_pointers(Tweak *tweak, int *total, int *current) {
    tweak->total = total;
    tweak->current = current;
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
            ui_droHDMIOutput,
            ui_droStorage
    };

    for (unsigned int i = 0; i < sizeof(dropdowns) / sizeof(dropdowns[0]); i++) {
        lv_obj_add_event_cb(dropdowns[i], dropdown_event_handler, LV_EVENT_ALL, NULL);
    }

    init_pointers(&accelerate, &accelerate_total, &accelerate_current);
    init_pointers(&swap, &swap_total, &swap_current);
    init_pointers(&thermal, &thermal_total, &thermal_current);
    init_pointers(&font, &font_total, &font_current);
    init_pointers(&volume, &volume_total, &volume_current);
    init_pointers(&brightness, &brightness_total, &brightness_current);
    init_pointers(&offset, &offset_total, &offset_current);
    init_pointers(&lockdown, &lockdown_total, &lockdown_current);
    init_pointers(&led, &led_total, &led_current);
    init_pointers(&random_theme, &random_theme_total, &random_theme_current);
    init_pointers(&retrowait, &retrowait_total, &retrowait_current);
    init_pointers(&usbfunction, &usbfunction_total, &usbfunction_current);
    init_pointers(&state, &state_total, &state_current);
    init_pointers(&verbose, &verbose_total, &verbose_current);
    init_pointers(&hdmi_output, &hdmi_output_total, &hdmi_output_current);
}

void init_dropdown_settings() {
    Tweak settings[] = {
            {accelerate.total,   accelerate.current},
            {swap.total,         swap.current},
            {thermal.total,      thermal.current},
            {font.total,         font.current},
            {volume.total,       volume.current},
            {brightness.total,   brightness.current},
            {offset.total,       offset.current},
            {lockdown.total,     lockdown.current},
            {led.total,          led.current},
            {random_theme.total, random_theme.current},
            {retrowait.total,    retrowait.current},
            {usbfunction.total,  usbfunction.current},
            {state.total,        state.current},
            {verbose.total,      verbose.current},
            {hdmi_output.total,  hdmi_output.current},
    };

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
            ui_droHDMIOutput
    };

    for (unsigned int i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
        *(settings[i].total) = lv_dropdown_get_option_cnt(dropdowns[i]);
        *(settings[i].current) = lv_dropdown_get_selected(dropdowns[i]);
    }
}

void restore_tweak_options() {
    switch (config.SETTINGS.ADVANCED.ACCELERATE) {
        case 32767:
            lv_dropdown_set_selected(ui_droAccelerate, 0);
            break;
        case 16:
            lv_dropdown_set_selected(ui_droAccelerate, 1);
            break;
        case 32:
            lv_dropdown_set_selected(ui_droAccelerate, 2);
            break;
        case 48:
            lv_dropdown_set_selected(ui_droAccelerate, 3);
            break;
        case 64:
            lv_dropdown_set_selected(ui_droAccelerate, 4);
            break;
        case 80:
            lv_dropdown_set_selected(ui_droAccelerate, 5);
            break;
        case 96:
            lv_dropdown_set_selected(ui_droAccelerate, 6);
            break;
        case 112:
            lv_dropdown_set_selected(ui_droAccelerate, 7);
            break;
        case 128:
            lv_dropdown_set_selected(ui_droAccelerate, 8);
            break;
        case 144:
            lv_dropdown_set_selected(ui_droAccelerate, 9);
            break;
        case 160:
            lv_dropdown_set_selected(ui_droAccelerate, 10);
            break;
        case 176:
            lv_dropdown_set_selected(ui_droAccelerate, 11);
            break;
        case 192:
            lv_dropdown_set_selected(ui_droAccelerate, 12);
            break;
        case 208:
            lv_dropdown_set_selected(ui_droAccelerate, 13);
            break;
        case 224:
            lv_dropdown_set_selected(ui_droAccelerate, 14);
            break;
        case 240:
            lv_dropdown_set_selected(ui_droAccelerate, 15);
            break;
        case 256:
            lv_dropdown_set_selected(ui_droAccelerate, 16);
            break;
        default:
            lv_dropdown_set_selected(ui_droAccelerate, 6);
            break;
    }

    lv_dropdown_set_selected(ui_droSwap, config.SETTINGS.ADVANCED.SWAP);
    lv_dropdown_set_selected(ui_droThermal, config.SETTINGS.ADVANCED.THERMAL);
    lv_dropdown_set_selected(ui_droFont, config.SETTINGS.ADVANCED.FONT);

    const char *volume_type = config.SETTINGS.ADVANCED.VOLUME;
    if (strcasecmp(volume_type, "previous") == 0) {
        lv_dropdown_set_selected(ui_droVolume, 0);
    } else if (strcasecmp(volume_type, "quiet") == 0) {
        lv_dropdown_set_selected(ui_droVolume, 1);
    } else if (strcasecmp(volume_type, "loud") == 0) {
        lv_dropdown_set_selected(ui_droVolume, 2);
    } else {
        lv_dropdown_set_selected(ui_droVolume, 0);
    }

    const char *brightness_type = config.SETTINGS.ADVANCED.BRIGHTNESS;
    if (strcasecmp(brightness_type, "previous") == 0) {
        lv_dropdown_set_selected(ui_droBrightness, 0);
    } else if (strcasecmp(brightness_type, "low") == 0) {
        lv_dropdown_set_selected(ui_droBrightness, 1);
    } else if (strcasecmp(brightness_type, "high") == 0) {
        lv_dropdown_set_selected(ui_droBrightness, 2);
    } else {
        lv_dropdown_set_selected(ui_droBrightness, 0);
    }

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
    lv_dropdown_set_selected(ui_droHDMIOutput, config.SETTINGS.ADVANCED.HDMIOUTPUT);
}

void save_tweak_options() {
    int idx_accelerate;
    switch (lv_dropdown_get_selected(ui_droAccelerate)) {
        case 0:
            idx_accelerate = 32767;
            break;
        case 1:
            idx_accelerate = 16;
            break;
        case 2:
            idx_accelerate = 32;
            break;
        case 3:
            idx_accelerate = 48;
            break;
        case 4:
            idx_accelerate = 64;
            break;
        case 5:
            idx_accelerate = 80;
            break;
        case 6:
            idx_accelerate = 96;
            break;
        case 7:
            idx_accelerate = 112;
            break;
        case 8:
            idx_accelerate = 128;
            break;
        case 9:
            idx_accelerate = 144;
            break;
        case 10:
            idx_accelerate = 160;
            break;
        case 11:
            idx_accelerate = 176;
            break;
        case 12:
            idx_accelerate = 192;
            break;
        case 13:
            idx_accelerate = 208;
            break;
        case 14:
            idx_accelerate = 224;
            break;
        case 15:
            idx_accelerate = 240;
            break;
        case 16:
            idx_accelerate = 256;
            break;
        default:
            idx_accelerate = 96;
            break;
    }

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
    int idx_hdmi_output = lv_dropdown_get_selected(ui_droHDMIOutput);

    write_text_to_file("/run/muos/global/settings/advanced/accelerate", "w", INT, idx_accelerate);
    write_text_to_file("/run/muos/global/settings/advanced/swap", "w", INT, idx_swap);
    write_text_to_file("/run/muos/global/settings/advanced/thermal", "w", INT, idx_thermal);
    write_text_to_file("/run/muos/global/settings/advanced/font", "w", INT, idx_font);
    write_text_to_file("/run/muos/global/settings/advanced/volume", "w", CHAR, idx_volume);
    write_text_to_file("/run/muos/global/settings/advanced/brightness", "w", CHAR, idx_brightness);
    write_text_to_file("/run/muos/global/settings/advanced/offset", "w", INT, idx_offset);
    write_text_to_file("/run/muos/global/settings/advanced/lock", "w", INT, idx_lockdown);
    write_text_to_file("/run/muos/global/settings/advanced/led", "w", INT, idx_led);
    write_text_to_file("/run/muos/global/settings/advanced/random_theme", "w", INT, idx_random_theme);
    write_text_to_file("/run/muos/global/settings/advanced/retrowait", "w", INT, idx_retrowait);
    write_text_to_file("/run/muos/global/settings/advanced/usb_function", "w", CHAR, idx_usbfunction);
    write_text_to_file("/run/muos/global/settings/advanced/state", "w", CHAR, idx_state);
    write_text_to_file("/run/muos/global/settings/advanced/verbose", "w", INT, idx_verbose);
    write_text_to_file("/run/muos/global/settings/advanced/hdmi_output", "w", INT, idx_hdmi_output);

    static char tweak_script[MAX_BUFFER_SIZE];
    snprintf(tweak_script, sizeof(tweak_script),
             "%s/script/mux/tweak.sh", INTERNAL_PATH);
    system(tweak_script);
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
            ui_pnlHDMIOutput,
            ui_pnlStorage
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
    ui_objects[14] = ui_lblHDMIOutput;
    ui_objects[15] = ui_lblStorage;

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
            ui_droHDMIOutput,
            ui_droStorage
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
            ui_icoHDMIOutput,
            ui_icoStorage
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
    apply_theme_list_panel(&theme, &device, ui_pnlHDMIOutput);
    apply_theme_list_panel(&theme, &device, ui_pnlStorage);

    apply_theme_list_item(&theme, ui_lblAccelerate, TS("Menu Acceleration"), false, true);
    apply_theme_list_item(&theme, ui_lblSwap, TS("A+B Button Swap"), false, true);
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
    apply_theme_list_item(&theme, ui_lblHDMIOutput, TS("HDMI Audio Output"), false, true);
    apply_theme_list_item(&theme, ui_lblStorage, TS("Storage Preference"), false, true);

    apply_theme_list_glyph(&theme, ui_icoAccelerate, mux_prog, "accelerate");
    apply_theme_list_glyph(&theme, ui_icoSwap, mux_prog, "swap");
    apply_theme_list_glyph(&theme, ui_icoThermal, mux_prog, "thermal");
    apply_theme_list_glyph(&theme, ui_icoFont, mux_prog, "font");
    apply_theme_list_glyph(&theme, ui_icoVolume, mux_prog, "volume");
    apply_theme_list_glyph(&theme, ui_icoBrightness, mux_prog, "brightness");
    apply_theme_list_glyph(&theme, ui_icoOffset, mux_prog, "offset");
    apply_theme_list_glyph(&theme, ui_icoPasscode, mux_prog, "lock");
    apply_theme_list_glyph(&theme, ui_icoLED, mux_prog, "led");
    apply_theme_list_glyph(&theme, ui_icoTheme, mux_prog, "theme");
    apply_theme_list_glyph(&theme, ui_icoRetroWait, mux_prog, "retrowait");
    apply_theme_list_glyph(&theme, ui_icoUSBFunction, mux_prog, "usbfunction");
    apply_theme_list_glyph(&theme, ui_icoState, mux_prog, "state");
    apply_theme_list_glyph(&theme, ui_icoVerbose, mux_prog, "verbose");
    apply_theme_list_glyph(&theme, ui_icoHDMIOutput, mux_prog, "hdmi");
    apply_theme_list_glyph(&theme, ui_icoStorage, mux_prog, "storage");

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
    apply_theme_list_drop_down(&theme, ui_droHDMIOutput, NULL);
    apply_theme_list_drop_down(&theme, ui_droStorage, "");

    char *disabled_enabled[] = {TG("Disabled"), TG("Enabled")};
    add_drop_down_options(ui_droSwap, disabled_enabled, 2);
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
        if (current_item_index > 0) {
            current_item_index--;
            nav_prev(ui_group, 1);
            nav_prev(ui_group_value, 1);
            nav_prev(ui_group_glyph, 1);
            nav_prev(ui_group_panel, 1);
        }
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    play_sound("navigate", nav_sound, 0);
    for (int step = 0; step < steps; ++step) {
        if (current_item_index < (ui_count)) {
            current_item_index++;
            nav_next(ui_group, 1);
            nav_next(ui_group_value, 1);
            nav_next(ui_group_glyph, 1);
            nav_next(ui_group_panel, 1);
        }
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void joystick_task() {
    struct input_event ev;
    int epoll_fd;
    struct epoll_event event, events[device.DEVICE.EVENT];

    int JOYUP_pressed = 0;
    int JOYDOWN_pressed = 0;
    int JOYHOTKEY_pressed = 0;
    int JOYHOTKEY_screenshot = 0;

    int nav_hold = 0;

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Error creating EPOLL instance");
        return;
    }

    event.events = EPOLLIN;
    event.data.fd = js_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, js_fd, &event) == -1) {
        perror("Error with EPOLL controller");
        return;
    }

    event.data.fd = js_fd_sys;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, js_fd_sys, &event) == -1) {
        perror("Error with EPOLL controller");
        return;
    }

    while (1) {
        int num_events = epoll_wait(epoll_fd, events, device.DEVICE.EVENT, config.SETTINGS.ADVANCED.ACCELERATE);
        if (num_events == -1) {
            perror("Error with EPOLL wait event timer");
            continue;
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == js_fd_sys) {
                ssize_t ret = read(js_fd_sys, &ev, sizeof(struct input_event));
                if (ret == -1) {
                    perror("Error reading input");
                    continue;
                }
                if (JOYHOTKEY_pressed == 1 && ev.type == EV_KEY && ev.value == 1 &&
                    (ev.code == device.RAW_INPUT.BUTTON.POWER_SHORT || ev.code == device.RAW_INPUT.BUTTON.POWER_LONG)) {
                    JOYHOTKEY_screenshot = 1;
                }
            } else if (events[i].data.fd == js_fd) {
                ssize_t ret = read(js_fd, &ev, sizeof(struct input_event));
                if (ret == -1) {
                    perror("Error reading input");
                    continue;
                }

                struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
                switch (ev.type) {
                    case EV_KEY:
                        if (ev.value == 1) {
                            if (msgbox_active) {
                                if (ev.code == NAV_B) {
                                    play_sound("confirm", nav_sound, 1);
                                    msgbox_active = 0;
                                    progress_onscreen = 0;
                                    lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
                                }
                            } else {
                                if (ev.code == device.RAW_INPUT.BUTTON.MENU_LONG) {
                                    JOYHOTKEY_pressed = 1;
                                    JOYHOTKEY_screenshot = 0;
                                } else if (ev.code == NAV_A || ev.code == device.RAW_INPUT.ANALOG.LEFT.CLICK) {
                                    play_sound("confirm", nav_sound, 1);
                                    if (element_focused == ui_lblAccelerate) {
                                        increase_option_value(ui_droAccelerate,
                                                              &accelerate_current,
                                                              accelerate_total);
                                    } else if (element_focused == ui_lblSwap) {
                                        increase_option_value(ui_droSwap,
                                                              &swap_current,
                                                              swap_total);
                                    } else if (element_focused == ui_lblThermal) {
                                        increase_option_value(ui_droThermal,
                                                              &thermal_current,
                                                              thermal_total);
                                    } else if (element_focused == ui_lblFont) {
                                        increase_option_value(ui_droFont,
                                                              &font_current,
                                                              font_total);
                                    } else if (element_focused == ui_lblVolume) {
                                        increase_option_value(ui_droVolume,
                                                              &volume_current,
                                                              volume_total);
                                    } else if (element_focused == ui_lblBrightness) {
                                        increase_option_value(ui_droBrightness,
                                                              &brightness_current,
                                                              brightness_total);
                                    } else if (element_focused == ui_lblOffset) {
                                        increase_option_value(ui_droOffset,
                                                              &offset_current,
                                                              offset_total);
                                    } else if (element_focused == ui_lblPasscode) {
                                        increase_option_value(ui_droPasscode,
                                                              &lockdown_current,
                                                              lockdown_total);
                                    } else if (element_focused == ui_lblLED) {
                                        increase_option_value(ui_droLED,
                                                              &led_current,
                                                              led_total);
                                    } else if (element_focused == ui_lblTheme) {
                                        increase_option_value(ui_droTheme,
                                                              &random_theme_current,
                                                              random_theme_total);
                                    } else if (element_focused == ui_lblRetroWait) {
                                        increase_option_value(ui_droRetroWait,
                                                              &retrowait_current,
                                                              retrowait_total);
                                    } else if (element_focused == ui_lblUSBFunction) {
                                        increase_option_value(ui_droUSBFunction,
                                                              &usbfunction_current,
                                                              usbfunction_total);
                                    } else if (element_focused == ui_lblState) {
                                        increase_option_value(ui_droState,
                                                              &state_current,
                                                              state_total);
                                    } else if (element_focused == ui_lblVerbose) {
                                        increase_option_value(ui_droVerbose,
                                                              &verbose_current,
                                                              verbose_total);
                                    } else if (element_focused == ui_lblHDMIOutput) {
                                        increase_option_value(ui_droHDMIOutput,
                                                              &hdmi_output_current,
                                                              hdmi_output_total);
                                    } else if (element_focused == ui_lblStorage) {
                                        save_tweak_options();

                                        load_mux("storage");
                                        return;
                                    }
                                } else if (ev.code == NAV_B) {
                                    play_sound("back", nav_sound, 1);
                                    input_disable = 1;

                                    osd_message = TG("Saving Changes");
                                    lv_label_set_text(ui_lblMessage, osd_message);
                                    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);

                                    save_tweak_options();

                                    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "advanced");
                                    return;
                                }
                            }
                        } else {
                            if ((ev.code == device.RAW_INPUT.BUTTON.MENU_SHORT ||
                                 ev.code == device.RAW_INPUT.BUTTON.MENU_LONG) && !JOYHOTKEY_screenshot) {
                                JOYHOTKEY_pressed = 0;
                                if (progress_onscreen == -1) {
                                    play_sound("confirm", nav_sound, 1);
                                    show_help(element_focused);
                                }
                            }
                        }
                        break;
                    case EV_ABS:
                        if (msgbox_active) {
                            break;
                        }
                        if (ev.code == ABS_Y) {
                            JOYUP_pressed = 0;
                            JOYDOWN_pressed = 0;
                            nav_hold = 0;
                            break;
                        }
                        if (ev.code == NAV_DPAD_VER || ev.code == NAV_ANLG_VER) {
                            if (ev.value == -device.INPUT.AXIS || ev.value == -1) {
                                if (current_item_index == 0) {
                                    current_item_index = ui_count - 1;
                                    nav_prev(ui_group, 1);
                                    nav_prev(ui_group_value, 1);
                                    nav_prev(ui_group_glyph, 1);
                                    nav_prev(ui_group_panel, 1);
                                    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL,
                                                           ui_count, current_item_index, ui_pnlContent);
                                    nav_moved = 1;
                                } else if (current_item_index > 0) {
                                    JOYUP_pressed = (ev.value != 0);
                                    list_nav_prev(1);
                                    nav_moved = 1;
                                }
                            } else if (ev.value == device.INPUT.AXIS || ev.value == 1) {
                                if (current_item_index == ui_count - 1) {
                                    current_item_index = 0;
                                    nav_next(ui_group, 1);
                                    nav_next(ui_group_value, 1);
                                    nav_next(ui_group_glyph, 1);
                                    nav_next(ui_group_panel, 1);
                                    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL,
                                                           ui_count, current_item_index, ui_pnlContent);
                                    nav_moved = 1;
                                } else if (current_item_index < ui_count - 1) {
                                    JOYDOWN_pressed = (ev.value != 0);
                                    list_nav_next(1);
                                    nav_moved = 1;
                                }
                            } else {
                                JOYUP_pressed = 0;
                                JOYDOWN_pressed = 0;
                            }
                        } else if (ev.code == NAV_DPAD_HOR || ev.code == NAV_ANLG_HOR) {
                            if (ev.value == -device.INPUT.AXIS || ev.value == -1) {
                                play_sound("navigate", nav_sound, 0);
                                if (element_focused == ui_lblAccelerate) {
                                    decrease_option_value(ui_droAccelerate,
                                                          &accelerate_current,
                                                          accelerate_total);
                                } else if (element_focused == ui_lblSwap) {
                                    decrease_option_value(ui_droSwap,
                                                          &swap_current,
                                                          swap_total);
                                } else if (element_focused == ui_lblThermal) {
                                    decrease_option_value(ui_droThermal,
                                                          &thermal_current,
                                                          thermal_total);
                                } else if (element_focused == ui_lblFont) {
                                    decrease_option_value(ui_droFont,
                                                          &font_current,
                                                          font_total);
                                } else if (element_focused == ui_lblVolume) {
                                    decrease_option_value(ui_droVolume,
                                                          &volume_current,
                                                          volume_total);
                                } else if (element_focused == ui_lblBrightness) {
                                    decrease_option_value(ui_droBrightness,
                                                          &brightness_current,
                                                          brightness_total);
                                } else if (element_focused == ui_lblOffset) {
                                    decrease_option_value(ui_droOffset,
                                                          &offset_current,
                                                          offset_total);
                                } else if (element_focused == ui_lblPasscode) {
                                    decrease_option_value(ui_droPasscode,
                                                          &lockdown_current,
                                                          lockdown_total);
                                } else if (element_focused == ui_lblLED) {
                                    decrease_option_value(ui_droLED,
                                                          &led_current,
                                                          led_total);
                                } else if (element_focused == ui_lblTheme) {
                                    decrease_option_value(ui_droTheme,
                                                          &random_theme_current,
                                                          random_theme_total);
                                } else if (element_focused == ui_lblRetroWait) {
                                    decrease_option_value(ui_droRetroWait,
                                                          &retrowait_current,
                                                          retrowait_total);
                                } else if (element_focused == ui_lblUSBFunction) {
                                    decrease_option_value(ui_droUSBFunction,
                                                          &usbfunction_current,
                                                          usbfunction_total);
                                } else if (element_focused == ui_lblState) {
                                    decrease_option_value(ui_droState,
                                                          &state_current,
                                                          state_total);
                                } else if (element_focused == ui_lblVerbose) {
                                    decrease_option_value(ui_droVerbose,
                                                          &verbose_current,
                                                          verbose_total);
                                } else if (element_focused == ui_lblHDMIOutput) {
                                    decrease_option_value(ui_droHDMIOutput,
                                                          &hdmi_output_current,
                                                          hdmi_output_total);
                                }
                            } else if (ev.value == device.INPUT.AXIS || ev.value == 1) {
                                play_sound("navigate", nav_sound, 0);
                                if (element_focused == ui_lblAccelerate) {
                                    increase_option_value(ui_droAccelerate,
                                                          &accelerate_current,
                                                          accelerate_total);
                                } else if (element_focused == ui_lblSwap) {
                                    increase_option_value(ui_droSwap,
                                                          &swap_current,
                                                          swap_total);
                                } else if (element_focused == ui_lblThermal) {
                                    increase_option_value(ui_droThermal,
                                                          &thermal_current,
                                                          thermal_total);
                                } else if (element_focused == ui_lblFont) {
                                    increase_option_value(ui_droFont,
                                                          &font_current,
                                                          font_total);
                                } else if (element_focused == ui_lblVolume) {
                                    increase_option_value(ui_droVolume,
                                                          &volume_current,
                                                          volume_total);
                                } else if (element_focused == ui_lblBrightness) {
                                    increase_option_value(ui_droBrightness,
                                                          &brightness_current,
                                                          brightness_total);
                                } else if (element_focused == ui_lblOffset) {
                                    increase_option_value(ui_droOffset,
                                                          &offset_current,
                                                          offset_total);
                                } else if (element_focused == ui_lblPasscode) {
                                    increase_option_value(ui_droPasscode,
                                                          &lockdown_current,
                                                          lockdown_total);
                                } else if (element_focused == ui_lblLED) {
                                    increase_option_value(ui_droLED,
                                                          &led_current,
                                                          led_total);
                                } else if (element_focused == ui_lblTheme) {
                                    increase_option_value(ui_droTheme,
                                                          &random_theme_current,
                                                          random_theme_total);
                                } else if (element_focused == ui_lblRetroWait) {
                                    increase_option_value(ui_droRetroWait,
                                                          &retrowait_current,
                                                          retrowait_total);
                                } else if (element_focused == ui_lblUSBFunction) {
                                    increase_option_value(ui_droUSBFunction,
                                                          &usbfunction_current,
                                                          usbfunction_total);
                                } else if (element_focused == ui_lblState) {
                                    increase_option_value(ui_droState,
                                                          &state_current,
                                                          state_total);
                                } else if (element_focused == ui_lblVerbose) {
                                    increase_option_value(ui_droVerbose,
                                                          &verbose_current,
                                                          verbose_total);
                                } else if (element_focused == ui_lblHDMIOutput) {
                                    increase_option_value(ui_droHDMIOutput,
                                                          &hdmi_output_current,
                                                          hdmi_output_total);
                                }
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
            refresh_screen();
        }

        if (JOYUP_pressed || JOYDOWN_pressed) {
            if (nav_hold > 2) {
                if (JOYUP_pressed && current_item_index > 0) {
                    list_nav_prev(1);
                }
                if (JOYDOWN_pressed && current_item_index < ui_count - 1) {
                    list_nav_next(1);
                }
            }
            nav_hold++;
        } else {
            nav_hold = 0;
        }

        if (!atoi(read_line_from_file("/tmp/hdmi_in_use", 1)) || config.SETTINGS.ADVANCED.HDMIOUTPUT) {
            if (ev.type == EV_KEY && ev.value == 1 &&
                (ev.code == device.RAW_INPUT.BUTTON.VOLUME_DOWN || ev.code == device.RAW_INPUT.BUTTON.VOLUME_UP)) {
                if (JOYHOTKEY_pressed) {
                    progress_onscreen = 1;
                    lv_obj_add_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_icoProgressBrightness, "\uF185");
                    lv_bar_set_value(ui_barProgressBrightness, atoi(read_text_from_file(BRIGHT_PERC)), LV_ANIM_OFF);
                } else {
                    progress_onscreen = 2;
                    lv_obj_add_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN);
                    int volume = atoi(read_text_from_file(VOLUME_PERC));
                    switch (volume) {
                        default:
                        case 0:
                            lv_label_set_text(ui_icoProgressVolume, "\uF6A9");
                            break;
                        case 1 ... 46:
                            lv_label_set_text(ui_icoProgressVolume, "\uF026");
                            break;
                        case 47 ... 71:
                            lv_label_set_text(ui_icoProgressVolume, "\uF027");
                            break;
                        case 72 ... 100:
                            lv_label_set_text(ui_icoProgressVolume, "\uF028");
                            break;
                    }
                    lv_bar_set_value(ui_barProgressVolume, volume, LV_ANIM_OFF);
                }
            }
        }

        if (file_exist("/tmp/hdmi_do_refresh")) {
            if (atoi(read_text_from_file("/tmp/hdmi_do_refresh"))) {
                remove("/tmp/hdmi_do_refresh");
                lv_obj_invalidate(ui_pnlHeader);
                lv_obj_invalidate(ui_pnlContent);
                lv_obj_invalidate(ui_pnlFooter);
            }
        }

        refresh_screen();
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
    lv_obj_set_user_data(ui_lblHDMIOutput, "hdmi");
    lv_obj_set_user_data(ui_lblStorage, "storage");

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

    char *overlay = load_overlay_image();
    if (strlen(overlay) > 0 && theme.MISC.IMAGE_OVERLAY) {
        lv_obj_t *overlay_img = lv_img_create(ui_screen);
        lv_img_set_src(overlay_img, overlay);
        lv_obj_move_foreground(overlay_img);
    }

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
            static char old_wall[MAX_BUFFER_SIZE];
            static char new_wall[MAX_BUFFER_SIZE];

            snprintf(old_wall, sizeof(old_wall), "%s", current_wall);
            snprintf(new_wall, sizeof(new_wall), "%s", load_wallpaper(
                    ui_screen, ui_group, theme.MISC.ANIMATED_BACKGROUND, theme.MISC.RANDOM_BACKGROUND));

            if (strcasecmp(new_wall, old_wall) != 0) {
                strcpy(current_wall, new_wall);
                if (strlen(new_wall) > 3) {
                    if (theme.MISC.RANDOM_BACKGROUND) {
                        load_image_random(ui_imgWall, new_wall);
                    } else {
                        switch (theme.MISC.ANIMATED_BACKGROUND) {
                            case 1:
                                lv_gif_set_src(lv_gif_create(ui_pnlWall), new_wall);
                                break;
                            case 2:
                                load_image_animation(ui_imgWall, theme.ANIMATION.ANIMATION_DELAY, new_wall);
                                break;
                            default:
                                lv_img_set_src(ui_imgWall, new_wall);
                                break;
                        }
                    }
                } else {
                    lv_img_set_src(ui_imgWall, &ui_image_Nothing);
                }
            }

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

    mux_prog = basename(argv[0]);
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
    load_language(mux_prog);

    ui_common_screen_init(&theme, &device, TS("ADVANCED SETTINGS"));
    ui_init(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());

    switch (theme.MISC.NAVIGATION_TYPE) {
        case 1:
            NAV_DPAD_HOR = device.RAW_INPUT.DPAD.DOWN;
            NAV_ANLG_HOR = device.RAW_INPUT.ANALOG.LEFT.DOWN;
            NAV_DPAD_VER = device.RAW_INPUT.DPAD.RIGHT;
            NAV_ANLG_VER = device.RAW_INPUT.ANALOG.LEFT.RIGHT;
            break;
        default:
            NAV_DPAD_HOR = device.RAW_INPUT.DPAD.RIGHT;
            NAV_ANLG_HOR = device.RAW_INPUT.ANALOG.LEFT.RIGHT;
            NAV_DPAD_VER = device.RAW_INPUT.DPAD.DOWN;
            NAV_ANLG_VER = device.RAW_INPUT.ANALOG.LEFT.DOWN;
    }

    switch (config.SETTINGS.ADVANCED.SWAP) {
        case 1:
            NAV_A = device.RAW_INPUT.BUTTON.B;
            NAV_B = device.RAW_INPUT.BUTTON.A;
            break;
        default:
            NAV_A = device.RAW_INPUT.BUTTON.A;
            NAV_B = device.RAW_INPUT.BUTTON.B;
            break;
    }

    current_wall = load_wallpaper(ui_screen, NULL, theme.MISC.ANIMATED_BACKGROUND, theme.MISC.RANDOM_BACKGROUND);
    if (strlen(current_wall) > 3) {
        if (theme.MISC.RANDOM_BACKGROUND) {
            load_image_random(ui_imgWall, current_wall);
        } else {
            switch (theme.MISC.ANIMATED_BACKGROUND) {
                case 1:
                    lv_gif_set_src(lv_gif_create(ui_pnlWall), current_wall);
                    break;
                case 2:
                    load_image_animation(ui_imgWall, theme.ANIMATION.ANIMATION_DELAY, current_wall);
                    break;
                default:
                    lv_img_set_src(ui_imgWall, current_wall);
                    break;
            }
        }
    } else {
        lv_img_set_src(ui_imgWall, &ui_image_Nothing);
    }

    load_font_text(basename(argv[0]), ui_screen);
    load_font_section(basename(argv[0]), FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(mux_prog, FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(mux_prog, FONT_FOOTER_FOLDER, ui_pnlFooter);

    if (config.SETTINGS.GENERAL.SOUND) {
        if (SDL_Init(SDL_INIT_AUDIO) >= 0) {
            Mix_Init(0);
            Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
            printf("SDL init success!\n");
            nav_sound = 1;
        } else {
            fprintf(stderr, "Failed to init SDL\n");
        }
    }

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

    refresh_screen();
    joystick_task();

    close(js_fd);
    close(js_fd_sys);

    return 0;
}

uint32_t mux_tick(void) {
    struct timespec tv_now;
    clock_gettime(CLOCK_REALTIME, &tv_now);

    uint64_t now_ms = ((uint64_t) tv_now.tv_sec * 1000) + (tv_now.tv_nsec / 1000000);
    start_ms = start_ms || now_ms;

    return (uint32_t) (now_ms - start_ms);
}
