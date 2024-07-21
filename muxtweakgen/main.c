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
#include "../common/common.h"
#include "../common/help.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/glyph.h"
#include "../common/mini/mini.h"

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

lv_obj_t *msgbox_element = NULL;

int progress_onscreen = -1;

int hidden_total, hidden_current;
int bgm_total, bgm_current;
int startup_total, startup_current;
int colour_total, colour_current;
int brightness_total, brightness_current;
int hdmi_total, hdmi_current;
int shutdown_total, shutdown_current;

typedef struct {
    int *total;
    int *current;
} Tweak;

Tweak hidden, bgm, startup, colour, brightness, hdmi, shutdown;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;

lv_obj_t *ui_objects[10];

void show_help(lv_obj_t *element_focused) {
    char *message = NO_HELP_FOUND;

    if (element_focused == ui_lblHidden) {
        message = MUXTWEAKGEN_HIDDEN;
    } else if (element_focused == ui_lblBGM) {
        message = MUXTWEAKGEN_BGM;
    } else if (element_focused == ui_lblStartup) {
        message = MUXTWEAKGEN_STARTUP;
    } else if (element_focused == ui_lblColour) {
        message = MUXTWEAKGEN_COLOUR;
    } else if (element_focused == ui_lblBrightness) {
        message = MUXTWEAKGEN_BRIGHTNESS;
    } else if (element_focused == ui_lblHDMI) {
        message = MUXTWEAKGEN_HDMI;
    } else if (element_focused == ui_lblShutdown) {
        message = MUXTWEAKGEN_SHUTDOWN;
    } else if (element_focused == ui_lblInterface) {
        message = MUXTWEAKGEN_INTERFACE;
    } else if (element_focused == ui_lblStorage) {
        message = MUXTWEAKGEN_STORAGE;
    } else if (element_focused == ui_lblAdvanced) {
        message = MUXTWEAKGEN_ADVANCED;
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
            ui_droHidden,
            ui_droBGM,
            ui_droStartup,
            ui_droColour,
            ui_droBrightness,
            ui_droHDMI,
            ui_droShutdown
    };

    for (unsigned int i = 0; i < sizeof(dropdowns) / sizeof(dropdowns[0]); i++) {
        lv_obj_add_event_cb(dropdowns[i], dropdown_event_handler, LV_EVENT_ALL, NULL);
    }

    init_pointers(&hidden, &hidden_total, &hidden_current);
    init_pointers(&bgm, &bgm_total, &bgm_current);
    init_pointers(&startup, &startup_total, &startup_current);
    init_pointers(&colour, &colour_total, &colour_current);
    init_pointers(&brightness, &brightness_total, &brightness_current);
    init_pointers(&hdmi, &hdmi_total, &hdmi_current);
    init_pointers(&shutdown, &shutdown_total, &shutdown_current);
}

void init_dropdown_settings() {
    Tweak settings[] = {
            {hidden.total,     hidden.current},
            {bgm.total,        bgm.current},
            {startup.total,    startup.current},
            {colour.total,     colour.current},
            {brightness.total, brightness.current},
            {hdmi.total,       hdmi.current},
            {shutdown.total,   shutdown.current}
    };

    lv_obj_t *dropdowns[] = {
            ui_droHidden,
            ui_droBGM,
            ui_droStartup,
            ui_droColour,
            ui_droBrightness,
            ui_droHDMI,
            ui_droShutdown
    };

    for (unsigned int i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
        *(settings[i].total) = lv_dropdown_get_option_cnt(dropdowns[i]);
        *(settings[i].current) = lv_dropdown_get_selected(dropdowns[i]);
    }
}

void restore_tweak_options() {
    lv_dropdown_set_selected(ui_droHidden, config.SETTINGS.GENERAL.HIDDEN);
    lv_dropdown_set_selected(ui_droBGM, config.SETTINGS.GENERAL.BGM);
    lv_dropdown_set_selected(ui_droBrightness, atoi(read_text_from_file(BRIGHT_FILE)));

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
    if (strcasecmp(startup_type, "launcher") == 0) {
        lv_dropdown_set_selected(ui_droStartup, 0);
    } else if (strcasecmp(startup_type, "explore") == 0) {
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
}

void save_tweak_options() {
    static char config_file[MAX_BUFFER_SIZE];
    snprintf(config_file, sizeof(config_file),
             "%s/config/config.ini", INTERNAL_PATH);

    mini_t * muos_config = mini_try_load(config_file);

    int idx_hidden = lv_dropdown_get_selected(ui_droHidden);
    int idx_bgm = lv_dropdown_get_selected(ui_droBGM);
    int idx_brightness = lv_dropdown_get_selected(ui_droBrightness);

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

    mini_set_int(muos_config, "settings.general", "hidden", idx_hidden);
    mini_set_int(muos_config, "settings.general", "bgm", idx_bgm);
    mini_set_string(muos_config, "settings.general", "startup", idx_startup);
    mini_set_int(muos_config, "settings.general", "colour", idx_colour);
    mini_set_int(muos_config, "settings.general", "hdmi", idx_hdmi);
    mini_set_int(muos_config, "settings.general", "shutdown", idx_shutdown);

    mini_save(muos_config, MINI_FLAGS_SKIP_EMPTY_GROUPS);
    mini_free(muos_config);

    char command[MAX_BUFFER_SIZE];
    snprintf(command, sizeof(command), "%s/device/%s/input/combo/bright.sh %d",
             INTERNAL_PATH, str_tolower(device.DEVICE.NAME), idx_brightness);
    system(command);

    static char tweak_script[MAX_BUFFER_SIZE];
    snprintf(tweak_script, sizeof(tweak_script),
             "%s/script/mux/tweak.sh", INTERNAL_PATH);

    system(tweak_script);
}

void init_navigation_groups() {
    ui_objects[0] = ui_lblHidden;
    ui_objects[1] = ui_lblBGM;
    ui_objects[2] = ui_lblStartup;
    ui_objects[3] = ui_lblColour;
    ui_objects[4] = ui_lblBrightness;
    ui_objects[5] = ui_lblHDMI;
    ui_objects[6] = ui_lblShutdown;
    ui_objects[7] = ui_lblInterface;
    ui_objects[8] = ui_lblStorage;
    ui_objects[9] = ui_lblAdvanced;

    lv_obj_t *ui_objects_value[] = {
            ui_droHidden,
            ui_droBGM,
            ui_droStartup,
            ui_droColour,
            ui_droBrightness,
            ui_droHDMI,
            ui_droShutdown,
            ui_droInterface,
            ui_droStorage,
            ui_droAdvanced
    };

    lv_obj_t *ui_objects_icon[] = {
            ui_icoHidden,
            ui_icoBGM,
            ui_icoStartup,
            ui_icoColour,
            ui_icoBrightness,
            ui_icoHDMI,
            ui_icoShutdown,
            ui_icoInterface,
            ui_icoStorage,
            ui_icoAdvanced
    };

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();

    for (unsigned int i = 0; i < sizeof(ui_objects) / sizeof(ui_objects[0]); i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
    }

    for (unsigned int i = 0; i < sizeof(ui_objects_value) / sizeof(ui_objects_value[0]); i++) {
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
    }

    for (unsigned int i = 0; i < sizeof(ui_objects_icon) / sizeof(ui_objects_icon[0]); i++) {
        lv_group_add_obj(ui_group_glyph, ui_objects_icon[i]);
    }
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
                                    play_sound("confirm", nav_sound);
                                    msgbox_active = 0;
                                    progress_onscreen = 0;
                                    lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
                                }
                            } else {
                                if (ev.code == device.RAW_INPUT.BUTTON.MENU_LONG) {
                                    JOYHOTKEY_pressed = 1;
                                } else if (ev.code == NAV_A) {
                                    if (element_focused == ui_lblHidden) {
                                        increase_option_value(ui_droHidden,
                                                              &hidden_current,
                                                              hidden_total);
                                    } else if (element_focused == ui_lblBGM) {
                                        increase_option_value(ui_droBGM,
                                                              &bgm_current,
                                                              bgm_total);
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
                                    } else if (element_focused == ui_lblInterface) {
                                        save_tweak_options();

                                        load_mux("visual");
                                        safe_quit = 1;
                                    } else if (element_focused == ui_lblStorage) {
                                        save_tweak_options();

                                        load_mux("storage");
                                        safe_quit = 1;
                                    } else if (element_focused == ui_lblAdvanced) {
                                        save_tweak_options();

                                        load_mux("tweakadv");
                                        safe_quit = 1;
                                    }
                                    play_sound("navigate", nav_sound);
                                } else if (ev.code == NAV_B) {
                                    play_sound("back", nav_sound);
                                    input_disable = 1;

                                    lv_label_set_text(ui_lblMessage, "Saving Changes");
                                    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);

                                    save_tweak_options();

                                    write_text_to_file(MUOS_PDI_LOAD, "general", "w");
                                    safe_quit = 1;
                                } else if (ev.code == device.RAW_INPUT.BUTTON.L1) {
                                    if (element_focused == ui_lblBrightness) {
                                        for (i = 1; i <= 10; i++) {
                                            decrease_option_value(ui_droBrightness,
                                                                  &brightness_current,
                                                                  brightness_total);
                                        }
                                    }
                                } else if (ev.code == device.RAW_INPUT.BUTTON.R1) {
                                    if (element_focused == ui_lblBrightness) {
                                        for (i = 1; i <= 10; i++) {
                                            increase_option_value(ui_droBrightness,
                                                                  &brightness_current,
                                                                  brightness_total);
                                        }
                                    }
                                }
                            }
                        } else {
                            if (ev.code == device.RAW_INPUT.BUTTON.MENU_SHORT ||
                                ev.code == device.RAW_INPUT.BUTTON.MENU_LONG) {
                                JOYHOTKEY_pressed = 0;
                                /* DISABLED HELP SCREEN TEMPORARILY
                                if (progress_onscreen == -1) {
                                    play_sound("confirm", nav_sound);
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
                                nav_prev(ui_group, 1);
                                nav_prev(ui_group_value, 1);
                                nav_prev(ui_group_glyph, 1);
                                play_sound("navigate", nav_sound);
                                nav_moved = 1;
                            } else if ((ev.value >= (device.INPUT.AXIS_MIN >> 2) &&
                                        ev.value <= (device.INPUT.AXIS_MAX >> 2)) ||
                                       ev.value == 1) {
                                nav_next(ui_group, 1);
                                nav_next(ui_group_value, 1);
                                nav_next(ui_group_glyph, 1);
                                play_sound("navigate", nav_sound);
                                nav_moved = 1;
                            }
                        } else if (ev.code == NAV_DPAD_HOR || ev.code == NAV_ANLG_HOR) {
                            if ((ev.value >= ((device.INPUT.AXIS_MAX >> 2) * -1) &&
                                 ev.value <= ((device.INPUT.AXIS_MIN >> 2) * -1)) ||
                                ev.value == -1) {
                                if (element_focused == ui_lblHidden) {
                                    decrease_option_value(ui_droHidden,
                                                          &hidden_current,
                                                          hidden_total);
                                } else if (element_focused == ui_lblBGM) {
                                    decrease_option_value(ui_droBGM,
                                                          &bgm_current,
                                                          bgm_total);
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
                                }
                                play_sound("navigate", nav_sound);
                            } else if ((ev.value >= (device.INPUT.AXIS_MIN >> 2) &&
                                        ev.value <= (device.INPUT.AXIS_MAX >> 2)) ||
                                       ev.value == 1) {
                                if (element_focused == ui_lblHidden) {
                                    decrease_option_value(ui_droHidden,
                                                          &hidden_current,
                                                          hidden_total);
                                } else if (element_focused == ui_lblBGM) {
                                    increase_option_value(ui_droBGM,
                                                          &bgm_current,
                                                          bgm_total);
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
                                }
                                play_sound("navigate", nav_sound);
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

void populate_brightness() {
    int max_value = device.SCREEN.BRIGHT;
    int max_digits = snprintf(NULL, 0, "%d", max_value);
    int options_length = max_value * (max_digits + 1);

    char *options = (char *) malloc(options_length * sizeof(char));
    if (options == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }

    char *ptr = options;
    for (int i = 1; i <= max_value; ++i) {
        int chars_written = snprintf(ptr, options + options_length - ptr, "%d", i);
        ptr += chars_written;
        if (i < max_value) {
            *ptr++ = '\n';
        }
    }
    *ptr = '\0';

    lv_dropdown_set_options(ui_droBrightness, options);
    lv_dropdown_set_selected(ui_droBrightness, atoi(read_text_from_file(BRIGHT_FILE)));

    free(options);
}

void init_elements() {
    populate_brightness();

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

    lv_obj_set_user_data(ui_lblHidden, "hidden");
    lv_obj_set_user_data(ui_lblBGM, "bgm");
    lv_obj_set_user_data(ui_lblStartup, "startup");
    lv_obj_set_user_data(ui_lblColour, "colour");
    lv_obj_set_user_data(ui_lblBrightness, "brightness");
    lv_obj_set_user_data(ui_lblHDMI, "hdmi");
    lv_obj_set_user_data(ui_lblShutdown, "shutdown");
    lv_obj_set_user_data(ui_lblInterface, "interface");
    lv_obj_set_user_data(ui_lblStorage, "storage");
    lv_obj_set_user_data(ui_lblAdvanced, "advanced");

    if (!device.DEVICE.HAS_HDMI) {
        lv_obj_add_flag(ui_lblHDMI, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblHDMI, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_icoHDMI, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_icoHDMI, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_droHDMI, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_droHDMI, LV_OBJ_FLAG_FLOATING);
    }

    char *overlay = load_overlay_image();
    if (strlen(overlay) > 0 && theme.MISC.IMAGE_OVERLAY) {
        lv_obj_t * overlay_img = lv_img_create(ui_scrTweakGeneral);
        lv_img_set_src(overlay_img, overlay);
        lv_obj_move_foreground(overlay_img);
    }

    if (TEST_IMAGE) display_testing_message(ui_scrTweakGeneral);
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
                    ui_scrTweakGeneral, ui_group, theme.MISC.ANIMATED_BACKGROUND));

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
                     load_static_image(ui_scrTweakGeneral, ui_group));

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
            nav_next(ui_group, text_hit);
            nav_next(ui_group_glyph, text_hit);
            nav_next(ui_group_value, text_hit);
            nav_moved = 1;
        }
    }
}

int main(int argc, char *argv[]) {
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

    lv_obj_set_user_data(ui_scrTweakGeneral, basename(argv[0]));

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

    current_wall = load_wallpaper(ui_scrTweakGeneral, NULL, theme.MISC.ANIMATED_BACKGROUND);
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

    load_font_text(basename(argv[0]), ui_scrTweakGeneral);

    if (config.SETTINGS.GENERAL.SOUND == 2) {
        nav_sound = 1;
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

    init_elements();
    direct_to_previous();

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
