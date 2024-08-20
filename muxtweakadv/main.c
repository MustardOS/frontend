#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <libgen.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "../common/common.h"
#include "../common/help.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/mini/mini.h"

char *mux_prog;
static int js_fd;

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
int safe_quit = 0;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;

struct mux_config config;
struct mux_device device;

int nav_moved = 1;
char *current_wall = "";
int current_item_index = 0;

lv_obj_t *msgbox_element = NULL;

int progress_onscreen = -1;

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
int android_total, android_current;
int state_total, state_current;
int verbose_total, verbose_current;

typedef struct {
    int *total;
    int *current;
} Tweak;

Tweak swap, thermal, font, volume, brightness, offset, lockdown, led, random_theme, retrowait, android, state, verbose;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;

#define UI_COUNT 13
lv_obj_t *ui_objects[UI_COUNT];

void show_help(lv_obj_t *element_focused) {
    char *message = NO_HELP_FOUND;

    if (element_focused == ui_lblSwap) {
        message = MUXTWEAKADV_SWAP;
    } else if (element_focused == ui_lblThermal) {
        message = MUXTWEAKADV_THERMAL;
    } else if (element_focused == ui_lblFont) {
        message = MUXTWEAKADV_FONT;
    } else if (element_focused == ui_lblVolume) {
        message = MUXTWEAKADV_VOLUME;
    } else if (element_focused == ui_lblBrightness) {
        message = MUXTWEAKADV_BRIGHT;
    } else if (element_focused == ui_lblOffset) {
        message = MUXTWEAKADV_OFFSET;
    } else if (element_focused == ui_lblPasscode) {
        message = MUXTWEAKADV_LOCK;
    } else if (element_focused == ui_lblLED) {
        message = MUXTWEAKADV_LED;
    } else if (element_focused == ui_lblTheme) {
        message = MUXTWEAKADV_THEME;
    } else if (element_focused == ui_lblRetroWait) {
        message = MUXTWEAKADV_RETRO;
    } else if (element_focused == ui_lblAndroid) {
        message = MUXTWEAKADV_ANDROID;
    } else if (element_focused == ui_lblState) {
        message = MUXTWEAKADV_STATE;
    } else if (element_focused == ui_lblVerbose) {
        message = MUXTWEAKADV_VERBOSE;
    }

    if (strlen(message) <= 1) {
        message = NO_HELP_FOUND;
    }

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent, lv_label_get_text(element_focused), message);
}

void init_pointers(Tweak *tweak, int *total, int *current) {
    tweak->total = total;
    tweak->current = current;
}

static void dropdown_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        char buf[MAX_BUFFER_SIZE];
        lv_dropdown_get_selected_str(obj, buf, sizeof(buf));
    }
}

void elements_events_init() {
    lv_obj_t *dropdowns[] = {
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
            ui_droAndroid,
            ui_droState,
            ui_droVerbose
    };

    for (unsigned int i = 0; i < sizeof(dropdowns) / sizeof(dropdowns[0]); i++) {
        lv_obj_add_event_cb(dropdowns[i], dropdown_event_handler, LV_EVENT_ALL, NULL);
    }

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
    init_pointers(&android, &android_total, &android_current);
    init_pointers(&state, &state_total, &state_current);
    init_pointers(&verbose, &verbose_total, &verbose_current);
}

void init_dropdown_settings() {
    Tweak settings[] = {
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
            {android.total,      android.current},
            {state.total,        state.current},
            {verbose.total,      verbose.current}
    };

    lv_obj_t *dropdowns[] = {
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
            ui_droAndroid,
            ui_droState,
            ui_droVerbose
    };

    for (unsigned int i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
        *(settings[i].total) = lv_dropdown_get_option_cnt(dropdowns[i]);
        *(settings[i].current) = lv_dropdown_get_selected(dropdowns[i]);
    }
}

void restore_tweak_options() {
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
    lv_dropdown_set_selected(ui_droAndroid, config.SETTINGS.ADVANCED.ANDROID);

    const char *state_type = config.SETTINGS.ADVANCED.STATE;
    if (strcasecmp(state_type, "mem") == 0) {
        lv_dropdown_set_selected(ui_droState, 0);
    } else if (strcasecmp(state_type, "freeze") == 0) {
        lv_dropdown_set_selected(ui_droState, 1);
    } else {
        lv_dropdown_set_selected(ui_droState, 0);
    }

    lv_dropdown_set_selected(ui_droVerbose, config.SETTINGS.ADVANCED.VERBOSE);
}

void save_tweak_options() {
    int idx_swap = lv_dropdown_get_selected(ui_droSwap);
    int idx_thermal = lv_dropdown_get_selected(ui_droThermal);
    int idx_font = lv_dropdown_get_selected(ui_droFont);
    int idx_offset = lv_dropdown_get_selected(ui_droOffset);
    int idx_lockdown = lv_dropdown_get_selected(ui_droPasscode);
    int idx_led = lv_dropdown_get_selected(ui_droLED);
    int idx_random_theme = lv_dropdown_get_selected(ui_droTheme);
    int idx_retrowait = lv_dropdown_get_selected(ui_droRetroWait);
    int idx_android = lv_dropdown_get_selected(ui_droAndroid);
    int idx_verbose = lv_dropdown_get_selected(ui_droVerbose);

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
        case 0:
            idx_state = "mem";
            break;
        case 1:
            idx_state = "freeze";
            break;
        default:
            idx_state = "mem";
            break;
    }

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
    write_text_to_file("/run/muos/global/settings/advanced/android", "w", INT, idx_android);
    write_text_to_file("/run/muos/global/settings/advanced/state", "w", CHAR, idx_state);
    write_text_to_file("/run/muos/global/settings/advanced/verbose", "w", INT, idx_verbose);

    static char tweak_script[MAX_BUFFER_SIZE];
    snprintf(tweak_script, sizeof(tweak_script),
             "%s/script/mux/tweak.sh", INTERNAL_PATH);
    system(tweak_script);
}

void init_navigation_groups() {
    ui_objects[0] = ui_lblSwap;
    ui_objects[1] = ui_lblThermal;
    ui_objects[2] = ui_lblFont;
    ui_objects[3] = ui_lblVolume;
    ui_objects[4] = ui_lblBrightness;
    ui_objects[5] = ui_lblOffset;
    ui_objects[6] = ui_lblPasscode;
    ui_objects[7] = ui_lblLED;
    ui_objects[8] = ui_lblTheme;
    ui_objects[9] = ui_lblRetroWait;
    ui_objects[10] = ui_lblAndroid;
    ui_objects[11] = ui_lblState;
    ui_objects[12] = ui_lblVerbose;

    lv_obj_t *ui_objects_value[] = {
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
            ui_droAndroid,
            ui_droState,
            ui_droVerbose
    };

    lv_obj_t *ui_objects_icon[] = {
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
            ui_icoAndroid,
            ui_icoState,
            ui_icoVerbose
    };

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
    apply_theme_list_panel(&theme, &device, ui_pnlAndroid);
    apply_theme_list_panel(&theme, &device, ui_pnlState);
    apply_theme_list_panel(&theme, &device, ui_pnlVerbose);

    apply_theme_list_item(&theme, ui_lblSwap, "A+B Button Swap", false, true);
    apply_theme_list_item(&theme, ui_lblThermal, "Thermal Zone Control", false, true);
    apply_theme_list_item(&theme, ui_lblFont, "Interface Font Type", false, true);
    apply_theme_list_item(&theme, ui_lblVolume, "Volume On Boot", false, true);
    apply_theme_list_item(&theme, ui_lblBrightness, "Brightness On Boot", false, true);
    apply_theme_list_item(&theme, ui_lblOffset, "Battery Offset", false, true);
    apply_theme_list_item(&theme, ui_lblPasscode, "Passcode Lock", false, true);
    apply_theme_list_item(&theme, ui_lblLED, "LED During Play", false, true);
    apply_theme_list_item(&theme, ui_lblTheme, "Random Theme on Boot", false, true);
    apply_theme_list_item(&theme, ui_lblRetroWait, "RetroArch Network Wait", false, true);
    apply_theme_list_item(&theme, ui_lblAndroid, "Android Debug Bridge", false, true);
    apply_theme_list_item(&theme, ui_lblState, "Suspend Power State", false, true);
    apply_theme_list_item(&theme, ui_lblVerbose, "Verbose Messages", false, true);

    apply_theme_list_glyph(&theme, &device, ui_icoSwap, mux_prog, "swap");
    apply_theme_list_glyph(&theme, &device, ui_icoThermal, mux_prog, "thermal");
    apply_theme_list_glyph(&theme, &device, ui_icoFont, mux_prog, "font");
    apply_theme_list_glyph(&theme, &device, ui_icoVolume, mux_prog, "volume");
    apply_theme_list_glyph(&theme, &device, ui_icoBrightness, mux_prog, "brightness");
    apply_theme_list_glyph(&theme, &device, ui_icoOffset, mux_prog, "offset");
    apply_theme_list_glyph(&theme, &device, ui_icoPasscode, mux_prog, "lock");
    apply_theme_list_glyph(&theme, &device, ui_icoLED, mux_prog, "led");
    apply_theme_list_glyph(&theme, &device, ui_icoTheme, mux_prog, "theme");
    apply_theme_list_glyph(&theme, &device, ui_icoRetroWait, mux_prog, "retrowait");
    apply_theme_list_glyph(&theme, &device, ui_icoAndroid, mux_prog, "android");
    apply_theme_list_glyph(&theme, &device, ui_icoState, mux_prog, "state");
    apply_theme_list_glyph(&theme, &device, ui_icoVerbose, mux_prog, "verbose");

    apply_theme_list_drop_down(&theme, ui_droSwap, "Disabled\nEnabled");
    apply_theme_list_drop_down(&theme, ui_droThermal, "Disabled\nEnabled");
    apply_theme_list_drop_down(&theme, ui_droFont, "Noto Sans\nTheme Controlled");
    apply_theme_list_drop_down(&theme, ui_droVolume, "Previous\nQuiet\nLoud");
    apply_theme_list_drop_down(&theme, ui_droBrightness, "Previous\nLow\nHigh");
    apply_theme_list_drop_down(&theme, ui_droOffset,
                               "-50\n-49\n-48\n-47\n-46\n-45\n-44\n-43\n-42\n-41\n-40\n-39\n-38\n-37\n-36\n-35\n-34\n-33\n-32\n"
                               "-31\n-30\n-29\n-28\n-27\n-26\n-25\n-24\n-23\n-22\n-21\n-20\n-19\n-18\n-17\n-16\n-15\n-14\n-13\n"
                               "-12\n-11\n-10\n-9\n-8\n-7\n-6\n-5\n-4\n-3\n-2\n-1\n0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n"
                               "14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n"
                               "37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50");
    apply_theme_list_drop_down(&theme, ui_droPasscode, "Disabled\nEnabled");
    apply_theme_list_drop_down(&theme, ui_droLED, "Disabled\nEnabled");
    apply_theme_list_drop_down(&theme, ui_droTheme, "Disabled\nEnabled");
    apply_theme_list_drop_down(&theme, ui_droRetroWait, "Disabled\nEnabled");
    apply_theme_list_drop_down(&theme, ui_droAndroid, "Disabled\nEnabled");
    apply_theme_list_drop_down(&theme, ui_droState, "mem\nfreeze");
    apply_theme_list_drop_down(&theme, ui_droVerbose, "Disabled\nEnabled");

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();

    for (unsigned int i = 0; i < sizeof(ui_objects) / sizeof(ui_objects[0]); i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_icon[i]);
    }
}

void list_nav_prev(int steps) {
    for (int step = 0; step < steps; ++step) {
        if (current_item_index > 0) {
            current_item_index--;
            nav_prev(ui_group, 1);
            nav_prev(ui_group_value, 1);
            nav_prev(ui_group_glyph, 1);
        }
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, UI_COUNT, current_item_index, ui_pnlContent);
    play_sound("navigate", nav_sound, 0);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    for (int step = 0; step < steps; ++step) {
        if (current_item_index < (UI_COUNT - 1)) {
            current_item_index++;
            nav_next(ui_group, 1);
            nav_next(ui_group_value, 1);
            nav_next(ui_group_glyph, 1);
        }
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, UI_COUNT, current_item_index, ui_pnlContent);
    play_sound("navigate", nav_sound, 0);
    nav_moved = 1;
}

void *joystick_task() {
    struct input_event ev;
    int epoll_fd;
    struct epoll_event event, events[device.DEVICE.EVENT];

    int JOYHOTKEY_pressed = 0;

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Error creating EPOLL instance");
        return NULL;
    }

    event.events = EPOLLIN;
    event.data.fd = js_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, js_fd, &event) == -1) {
        perror("Error with EPOLL controller");
        return NULL;
    }

    while (1) {
        int num_events = epoll_wait(epoll_fd, events, device.DEVICE.EVENT, 64);
        if (num_events == -1) {
            perror("Error with EPOLL wait event timer");
            continue;
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == js_fd) {
                int ret = read(js_fd, &ev, sizeof(struct input_event));
                if (ret == -1) {
                    perror("Error reading input");
                    continue;
                }

                struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
                switch (ev.type) {
                    case EV_KEY:
                        if (ev.value == 1) {
                            if (msgbox_active) {
                                if (ev.code == NAV_B || ev.code == device.RAW_INPUT.BUTTON.MENU_SHORT) {
                                    play_sound("confirm", nav_sound, 1);
                                    msgbox_active = 0;
                                    progress_onscreen = 0;
                                    lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
                                }
                            } else {
                                if (ev.code == device.RAW_INPUT.BUTTON.MENU_LONG) {
                                    JOYHOTKEY_pressed = 1;
                                } else if (ev.code == NAV_A) {
                                    if (element_focused == ui_lblSwap) {
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
                                    } else if (element_focused == ui_lblAndroid) {
                                        increase_option_value(ui_droAndroid,
                                                              &android_current,
                                                              android_total);
                                    } else if (element_focused == ui_lblState) {
                                        increase_option_value(ui_droState,
                                                              &state_current,
                                                              state_total);
                                    } else if (element_focused == ui_lblVerbose) {
                                        increase_option_value(ui_droVerbose,
                                                              &verbose_current,
                                                              verbose_total);
                                    }
                                    play_sound("navigate", nav_sound, 0);
                                } else if (ev.code == NAV_B) {
                                    play_sound("back", nav_sound, 1);
                                    input_disable = 1;

                                    osd_message = "Saving Changes";
                                    lv_label_set_text(ui_lblMessage, osd_message);
                                    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);

                                    save_tweak_options();

                                    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "advanced");
                                    safe_quit = 1;
                                }
                            }
                        } else {
                            if (ev.code == device.RAW_INPUT.BUTTON.MENU_SHORT ||
                                ev.code == device.RAW_INPUT.BUTTON.MENU_LONG) {
                                JOYHOTKEY_pressed = 0;
                                /* DISABLED HELP SCREEN TEMPORARILY
                                if (progress_onscreen == -1) {
                                    play_sound("confirm", nav_sound, 1);
                                    show_help(element_focused);
                                }
                                */
                            }
                        }
                    case EV_ABS:
                        if (msgbox_active) {
                            break;
                        }
                        if (ev.code == NAV_DPAD_VER || ev.code == NAV_ANLG_VER) {
                            if ((ev.value >= ((device.INPUT.AXIS_MAX >> 2) * -1) &&
                                 ev.value <= ((device.INPUT.AXIS_MIN >> 2) * -1)) ||
                                ev.value == -1) {
                                if (current_item_index == 0) {
                                    current_item_index = UI_COUNT - 1;
                                    nav_prev(ui_group, 1);
                                    nav_prev(ui_group_value, 1);
                                    nav_prev(ui_group_glyph, 1);
                                    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL,
                                                           UI_COUNT, current_item_index, ui_pnlContent);
                                    nav_moved = 1;
                                } else if (current_item_index > 0) {
                                    list_nav_prev(1);
                                    nav_moved = 1;
                                }
                            } else if ((ev.value >= (device.INPUT.AXIS_MIN >> 2) &&
                                        ev.value <= (device.INPUT.AXIS_MAX >> 2)) ||
                                       ev.value == 1) {
                                if (current_item_index == UI_COUNT - 1) {
                                    current_item_index = 0;
                                    nav_next(ui_group, 1);
                                    nav_next(ui_group_value, 1);
                                    nav_next(ui_group_glyph, 1);
                                    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL,
                                                           UI_COUNT, current_item_index, ui_pnlContent);
                                    nav_moved = 1;
                                } else if (current_item_index < UI_COUNT - 1) {
                                    list_nav_next(1);
                                    nav_moved = 1;
                                }
                            }
                        } else if (ev.code == NAV_DPAD_HOR || ev.code == NAV_ANLG_HOR) {
                            if ((ev.value >= ((device.INPUT.AXIS_MAX >> 2) * -1) &&
                                 ev.value <= ((device.INPUT.AXIS_MIN >> 2) * -1)) ||
                                ev.value == -1) {
                                if (element_focused == ui_lblSwap) {
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
                                } else if (element_focused == ui_lblAndroid) {
                                    decrease_option_value(ui_droAndroid,
                                                          &android_current,
                                                          android_total);
                                } else if (element_focused == ui_lblState) {
                                    decrease_option_value(ui_droState,
                                                          &state_current,
                                                          state_total);
                                } else if (element_focused == ui_lblVerbose) {
                                    decrease_option_value(ui_droVerbose,
                                                          &verbose_current,
                                                          verbose_total);
                                }
                                play_sound("navigate", nav_sound, 0);
                            } else if ((ev.value >= (device.INPUT.AXIS_MIN >> 2) &&
                                        ev.value <= (device.INPUT.AXIS_MAX >> 2)) ||
                                       ev.value == 1) {
                                if (element_focused == ui_lblSwap) {
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
                                } else if (element_focused == ui_lblAndroid) {
                                    increase_option_value(ui_droAndroid,
                                                          &android_current,
                                                          android_total);
                                } else if (element_focused == ui_lblState) {
                                    increase_option_value(ui_droState,
                                                          &state_current,
                                                          state_total);
                                } else if (element_focused == ui_lblVerbose) {
                                    increase_option_value(ui_droVerbose,
                                                          &verbose_current,
                                                          verbose_total);
                                }
                                play_sound("navigate", nav_sound, 0);
                            }
                        }
                    default:
                        break;
                }
            }
        }

        if (!atoi(read_line_from_file("/tmp/hdmi_in_use", 1))) {
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

        lv_task_handler();
        usleep(device.SCREEN.WAIT);
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

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, osd_message);

    lv_label_set_text(ui_lblNavB, "Save");

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
    lv_obj_set_user_data(ui_lblAndroid, "android");
    lv_obj_set_user_data(ui_lblState, "state");
    lv_obj_set_user_data(ui_lblVerbose, "verbose");

    if (!device.DEVICE.HAS_NETWORK) {
        lv_obj_add_flag(ui_lblRetroWait, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblRetroWait, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_icoRetroWait, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_icoRetroWait, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_droRetroWait, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_droRetroWait, LV_OBJ_FLAG_FLOATING);
    }

    char *overlay = load_overlay_image();
    if (strlen(overlay) > 0 && theme.MISC.IMAGE_OVERLAY) {
        lv_obj_t * overlay_img = lv_img_create(ui_scrTweakAdvanced);
        lv_img_set_src(overlay_img, overlay);
        lv_obj_move_foreground(overlay_img);
    }

    if (TEST_IMAGE) display_testing_message(ui_scrTweakAdvanced);
}

void glyph_task() {
    // TODO: Bluetooth connectivity!

    if (device.DEVICE.HAS_NETWORK && is_network_connected()) {
        lv_obj_set_style_text_color(ui_staNetwork, lv_color_hex(theme.STATUS.NETWORK.ACTIVE),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staNetwork, theme.STATUS.NETWORK.ACTIVE_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_text_color(ui_staNetwork, lv_color_hex(theme.STATUS.NETWORK.NORMAL),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staNetwork, theme.STATUS.NETWORK.NORMAL_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (atoi(read_text_from_file(device.BATTERY.CHARGER))) {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.STATUS.BATTERY.ACTIVE),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.STATUS.BATTERY.ACTIVE_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (read_battery_capacity() <= 15) {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.STATUS.BATTERY.LOW),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.STATUS.BATTERY.LOW_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.STATUS.BATTERY.NORMAL),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.STATUS.BATTERY.NORMAL_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

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
    lv_bar_set_value(ui_barProgressBrightness, atoi(read_text_from_file(BRIGHT_PERC)), LV_ANIM_OFF);
    lv_bar_set_value(ui_barProgressVolume, atoi(read_text_from_file(VOLUME_PERC)), LV_ANIM_OFF);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            static char old_wall[MAX_BUFFER_SIZE];
            static char new_wall[MAX_BUFFER_SIZE];

            snprintf(old_wall, sizeof(old_wall), "%s", current_wall);
            snprintf(new_wall, sizeof(new_wall), "%s", load_wallpaper(
                    ui_scrTweakAdvanced, ui_group, theme.MISC.ANIMATED_BACKGROUND));

            if (strcasecmp(new_wall, old_wall) != 0) {
                strcpy(current_wall, new_wall);
                if (strlen(new_wall) > 3) {
                    printf("LOADING WALLPAPER: %s\n", new_wall);
                    if (theme.MISC.ANIMATED_BACKGROUND) {
                        lv_obj_t * img = lv_gif_create(ui_pnlWall);
                        lv_gif_set_src(img, new_wall);
                    } else {
                        lv_img_set_src(ui_imgWall, new_wall);
                    }
                    lv_obj_invalidate(ui_pnlWall);
                } else {
                    lv_img_set_src(ui_imgWall, &ui_img_nothing_png);
                }
            }

            static char static_image[MAX_BUFFER_SIZE];
            snprintf(static_image, sizeof(static_image), "%s",
                     load_static_image(ui_scrTweakAdvanced, ui_group));

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
                lv_img_set_src(ui_imgBox, &ui_img_nothing_png);
            }
        }
        lv_obj_invalidate(ui_pnlContent);
        lv_task_handler();
        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    mux_prog = basename(argv[0]);
    load_device(&device);
    srand(time(NULL));

    lv_init();
    fbdev_init(device.SCREEN.DEVICE);

    static lv_disp_draw_buf_t disp_buf;
    uint32_t disp_buf_size = device.SCREEN.BUFFER;

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
    lv_disp_drv_register(&disp_drv);

    load_config(&config);

    ui_init();
    init_elements();

    lv_obj_set_user_data(ui_scrTweakAdvanced, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());
    lv_label_set_text(ui_staCapacity, get_capacity());

    load_theme(&theme, &config, &device, basename(argv[0]));
    apply_theme();

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
            lv_label_set_text(ui_lblNavAGlyph, "\u21D2");
            lv_label_set_text(ui_lblNavBGlyph, "\u21D3");
            break;
        default:
            NAV_A = device.RAW_INPUT.BUTTON.A;
            NAV_B = device.RAW_INPUT.BUTTON.B;
            lv_label_set_text(ui_lblNavAGlyph, "\u21D3");
            lv_label_set_text(ui_lblNavBGlyph, "\u21D2");
            break;
    }

    current_wall = load_wallpaper(ui_scrTweakAdvanced, NULL, theme.MISC.ANIMATED_BACKGROUND);
    if (strlen(current_wall) > 3) {
        if (theme.MISC.ANIMATED_BACKGROUND) {
            lv_obj_t * img = lv_gif_create(ui_pnlWall);
            lv_gif_set_src(img, current_wall);
        } else {
            lv_img_set_src(ui_imgWall, current_wall);
        }
    } else {
        lv_img_set_src(ui_imgWall, &ui_img_nothing_png);
    }

    load_font_text(basename(argv[0]), ui_scrTweakAdvanced);
    load_font_section(basename(argv[0]), FONT_PANEL_FOLDER, ui_pnlContent);

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

    pthread_t joystick_thread;
    pthread_create(&joystick_thread, NULL, (void *(*)(void *)) joystick_task, NULL);

    while (!safe_quit) {
        usleep(device.SCREEN.WAIT);
    }

    pthread_cancel(joystick_thread);

    close(js_fd);

    return 0;
}

uint32_t mux_tick(void) {
    static uint64_t start_ms = 0;

    if (start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}
