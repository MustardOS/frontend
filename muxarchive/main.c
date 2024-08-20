#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/joystick.h>
#include <string.h>
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
#include "../common/ui_common.h"
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
struct theme_config theme;

int nav_moved = 1;
char *current_wall = "";

lv_obj_t *msgbox_element = NULL;

int progress_onscreen = -1;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;

lv_group_t *ui_group_installed;
lv_group_t *ui_group_data;

int ui_count = 0;
int current_item_index = 0;
int first_open = 1;

void show_help() {
    char *title = lv_label_get_text(ui_lblTitle);
    char *message = MUXARCHIVE_GENERIC;

    if (strlen(message) <= 1) {
        message = NO_HELP_FOUND;
    }

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent, title, message);
}

void create_archive_items() {
    const char *archive_directories[] = {
            "/mnt/mmc/muos/update",
            "/mnt/mmc/backup",
            "/mnt/mmc/archive",
            "/mnt/sdcard/backup",
            "/mnt/sdcard/archive",
            "/mnt/usb/backup",
            "/mnt/usb/archive"};
    char archive_dir[MAX_BUFFER_SIZE];

    char **file_names = NULL;
    size_t file_count = 0;

    for (size_t dir_index = 0; dir_index < sizeof(archive_directories) / sizeof(archive_directories[0]); ++dir_index) {
        snprintf(archive_dir, sizeof(archive_dir), "%s/", archive_directories[dir_index]);

        DIR *ad = opendir(archive_dir);
        if (ad == NULL) {
            continue;
        }

        struct dirent *tf;
        while ((tf = readdir(ad))) {
            if (tf->d_type == DT_REG) {
                char *last_dot = strrchr(tf->d_name, '.');
                if (last_dot != NULL && strcasecmp(last_dot, ".zip") == 0) {
                    file_names = realloc(file_names, (file_count + 1) * sizeof(char *));
                    static char full_archive_name[MAX_BUFFER_SIZE];
                    snprintf(full_archive_name, sizeof(full_archive_name), "%s%s", archive_dir, tf->d_name);
                    file_names[file_count] = strdup(full_archive_name);
                    file_count++;
                }
            }
        }
        closedir(ad);
    }

    qsort(file_names, file_count, sizeof(char *), str_compare);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_installed = lv_group_create();
    ui_group_data = lv_group_create();

    for (size_t i = 0; i < file_count; i++) {
        char *base_filename = file_names[i];

        static char archive_name[MAX_BUFFER_SIZE];
        snprintf(archive_name, sizeof(archive_name), "%s",
                 str_remchar(str_replace(base_filename, strip_dir(base_filename), ""), '/'));

        static char install_check[MAX_BUFFER_SIZE];
        snprintf(install_check, sizeof(install_check), "/mnt/mmc/MUOS/update/installed/%s.done",
                 archive_name);

        char *is_installed;
        if (file_exist(install_check)) {
            is_installed = "INSTALLED";
        } else {
            is_installed = "";
        }

        char *prefix;
        if (strstr(base_filename, "/mnt/mmc/muos/update") != NULL) {
            prefix = "[MUX]";
        } else if (strstr(base_filename, "/mnt/mmc/backup") != NULL) {
            prefix = "[SD1-BK]";
        } else if (strstr(base_filename, "/mnt/mmc/") != NULL) {
            prefix = "[SD1]";
        } else if (strstr(base_filename, "/mnt/sdcard/backup") != NULL) {
            prefix = "[SD2-BK]";
        } else if (strstr(base_filename, "/mnt/sdcard/") != NULL) {
            prefix = "[SD2]";
        } else if (strstr(base_filename, "/mnt/usb/backup") != NULL) {
            prefix = "[USB-BK]";
        } else if (strstr(base_filename, "/mnt/usb/") != NULL) {
            prefix = "[USB]";
        } else {
            continue;
        }

        static char archive_store[MAX_BUFFER_SIZE];
        snprintf(archive_store, sizeof(archive_store), "%s %s", prefix, strip_ext(archive_name));

        ui_count++;

        lv_obj_t * ui_pnlArchive = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(&theme, &device, ui_pnlArchive);

        lv_obj_t * ui_lblArchiveItem = lv_label_create(ui_pnlArchive);
        apply_theme_list_item(&theme, ui_lblArchiveItem, archive_store, false, true);

        lv_obj_t * ui_lblArchiveItemInstalled = lv_label_create(ui_pnlArchive);
        apply_theme_list_value(&theme, ui_lblArchiveItemInstalled, is_installed);

        lv_obj_t * ui_lblArchiveItemData = lv_label_create(ui_pnlArchive);
        lv_label_set_text(ui_lblArchiveItemData, base_filename);
        lv_obj_set_y(ui_lblArchiveItemData, device.SCREEN.HEIGHT + 1);
        lv_obj_set_width(ui_lblArchiveItemData, 1);
        lv_obj_set_height(ui_lblArchiveItemData, 1);

        lv_obj_t * ui_lblArchiveItemGlyph = lv_img_create(ui_pnlArchive);
        char item_glyph[MAX_BUFFER_SIZE];
        snprintf(item_glyph, sizeof(item_glyph), "archive%s", (strcasecmp(is_installed, "INSTALLED") == 0) ? "installed" : "");
        apply_theme_list_glyph(&theme, &device, ui_lblArchiveItemGlyph, mux_prog, item_glyph);

        lv_group_add_obj(ui_group, ui_lblArchiveItem);
        lv_group_add_obj(ui_group_glyph, ui_lblArchiveItemGlyph);
        lv_group_add_obj(ui_group_installed, ui_lblArchiveItemInstalled);
        lv_group_add_obj(ui_group_data, ui_lblArchiveItemData);

        free(base_filename);
    }
    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
    free(file_names);
}

void list_nav_prev(int steps) {
    for (int step = 0; step < steps; ++step) {
        if (current_item_index > 0) {
            current_item_index--;
            nav_prev(ui_group, 1);
            nav_prev(ui_group_glyph, 1);
            nav_prev(ui_group_installed, 1);
            nav_prev(ui_group_data, 1);
        }
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    play_sound("navigate", nav_sound, 0);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    for (int step = 0; step < steps; ++step) {
        if (current_item_index < (ui_count - 1)) {
            current_item_index++;
            nav_next(ui_group, 1);
            nav_next(ui_group_glyph, 1);
            nav_next(ui_group_installed, 1);
            nav_next(ui_group_data, 1);
        }
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound, 0);
    }
    nav_moved = 1;
}

void *joystick_task() {
    struct input_event ev;
    int epoll_fd;
    struct epoll_event event, events[device.DEVICE.EVENT];

    int JOYUP_pressed = 0;
    int JOYDOWN_pressed = 0;
    int JOYHOTKEY_pressed = 0;

    int nav_hold = 0;
    int nav_delay = UINT8_MAX;

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
        int num_events = epoll_wait(epoll_fd, events, device.DEVICE.EVENT, nav_delay);
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

                struct _lv_obj_t *data_focused = lv_group_get_focused(ui_group_data);
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
                                    if (ui_count > 0) {
                                        play_sound("confirm", nav_sound, 1);

                                        static char extract_script[MAX_BUFFER_SIZE];
                                        snprintf(extract_script, sizeof(extract_script),
                                                 "%s/script/mux/extract.sh", INTERNAL_PATH);

                                        static char command[MAX_BUFFER_SIZE];
                                        snprintf(command, sizeof(command), "/opt/muos/bin/fbpad %s \"%s\"",
                                                 extract_script, lv_label_get_text(data_focused));
                                        setenv("TERM", "xterm-256color", 1);
                                        printf("RUNNING: %s\n", command);
                                        system(command);

                                        write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

                                        load_mux("archive");
                                        safe_quit = 1;
                                    }
                                } else if (ev.code == NAV_B) {
                                    play_sound("back", nav_sound, 1);
                                    safe_quit = 1;
                                } else if (ev.code == device.RAW_INPUT.BUTTON.L1) {
                                    if (current_item_index >= 0 && current_item_index < ui_count) {
                                        list_nav_prev(theme.MUX.ITEM.COUNT);
                                        lv_task_handler();
                                    }
                                } else if (ev.code == device.RAW_INPUT.BUTTON.R1) {
                                    if (current_item_index >= 0 && current_item_index < ui_count) {
                                        list_nav_next(theme.MUX.ITEM.COUNT);
                                        lv_task_handler();
                                    }
                                }
                            }
                        } else {
                            if (ev.code == device.RAW_INPUT.BUTTON.MENU_SHORT ||
                                ev.code == device.RAW_INPUT.BUTTON.MENU_LONG) {
                                JOYHOTKEY_pressed = 0;
                                /* DISABLED HELP SCREEN TEMPORARILY
                                if (progress_onscreen == -1) {
                                    play_sound("confirm", nav_sound, 1);
                                    show_help();
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
                                    current_item_index = ui_count - 1;
                                    nav_prev(ui_group, 1);
                                    nav_prev(ui_group_glyph, 1);
                                    nav_prev(ui_group_installed, 1);
                                    nav_prev(ui_group_data, 1);
                                    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
                                    lv_task_handler();
                                } else if (current_item_index > 0) {
                                    JOYUP_pressed = (ev.value != 0);
                                    list_nav_prev(1);
                                    lv_task_handler();
                                }
                            } else if ((ev.value >= (device.INPUT.AXIS_MIN >> 2) &&
                                        ev.value <= (device.INPUT.AXIS_MAX >> 2)) ||
                                       ev.value == 1) {
                                if (current_item_index == ui_count - 1) {
                                    current_item_index = 0;
                                    nav_next(ui_group, 1);
                                    nav_next(ui_group_glyph, 1);
                                    nav_next(ui_group_installed, 1);
                                    nav_next(ui_group_data, 1);
                                    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
                                    lv_task_handler();
                                } else if (current_item_index < ui_count) {
                                    JOYDOWN_pressed = (ev.value != 0);
                                    list_nav_next(1);
                                    lv_task_handler();
                                }
                            } else {
                                JOYUP_pressed = 0;
                                JOYDOWN_pressed = 0;
                            }
                        }
                    default:
                        break;
                }
            }
        }

        if (ui_count > theme.MUX.ITEM.COUNT && (JOYUP_pressed || JOYDOWN_pressed)) {
            if (nav_hold > 2) {
                if (nav_delay > 16) {
                    nav_delay -= 16;
                }
                if (JOYUP_pressed && current_item_index > 0) {
                    list_nav_prev(1);
                }
                if (JOYDOWN_pressed && current_item_index < ui_count) {
                    list_nav_next(1);
                }
            }
            nav_hold++;
        } else {
            nav_delay = UINT8_MAX;
            nav_hold = 0;
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

    lv_label_set_text(ui_lblNavA, "Extract");
    lv_label_set_text(ui_lblNavB, "Back");

    lv_obj_t *nav_hide[] = {
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
    }

    if (ui_count == 0) {
        lv_obj_add_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblNavA, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_FLOATING);
    } else {
        lv_obj_clear_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblNavA, LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_FLOATING);
    }

    char *overlay = load_overlay_image();
    if (strlen(overlay) > 0 && theme.MISC.IMAGE_OVERLAY) {
        lv_obj_t * overlay_img = lv_img_create(ui_screen);
        lv_img_set_src(overlay_img, overlay);
        lv_obj_move_foreground(overlay_img);
    }

    if (TEST_IMAGE) display_testing_message(ui_screen);
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
                    ui_screen, ui_group, theme.MISC.ANIMATED_BACKGROUND));

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
                lv_img_set_src(ui_imgBox, &ui_img_nothing_png);
            }
        }
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
    load_theme(&theme, &config, &device, basename(argv[0]));

    ui_common_screen_init(&theme, &device);
    init_elements();

    lv_obj_set_user_data(ui_screen, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());
    lv_label_set_text(ui_staCapacity, get_capacity());

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

    current_wall = load_wallpaper(ui_screen, NULL, theme.MISC.ANIMATED_BACKGROUND);
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

    create_archive_items();

    int sys_index = 0;
    if (file_exist(MUOS_IDX_LOAD)) {
        sys_index = atoi(read_line_from_file(MUOS_IDX_LOAD, 1));
        remove(MUOS_IDX_LOAD);
    }

    struct dt_task_param dt_par;
    struct bat_task_param bat_par;

    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;

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

    lv_timer_t *glyph_timer = lv_timer_create(glyph_task, UINT16_MAX / 64, NULL);
    lv_timer_ready(glyph_timer);

    lv_timer_t *ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
    lv_timer_ready(ui_refresh_timer);

    pthread_t joystick_thread;
    pthread_create(&joystick_thread, NULL, (void *(*)(void *)) joystick_task, NULL);

    if (ui_count > 0) {
        if (sys_index > -1 && sys_index <= ui_count && current_item_index < ui_count) {
            list_nav_next(sys_index);
        }
    } else {
        lv_label_set_text(ui_lblScreenMessage, "No Archives Found");
        lv_obj_clear_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
    }

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
