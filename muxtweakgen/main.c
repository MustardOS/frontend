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
#include "../common/input.h"
#include "../common/input/list_nav.h"

__thread uint64_t start_ms = 0;

char *mux_prog;
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
char *current_wall = "";
int current_item_index = 0;
int ui_count = 0;

lv_obj_t *msgbox_element = NULL;

int progress_onscreen = -1;

int hidden_total, hidden_current;
int bgm_total, bgm_current;
int sound_total, sound_current;
int startup_total, startup_current;
int colour_total, colour_current;
int brightness_total, brightness_current;
int hdmi_total, hdmi_current;
int shutdown_total, shutdown_current;
int battery_total, battery_current;
int idle_display_total, idle_display_current;
int idle_sleep_total, idle_sleep_current;

typedef struct {
    int *total;
    int *current;
} Tweak;

Tweak hidden, bgm, sound, startup, colour, brightness, hdmi, shutdown, battery, idle_display, idle_sleep;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

#define UI_COUNT 13
lv_obj_t *ui_objects[UI_COUNT];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblHidden,      "HELP.HIDDEN"},
            {ui_lblBGM,         "HELP.BGM"},
            {ui_lblSound,       "HELP.SOUND"},
            {ui_lblStartup,     "HELP.STARTUP"},
            {ui_lblColour,      "HELP.COLOUR"},
            {ui_lblBrightness,  "HELP.BRIGHTNESS"},
            {ui_lblHDMI,        "HELP.HDMI"},
            {ui_lblShutdown,    "HELP.SHUTDOWN"},
            {ui_lblBattery,     "HELP.BATTERY"},
            {ui_lblIdleDisplay, "HELP.IDLE_DISPLAY"},
            {ui_lblIdleSleep,   "HELP.IDLE_SLEEP"},
            {ui_lblInterface,   "HELP.INTERFACE"},
            {ui_lblAdvanced,    "HELP.ADVANCED"},
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
            ui_droHidden,
            ui_droBGM,
            ui_droSound,
            ui_droStartup,
            ui_droColour,
            ui_droBrightness,
            ui_droHDMI,
            ui_droShutdown,
            ui_droBattery,
            ui_droIdleDisplay,
            ui_droIdleSleep
    };

    for (unsigned int i = 0; i < sizeof(dropdowns) / sizeof(dropdowns[0]); i++) {
        lv_obj_add_event_cb(dropdowns[i], dropdown_event_handler, LV_EVENT_ALL, NULL);
    }

    init_pointers(&hidden, &hidden_total, &hidden_current);
    init_pointers(&bgm, &bgm_total, &bgm_current);
    init_pointers(&sound, &sound_total, &sound_current);
    init_pointers(&startup, &startup_total, &startup_current);
    init_pointers(&colour, &colour_total, &colour_current);
    init_pointers(&brightness, &brightness_total, &brightness_current);
    init_pointers(&hdmi, &hdmi_total, &hdmi_current);
    init_pointers(&shutdown, &shutdown_total, &shutdown_current);
    init_pointers(&battery, &battery_total, &battery_current);
    init_pointers(&idle_display, &idle_display_total, &idle_display_current);
    init_pointers(&idle_sleep, &idle_sleep_total, &idle_sleep_current);
}

void init_dropdown_settings() {
    Tweak settings[] = {
            {hidden.total,       hidden.current},
            {bgm.total,          bgm.current},
            {sound.total,        sound.current},
            {startup.total,      startup.current},
            {colour.total,       colour.current},
            {brightness.total,   brightness.current},
            {hdmi.total,         hdmi.current},
            {shutdown.total,     shutdown.current},
            {battery.total,      battery.current},
            {idle_display.total, idle_display.current},
            {idle_sleep.total,   idle_sleep.current}
    };

    lv_obj_t *dropdowns[] = {
            ui_droHidden,
            ui_droBGM,
            ui_droSound,
            ui_droStartup,
            ui_droColour,
            ui_droBrightness,
            ui_droHDMI,
            ui_droShutdown,
            ui_droBattery,
            ui_droIdleDisplay,
            ui_droIdleSleep,
    };

    for (unsigned int i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
        *(settings[i].total) = lv_dropdown_get_option_cnt(dropdowns[i]);
        *(settings[i].current) = lv_dropdown_get_selected(dropdowns[i]);
    }
}

void restore_tweak_options() {
    lv_dropdown_set_selected(ui_droHidden, config.SETTINGS.GENERAL.HIDDEN);
    lv_dropdown_set_selected(ui_droBGM, config.SETTINGS.GENERAL.BGM);
    lv_dropdown_set_selected(ui_droSound, config.SETTINGS.GENERAL.SOUND);
    lv_dropdown_set_selected(ui_droBrightness, config.SETTINGS.GENERAL.BRIGHTNESS + 1);

    switch (config.SETTINGS.GENERAL.HDMI) {
        case -1:
            lv_dropdown_set_selected(ui_droHDMI, 0);
            break;
        case 0:
            lv_dropdown_set_selected(ui_droHDMI, 1);
            break;
        case 1:
            lv_dropdown_set_selected(ui_droHDMI, 2);
            break;
        case 2:
            lv_dropdown_set_selected(ui_droHDMI, 3);
            break;
        case 3:
            lv_dropdown_set_selected(ui_droHDMI, 4);
            break;
        case 4:
            lv_dropdown_set_selected(ui_droHDMI, 5);
            break;
        case 5:
            lv_dropdown_set_selected(ui_droHDMI, 6);
            break;
        case 6:
            lv_dropdown_set_selected(ui_droHDMI, 7);
            break;
        case 7:
            lv_dropdown_set_selected(ui_droHDMI, 8);
            break;
        case 8:
            lv_dropdown_set_selected(ui_droHDMI, 9);
            break;
        case 9:
            lv_dropdown_set_selected(ui_droHDMI, 10);
            break;
        case 10:
            lv_dropdown_set_selected(ui_droHDMI, 11);
            break;
        default:
            lv_dropdown_set_selected(ui_droHDMI, 0);
            break;
    }

    const char *startup_type = config.SETTINGS.GENERAL.STARTUP;
    if (strcasecmp(startup_type, "explore") == 0) {
        lv_dropdown_set_selected(ui_droStartup, 1);
    } else if (strcasecmp(startup_type, "favourite") == 0) {
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

    switch (config.SETTINGS.GENERAL.SHUTDOWN) {
        case -2:
            lv_dropdown_set_selected(ui_droShutdown, 0);
            break;
        case -1:
            lv_dropdown_set_selected(ui_droShutdown, 1);
            break;
        case 2:
            lv_dropdown_set_selected(ui_droShutdown, 2);
            break;
        case 10:
            lv_dropdown_set_selected(ui_droShutdown, 3);
            break;
        case 30:
            lv_dropdown_set_selected(ui_droShutdown, 4);
            break;
        case 60:
            lv_dropdown_set_selected(ui_droShutdown, 5);
            break;
        case 120:
            lv_dropdown_set_selected(ui_droShutdown, 6);
            break;
        case 300:
            lv_dropdown_set_selected(ui_droShutdown, 7);
            break;
        case 600:
            lv_dropdown_set_selected(ui_droShutdown, 8);
            break;
        case 1800:
            lv_dropdown_set_selected(ui_droShutdown, 9);
            break;
        case 3600:
            lv_dropdown_set_selected(ui_droShutdown, 10);
            break;
        default:
            lv_dropdown_set_selected(ui_droShutdown, 0);
            break;
    }

    switch (config.SETTINGS.GENERAL.LOW_BATTERY) {
        case -255:
            lv_dropdown_set_selected(ui_droBattery, 0);
            break;
        case 5:
            lv_dropdown_set_selected(ui_droBattery, 1);
            break;
        case 10:
            lv_dropdown_set_selected(ui_droBattery, 2);
            break;
        case 15:
            lv_dropdown_set_selected(ui_droBattery, 3);
            break;
        case 20:
            lv_dropdown_set_selected(ui_droBattery, 4);
            break;
        case 25:
            lv_dropdown_set_selected(ui_droBattery, 5);
            break;
        case 30:
            lv_dropdown_set_selected(ui_droBattery, 6);
            break;
        case 35:
            lv_dropdown_set_selected(ui_droBattery, 7);
            break;
        case 40:
            lv_dropdown_set_selected(ui_droBattery, 8);
            break;
        case 45:
            lv_dropdown_set_selected(ui_droBattery, 9);
            break;
        case 50:
            lv_dropdown_set_selected(ui_droBattery, 10);
            break;
        default:
            lv_dropdown_set_selected(ui_droBattery, 5);
            break;
    }

    switch (config.SETTINGS.GENERAL.COLOUR) {
        case -256:
            lv_dropdown_set_selected(ui_droColour, 0);
            break;
        case -224:
            lv_dropdown_set_selected(ui_droColour, 1);
            break;
        case -192:
            lv_dropdown_set_selected(ui_droColour, 2);
            break;
        case -160:
            lv_dropdown_set_selected(ui_droColour, 3);
            break;
        case -128:
            lv_dropdown_set_selected(ui_droColour, 4);
            break;
        case -96:
            lv_dropdown_set_selected(ui_droColour, 5);
            break;
        case -64:
            lv_dropdown_set_selected(ui_droColour, 6);
            break;
        case -32:
            lv_dropdown_set_selected(ui_droColour, 7);
            break;
        case 0:
            lv_dropdown_set_selected(ui_droColour, 8);
            break;
        case 32:
            lv_dropdown_set_selected(ui_droColour, 9);
            break;
        case 64:
            lv_dropdown_set_selected(ui_droColour, 10);
            break;
        case 96:
            lv_dropdown_set_selected(ui_droColour, 11);
            break;
        case 128:
            lv_dropdown_set_selected(ui_droColour, 12);
            break;
        case 160:
            lv_dropdown_set_selected(ui_droColour, 13);
            break;
        case 192:
            lv_dropdown_set_selected(ui_droColour, 14);
            break;
        case 224:
            lv_dropdown_set_selected(ui_droColour, 15);
            break;
        case 256:
            lv_dropdown_set_selected(ui_droColour, 16);
            break;
        default:
            lv_dropdown_set_selected(ui_droColour, 9);
            break;
    }

    switch (config.SETTINGS.GENERAL.IDLE_DISPLAY) {
        case 10:
            lv_dropdown_set_selected(ui_droIdleDisplay, 1);
            break;
        case 30:
            lv_dropdown_set_selected(ui_droIdleDisplay, 2);
            break;
        case 60:
            lv_dropdown_set_selected(ui_droIdleDisplay, 3);
            break;
        case 120:
            lv_dropdown_set_selected(ui_droIdleDisplay, 4);
            break;
        case 300:
            lv_dropdown_set_selected(ui_droIdleDisplay, 5);
            break;
        case 600:
            lv_dropdown_set_selected(ui_droIdleDisplay, 6);
            break;
        case 900:
            lv_dropdown_set_selected(ui_droIdleDisplay, 7);
            break;
        case 1800:
            lv_dropdown_set_selected(ui_droIdleDisplay, 8);
            break;
        default:
            lv_dropdown_set_selected(ui_droIdleDisplay, 0);
            break;
    }

    switch (config.SETTINGS.GENERAL.IDLE_SLEEP) {
        case 10:
            lv_dropdown_set_selected(ui_droIdleSleep, 1);
            break;
        case 30:
            lv_dropdown_set_selected(ui_droIdleSleep, 2);
            break;
        case 60:
            lv_dropdown_set_selected(ui_droIdleSleep, 3);
            break;
        case 120:
            lv_dropdown_set_selected(ui_droIdleSleep, 4);
            break;
        case 300:
            lv_dropdown_set_selected(ui_droIdleSleep, 5);
            break;
        case 600:
            lv_dropdown_set_selected(ui_droIdleSleep, 6);
            break;
        case 900:
            lv_dropdown_set_selected(ui_droIdleSleep, 7);
            break;
        case 1800:
            lv_dropdown_set_selected(ui_droIdleSleep, 8);
            break;
        default:
            lv_dropdown_set_selected(ui_droIdleSleep, 0);
            break;
    }
}

void save_tweak_options() {
    int idx_hdmi;
    switch (lv_dropdown_get_selected(ui_droHDMI)) {
        case 0:
            idx_hdmi = -1;
            break;
        case 1:
            idx_hdmi = 0;
            break;
        case 2:
            idx_hdmi = 1;
            break;
        case 3:
            idx_hdmi = 2;
            break;
        case 4:
            idx_hdmi = 3;
            break;
        case 5:
            idx_hdmi = 4;
            break;
        case 6:
            idx_hdmi = 5;
            break;
        case 7:
            idx_hdmi = 6;
            break;
        case 8:
            idx_hdmi = 7;
            break;
        case 9:
            idx_hdmi = 8;
            break;
        case 10:
            idx_hdmi = 9;
            break;
        case 11:
            idx_hdmi = 10;
            break;
        default:
            idx_hdmi = -1;
            break;
    }

    char *idx_startup;
    switch (lv_dropdown_get_selected(ui_droStartup)) {
        case 0:
            idx_startup = "launcher";
            break;
        case 1:
            idx_startup = "explore";
            break;
        case 2:
            idx_startup = "favourite";
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

    int idx_shutdown;
    switch (lv_dropdown_get_selected(ui_droShutdown)) {
        case 0:
            idx_shutdown = -2;
            break;
        case 1:
            idx_shutdown = -1;
            break;
        case 2:
            idx_shutdown = 2;
            break;
        case 3:
            idx_shutdown = 10;
            break;
        case 4:
            idx_shutdown = 30;
            break;
        case 5:
            idx_shutdown = 60;
            break;
        case 6:
            idx_shutdown = 120;
            break;
        case 7:
            idx_shutdown = 300;
            break;
        case 8:
            idx_shutdown = 600;
            break;
        case 9:
            idx_shutdown = 1800;
            break;
        case 10:
            idx_shutdown = 3600;
            break;
        default:
            idx_shutdown = -2;
            break;
    }

    int idx_battery;
    switch (lv_dropdown_get_selected(ui_droBattery)) {
        case 0:
            idx_battery = -255;
            break;
        case 1:
            idx_battery = 5;
            break;
        case 2:
            idx_battery = 10;
            break;
        case 3:
            idx_battery = 15;
            break;
        case 4:
            idx_battery = 20;
            break;
        case 5:
            idx_battery = 25;
            break;
        case 6:
            idx_battery = 30;
            break;
        case 7:
            idx_battery = 35;
            break;
        case 8:
            idx_battery = 40;
            break;
        case 9:
            idx_battery = 45;
            break;
        case 10:
            idx_battery = 50;
            break;
        default:
            idx_battery = 25;
            break;
    }

    int idx_colour;
    switch (lv_dropdown_get_selected(ui_droColour)) {
        case 0:
            idx_colour = -256;
            break;
        case 1:
            idx_colour = -224;
            break;
        case 2:
            idx_colour = -192;
            break;
        case 3:
            idx_colour = -160;
            break;
        case 4:
            idx_colour = -128;
            break;
        case 5:
            idx_colour = -96;
            break;
        case 6:
            idx_colour = -64;
            break;
        case 7:
            idx_colour = -32;
            break;
        case 8:
            idx_colour = 0;
            break;
        case 9:
            idx_colour = 32;
            break;
        case 10:
            idx_colour = 64;
            break;
        case 11:
            idx_colour = 96;
            break;
        case 12:
            idx_colour = 128;
            break;
        case 13:
            idx_colour = 160;
            break;
        case 14:
            idx_colour = 192;
            break;
        case 15:
            idx_colour = 224;
            break;
        case 16:
            idx_colour = 256;
            break;
        default:
            idx_colour = 32;
            break;
    }

    int idx_idle_display;
    switch (lv_dropdown_get_selected(ui_droIdleDisplay)) {
        case 0:
            idx_idle_display = 0;
            break;
        case 1:
            idx_idle_display = 10;
            break;
        case 2:
            idx_idle_display = 30;
            break;
        case 3:
            idx_idle_display = 60;
            break;
        case 4:
            idx_idle_display = 120;
            break;
        case 5:
            idx_idle_display = 300;
            break;
        case 6:
            idx_idle_display = 600;
            break;
        case 7:
            idx_idle_display = 900;
            break;
        case 8:
            idx_idle_display = 1800;
            break;
        default:
            idx_idle_display = 0;
            break;
    }

    int idx_idle_sleep;
    switch (lv_dropdown_get_selected(ui_droIdleSleep)) {
        case 0:
            idx_idle_sleep = 0;
            break;
        case 1:
            idx_idle_sleep = 10;
            break;
        case 2:
            idx_idle_sleep = 30;
            break;
        case 3:
            idx_idle_sleep = 60;
            break;
        case 4:
            idx_idle_sleep = 120;
            break;
        case 5:
            idx_idle_sleep = 300;
            break;
        case 6:
            idx_idle_sleep = 600;
            break;
        case 7:
            idx_idle_sleep = 900;
            break;
        case 8:
            idx_idle_sleep = 1800;
            break;
        default:
            idx_idle_sleep = 0;
            break;
    }

    int idx_hidden = lv_dropdown_get_selected(ui_droHidden);
    int idx_bgm = lv_dropdown_get_selected(ui_droBGM);
    int idx_sound = lv_dropdown_get_selected(ui_droSound);
    int idx_brightness = lv_dropdown_get_selected(ui_droBrightness);

    write_text_to_file("/run/muos/global/settings/general/hidden", "w", INT, idx_hidden);
    write_text_to_file("/run/muos/global/settings/general/bgm", "w", INT, idx_bgm);
    write_text_to_file("/run/muos/global/settings/general/sound", "w", INT, idx_sound);
    write_text_to_file("/run/muos/global/settings/general/brightness", "w", INT, idx_brightness);
    write_text_to_file("/run/muos/global/settings/general/startup", "w", CHAR, idx_startup);
    write_text_to_file("/run/muos/global/settings/general/colour", "w", INT, idx_colour);
    write_text_to_file("/run/muos/global/settings/general/hdmi", "w", INT, idx_hdmi);
    write_text_to_file("/run/muos/global/settings/general/shutdown", "w", INT, idx_shutdown);
    write_text_to_file("/run/muos/global/settings/general/low_battery", "w", INT, idx_battery);
    write_text_to_file("/run/muos/global/settings/general/idle_display", "w", INT, idx_idle_display);
    write_text_to_file("/run/muos/global/settings/general/idle_sleep", "w", INT, idx_idle_sleep);

    char br_num[MAX_BUFFER_SIZE];
    lv_dropdown_get_selected_str(ui_droBrightness, br_num, MAX_BUFFER_SIZE);

    char command[MAX_BUFFER_SIZE];
    snprintf(command, sizeof(command), "%s/device/%s/input/combo/bright.sh %d",
             INTERNAL_PATH, str_tolower(device.DEVICE.NAME), atoi(br_num));
    system(command);

    static char tweak_script[MAX_BUFFER_SIZE];
    snprintf(tweak_script, sizeof(tweak_script),
             "%s/script/mux/tweak.sh", INTERNAL_PATH);
    system(tweak_script);
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
            ui_pnlShutdown,
            ui_pnlBattery,
            ui_pnlIdleDisplay,
            ui_pnlIdleSleep,
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
    ui_objects[7] = ui_lblShutdown;
    ui_objects[8] = ui_lblBattery;
    ui_objects[9] = ui_lblIdleDisplay;
    ui_objects[10] = ui_lblIdleSleep;
    ui_objects[11] = ui_lblInterface;
    ui_objects[12] = ui_lblAdvanced;

    lv_obj_t *ui_objects_value[] = {
            ui_droHidden,
            ui_droBGM,
            ui_droSound,
            ui_droStartup,
            ui_droColour,
            ui_droBrightness,
            ui_droHDMI,
            ui_droShutdown,
            ui_droBattery,
            ui_droIdleDisplay,
            ui_droIdleSleep,
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
            ui_icoShutdown,
            ui_icoBattery,
            ui_icoIdleDisplay,
            ui_icoIdleSleep,
            ui_icoInterface,
            ui_icoAdvanced
    };

    apply_theme_list_panel(&theme, &device, ui_pnlHidden);
    apply_theme_list_panel(&theme, &device, ui_pnlBGM);
    apply_theme_list_panel(&theme, &device, ui_pnlSound);
    apply_theme_list_panel(&theme, &device, ui_pnlStartup);
    apply_theme_list_panel(&theme, &device, ui_pnlColour);
    apply_theme_list_panel(&theme, &device, ui_pnlBrightness);
    apply_theme_list_panel(&theme, &device, ui_pnlHDMI);
    apply_theme_list_panel(&theme, &device, ui_pnlShutdown);
    apply_theme_list_panel(&theme, &device, ui_pnlBattery);
    apply_theme_list_panel(&theme, &device, ui_pnlIdleDisplay);
    apply_theme_list_panel(&theme, &device, ui_pnlIdleSleep);
    apply_theme_list_panel(&theme, &device, ui_pnlInterface);
    apply_theme_list_panel(&theme, &device, ui_pnlAdvanced);

    apply_theme_list_item(&theme, ui_lblHidden, TS("Show Hidden Content"), false, true);
    apply_theme_list_item(&theme, ui_lblBGM, TS("Background Music"), false, true);
    apply_theme_list_item(&theme, ui_lblSound, TS("Navigation Sound"), false, true);
    apply_theme_list_item(&theme, ui_lblStartup, TS("Device Startup"), false, true);
    apply_theme_list_item(&theme, ui_lblColour, TS("Colour Temperature"), false, true);
    apply_theme_list_item(&theme, ui_lblBrightness, TS("Brightness"), false, true);
    apply_theme_list_item(&theme, ui_lblHDMI, TS("HDMI Output"), false, true);
    apply_theme_list_item(&theme, ui_lblShutdown, TS("Sleep Function"), false, true);
    apply_theme_list_item(&theme, ui_lblBattery, TS("Low Battery Indicator"), false, true);
    apply_theme_list_item(&theme, ui_lblIdleDisplay, TS("Idle Input Display Timeout"), false, true);
    apply_theme_list_item(&theme, ui_lblIdleSleep, TS("Idle Input Sleep Timeout"), false, true);
    apply_theme_list_item(&theme, ui_lblInterface, TS("Interface Options"), false, true);
    apply_theme_list_item(&theme, ui_lblAdvanced, TS("Advanced Settings"), false, true);

    apply_theme_list_glyph(&theme, ui_icoHidden, mux_prog, "hidden");
    apply_theme_list_glyph(&theme, ui_icoBGM, mux_prog, "bgm");
    apply_theme_list_glyph(&theme, ui_icoSound, mux_prog, "sound");
    apply_theme_list_glyph(&theme, ui_icoStartup, mux_prog, "startup");
    apply_theme_list_glyph(&theme, ui_icoColour, mux_prog, "colour");
    apply_theme_list_glyph(&theme, ui_icoBrightness, mux_prog, "brightness");
    apply_theme_list_glyph(&theme, ui_icoHDMI, mux_prog, "hdmi");
    apply_theme_list_glyph(&theme, ui_icoShutdown, mux_prog, "shutdown");
    apply_theme_list_glyph(&theme, ui_icoBattery, mux_prog, "battery");
    apply_theme_list_glyph(&theme, ui_icoIdleDisplay, mux_prog, "idle_display");
    apply_theme_list_glyph(&theme, ui_icoIdleSleep, mux_prog, "idle_sleep");
    apply_theme_list_glyph(&theme, ui_icoInterface, mux_prog, "interface");
    apply_theme_list_glyph(&theme, ui_icoAdvanced, mux_prog, "advanced");

    apply_theme_list_drop_down(&theme, ui_droHidden, NULL);
    apply_theme_list_drop_down(&theme, ui_droBGM, NULL);
    apply_theme_list_drop_down(&theme, ui_droSound, NULL);
    apply_theme_list_drop_down(&theme, ui_droStartup, NULL);
    apply_theme_list_drop_down(&theme, ui_droColour, NULL);

    char *brightness_string = generate_number_string(1, device.SCREEN.BRIGHT, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droBrightness, brightness_string);
    free(brightness_string);

    apply_theme_list_drop_down(&theme, ui_droHDMI, NULL);
    apply_theme_list_drop_down(&theme, ui_droShutdown, NULL);

    char *battery_string = generate_number_string(5, 50, 5, TG("Disabled"), NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droBattery, battery_string);
    free(battery_string);

    apply_theme_list_drop_down(&theme, ui_droIdleDisplay, "");
    apply_theme_list_drop_down(&theme, ui_droIdleSleep, "");
    apply_theme_list_drop_down(&theme, ui_droInterface, "");
    apply_theme_list_drop_down(&theme, ui_droAdvanced, "");

    char *disabled_enabled[] = {TG("Disabled"), TG("Enabled")};
    add_drop_down_options(ui_droHidden, disabled_enabled, 2);
    add_drop_down_options(ui_droBGM, disabled_enabled, 2);
    add_drop_down_options(ui_droSound, disabled_enabled, 2);
    add_drop_down_options(ui_droStartup, (char *[]) {
            TS("Main Menu"), TS("Content Explorer"), TS("Favourites"),
            TS("History"), TS("Last Game"), TS("Resume Game")}, 6);
    add_drop_down_options(ui_droColour, (char *[]) {
            TS("Deep Arctic (-256)"), TS("Icy Chill (-224)"), TS("Frosty Breeze (-192)"),
            TS("Cool Glacier (-160)"), TS("Arctic Frost (-128)"), TS("Winter Sky (-96)"),
            TS("Frostbite Blue (-64)"), TS("Arctic Blue (-32)"), TS("Neutral White (0)"), TS("Daylight White (32)"),
            TS("Warm White (64)"), TS("Soft Ivory (96)"), TS("Candlelight Yellow (128)"), TS("Warm Glow (160)"),
            TS("Sunset Orange (192)"), TS("Amber Flame (224)"), TS("Deep Ember (256)")}, 17);
    add_drop_down_options(ui_droHDMI, (char *[]) {
            TG("Disabled"), TS("480i"), TS("576i"), TS("480p"), TS("576p"), TS("720p + 50hz"), TS("720p + 60hz"),
            TS("1080i + 50hz"), TS("1080i + 60hz"), TS("1080p + 24hz"), TS("1080p + 50hz"), TS("1080p + 60hz")}, 12);
    add_drop_down_options(ui_droShutdown, (char *[]) {
            TG("Disabled"), TS("Sleep Suspend"), TS("Instant Shutdown"),
            TS("Sleep 10s + Shutdown"), TS("Sleep 30s + Shutdown"),
            TS("Sleep 60s + Shutdown"), TS("Sleep 2m + Shutdown"),
            TS("Sleep 5m + Shutdown"), TS("Sleep 10m + Shutdown"),
            TS("Sleep 30m + Shutdown"), TS("Sleep 60m + Shutdown")}, 11);
    add_drop_down_options(ui_droIdleDisplay, (char *[]) {
            TG("Disabled"), TS("10s"), TS("30s"), TS("60s"), TS("2m"), TS("5m"), TS("10m"), TS("15m"), TS("30m")}, 9);
    add_drop_down_options(ui_droIdleSleep, (char *[]) {
            TG("Disabled"), TS("10s"), TS("30s"), TS("60s"), TS("2m"), TS("5m"), TS("10m"), TS("15m"), TS("30m")}, 9);

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
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblHidden) {
        decrease_option_value(ui_droHidden,
                              &hidden_current,
                              hidden_total);
    } else if (element_focused == ui_lblBGM) {
        decrease_option_value(ui_droBGM,
                              &bgm_current,
                              bgm_total);
    } else if (element_focused == ui_lblSound) {
        decrease_option_value(ui_droSound,
                              &sound_current,
                              sound_total);
    } else if (element_focused == ui_lblStartup) {
        decrease_option_value(ui_droStartup,
                              &startup_current,
                              startup_total);
    } else if (element_focused == ui_lblColour) {
        decrease_option_value(ui_droColour,
                              &colour_current,
                              colour_total);
    } else if (element_focused == ui_lblBrightness) {
        decrease_option_value(ui_droBrightness,
                              &brightness_current,
                              brightness_total);
    } else if (element_focused == ui_lblHDMI) {
        decrease_option_value(ui_droHDMI,
                              &hdmi_current,
                              hdmi_total);
    } else if (element_focused == ui_lblShutdown) {
        decrease_option_value(ui_droShutdown,
                              &shutdown_current,
                              shutdown_total);
    } else if (element_focused == ui_lblBattery) {
        decrease_option_value(ui_droBattery,
                              &battery_current,
                              battery_total);
    } else if (element_focused == ui_lblIdleDisplay) {
        decrease_option_value(ui_droIdleDisplay,
                              &idle_display_current,
                              idle_display_total);
    } else if (element_focused == ui_lblIdleSleep) {
        decrease_option_value(ui_droIdleSleep,
                              &idle_sleep_current,
                              idle_sleep_total);
    }
}

void handle_option_next(void) {
    if (msgbox_active) {
        return;
    }

    play_sound("navigate", nav_sound, 0);
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblHidden) {
        decrease_option_value(ui_droHidden,
                              &hidden_current,
                              hidden_total);
    } else if (element_focused == ui_lblBGM) {
        increase_option_value(ui_droBGM,
                              &bgm_current,
                              bgm_total);
    } else if (element_focused == ui_lblSound) {
        increase_option_value(ui_droSound,
                              &sound_current,
                              sound_total);
    } else if (element_focused == ui_lblStartup) {
        increase_option_value(ui_droStartup,
                              &startup_current,
                              startup_total);
    } else if (element_focused == ui_lblColour) {
        increase_option_value(ui_droColour,
                              &colour_current,
                              colour_total);
    } else if (element_focused == ui_lblBrightness) {
        increase_option_value(ui_droBrightness,
                              &brightness_current,
                              brightness_total);
    } else if (element_focused == ui_lblHDMI) {
        increase_option_value(ui_droHDMI,
                              &hdmi_current,
                              hdmi_total);
    } else if (element_focused == ui_lblShutdown) {
        increase_option_value(ui_droShutdown,
                              &shutdown_current,
                              shutdown_total);
    } else if (element_focused == ui_lblBattery) {
        increase_option_value(ui_droBattery,
                              &battery_current,
                              battery_total);
    } else if (element_focused == ui_lblIdleDisplay) {
        increase_option_value(ui_droIdleDisplay,
                              &idle_display_current,
                              idle_display_total);
    } else if (element_focused == ui_lblIdleSleep) {
        increase_option_value(ui_droIdleSleep,
                              &idle_sleep_current,
                              idle_sleep_total);
    }
}

void handle_confirm(void) {
    if (msgbox_active) {
        return;
    }

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblInterface) {
        play_sound("confirm", nav_sound, 1);
        save_tweak_options();

        load_mux("visual");
        mux_input_stop();
    } else if (element_focused == ui_lblAdvanced) {
        play_sound("confirm", nav_sound, 1);
        save_tweak_options();

        load_mux("tweakadv");
        mux_input_stop();
    } else {
        handle_option_next();
    }
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

    lv_label_set_text(ui_lblMessage, TG("Saving Changes"));
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);

    save_tweak_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "general");
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

    lv_obj_set_user_data(ui_lblHidden, "hidden");
    lv_obj_set_user_data(ui_lblBGM, "bgm");
    lv_obj_set_user_data(ui_lblSound, "sound");
    lv_obj_set_user_data(ui_lblStartup, "startup");
    lv_obj_set_user_data(ui_lblColour, "colour");
    lv_obj_set_user_data(ui_lblBrightness, "brightness");
    lv_obj_set_user_data(ui_lblHDMI, "hdmi");
    lv_obj_set_user_data(ui_lblShutdown, "shutdown");
    lv_obj_set_user_data(ui_lblBattery, "battery");
    lv_obj_set_user_data(ui_lblIdleDisplay, "idle_display");
    lv_obj_set_user_data(ui_lblIdleSleep, "idle_sleep");
    lv_obj_set_user_data(ui_lblInterface, "interface");
    lv_obj_set_user_data(ui_lblAdvanced, "advanced");

    if (!device.DEVICE.HAS_HDMI) {
        lv_obj_add_flag(ui_pnlHDMI, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlHDMI, LV_OBJ_FLAG_FLOATING);
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
            list_nav_next(text_hit - !device.DEVICE.HAS_HDMI);
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

    ui_common_screen_init(&theme, &device, TS("GENERAL SETTINGS"));
    ui_init(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());

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
            },
            {
                .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_DOWN),
                .press_handler = ui_common_handle_bright,
            },
            {
                .type_mask = BIT(MUX_INPUT_VOL_UP),
                .press_handler = ui_common_handle_vol,
            },
            {
                .type_mask = BIT(MUX_INPUT_VOL_DOWN),
                .press_handler = ui_common_handle_vol,
            },
        },
        .idle_handler = ui_common_handle_idle,
    };
    mux_input_task(&input_opts);

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
