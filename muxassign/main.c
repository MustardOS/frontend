#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>
#include <linux/joystick.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <libgen.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "../common/log.h"
#include "../common/common.h"
#include "../common/help.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/json/json.h"
#include "../common/mini/mini.h"

__thread uint64_t start_ms = 0;

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
lv_group_t *ui_group_panel;

int ui_count = 0;
int current_item_index = 0;
int first_open = 1;

char *auto_assign;
char *rom_name;
char *rom_dir;
char *rom_system;

enum core_gen_type {
    SINGLE,
    DIRECTORY,
    PARENT,
    DIRECTORY_NO_WIPE
} gen_method;

void show_help() {
    char *title = _("ASSIGN CORE");
    char *message = MUXASSIGN_GENERIC;

    if (strlen(message) <= 1) {
        message = NO_HELP_FOUND;
    }

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent, title, message);
}

char **get_subdirectories(const char *base_dir) {
    struct dirent *entry;
    DIR *dir = opendir(base_dir);
    char **dir_names = NULL;
    int dir_count = 0;

    if (!dir) {
        perror("opendir");
        return NULL;
    }

    char skip_ini[MAX_BUFFER_SIZE];
    snprintf(skip_ini, sizeof(skip_ini), "%s/MUOS/info/skip.ini", device.STORAGE.ROM.MOUNT);
    load_skip_patterns(skip_ini);

    while ((entry = readdir(dir)) != NULL) {
        if (!should_skip(entry->d_name)) {
            if (entry->d_type == DT_DIR) {
                if (strcasecmp(entry->d_name, ".") != 0 && strcasecmp(entry->d_name, "..") != 0) {
                    char *subdir_name = (char *) malloc(strlen(entry->d_name) + 1);
                    if (!subdir_name) {
                        perror("malloc");
                        closedir(dir);
                        return NULL;
                    }
                    snprintf(subdir_name, strlen(entry->d_name) + 1, "%s", entry->d_name);

                    dir_names = (char **) realloc(dir_names, (dir_count + 1) * sizeof(char *));
                    if (!dir_names) {
                        perror("realloc");
                        closedir(dir);
                        return NULL;
                    }
                    dir_names[dir_count] = subdir_name;
                    dir_count++;
                }
            }
        }
    }

    dir_names = (char **) realloc(dir_names, (dir_count + 1) * sizeof(char *));
    if (!dir_names) {
        perror("realloc");
        closedir(dir);
        return NULL;
    }
    dir_names[dir_count] = NULL;

    closedir(dir);
    return dir_names;
}

void free_subdirectories(char **dir_names) {
    if (dir_names == NULL) return;

    for (int i = 0; dir_names[i] != NULL; i++) {
        free(dir_names[i]);
    }
    free(dir_names);
}

void create_core_assignment(const char *core, char *sys, char *rom, int cache, enum core_gen_type method) {
    char core_dir[MAX_BUFFER_SIZE];
    snprintf(core_dir, sizeof(core_dir), "%s/info/core/%s/",
             STORAGE_PATH, get_last_subdir(rom_dir, '/', 4));

    create_directories(core_dir);

    switch (method) {
        case SINGLE:
            char rom_path[MAX_BUFFER_SIZE];
            snprintf(rom_path, sizeof(rom_path), "%s/%s.cfg",
                     core_dir, strip_ext(rom));

            if (file_exist(rom_path)) remove(rom_path);

            FILE * rom_file = fopen(rom_path, "w");
            if (rom_file == NULL) {
                perror("Error opening file rom_path file");
                return;
            }

            LOG_INFO(mux_prog, "Single Assign Content:\n\t%s\n\t%s\n\t%s\n\t%d\n\t%s\n\t%s\n\t%s",
                     strip_ext(rom), core, str_trim(sys), cache,
                     str_replace(rom_dir, get_last_subdir(rom_dir, '/', 4), ""),
                     get_last_subdir(rom_dir, '/', 4), rom);
            fprintf(rom_file, "%s\n%s\n%s\n%d\n%s\n%s\n%s\n",
                    strip_ext(rom), core, str_trim(sys), cache,
                    str_replace(rom_dir, get_last_subdir(rom_dir, '/', 4), ""),
                    get_last_subdir(rom_dir, '/', 4), rom);
            fclose(rom_file);
            break;
        case PARENT: {
            delete_files_of_type(core_dir, "cfg", NULL, 1);

            create_core_assignment(core, sys, rom, cache, DIRECTORY);

            char **subdirs = get_subdirectories(core_dir);
            if (subdirs != NULL) {
                for (int i = 0; subdirs[i] != NULL; i++) {
                    char subdir_file[MAX_BUFFER_SIZE];
                    snprintf(subdir_file, sizeof(subdir_file), "%s%s/core.cfg", core_dir, subdirs[i]);

                    create_directories(strip_dir(subdir_file));

                    FILE * subdir_file_handle = fopen(subdir_file, "w");
                    if (subdir_file_handle == NULL) {
                        perror("Error opening file");
                        continue;
                    }

                    fprintf(subdir_file_handle, "%s\n%s\n%d\n", core, str_trim(sys), cache);
                    fclose(subdir_file_handle);
                }
                free_subdirectories(subdirs);
            }
        }
            break;
        case DIRECTORY: {
            delete_files_of_type(core_dir, "cfg", NULL, 0);

            char core_file[MAX_BUFFER_SIZE];
            snprintf(core_file, sizeof(core_file), "%s/info/core/%s/core.cfg",
                     STORAGE_PATH, get_last_subdir(rom_dir, '/', 4));

            FILE * file = fopen(core_file, "w");
            if (file == NULL) {
                perror("Error opening file");
                return;
            }

            fprintf(file, "%s\n%s\n%d\n", core, str_trim(sys), cache);
            fclose(file);
        }
        case DIRECTORY_NO_WIPE:
        default: {
            char core_file[MAX_BUFFER_SIZE];
            snprintf(core_file, sizeof(core_file), "%s/info/core/%s/core.cfg",
                     STORAGE_PATH, get_last_subdir(rom_dir, '/', 4));

            FILE * file = fopen(core_file, "w");
            if (file == NULL) {
                perror("Error opening file");
                return;
            }

            fprintf(file, "%s\n%s\n%d\n", core, str_trim(sys), cache);
            fclose(file);
        }
            break;
    }

    char pico8_splore[MAX_BUFFER_SIZE];
    snprintf(pico8_splore, sizeof(pico8_splore), "%s/Splore.p8", rom_dir);
    if (strcasecmp(core, "ext-pico8") == 0 && !file_exist(pico8_splore)) {
        char pico8_splore_create[MAX_BUFFER_SIZE];
        snprintf(pico8_splore_create, sizeof(pico8_splore_create), "touch %s", pico8_splore);
        system(pico8_splore_create);
    }

    if (file_exist(MUOS_SAA_LOAD)) {
        remove(MUOS_SAA_LOAD);
    }
}

char **read_assign_ini(const char *filename, int *cores) {
    FILE * file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file!");
        return NULL;
    }

    char **headers = (char **) malloc(MAX_BUFFER_SIZE * sizeof(char *));
    if (headers == NULL) {
        perror("Memory allocation failure!");
        fclose(file);
        return NULL;
    }

    *cores = 0;
    char line[MAX_BUFFER_SIZE];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '[') {
            char *start = strstr(line, "[");
            char *end = strstr(line, "]");
            if (start != NULL && end != NULL && start < end) {
                size_t len = end - start - 1;
                headers[*cores] = (char *) malloc((len + 1) * sizeof(char));
                strncpy(headers[*cores], start + 1, len);
                headers[*cores][len] = '\0';
                (*cores)++;
            }
        }
    }

    fclose(file);
    return headers;
}

void create_system_items() {
    DIR *td;
    struct dirent *tf;

    char assign_dir[PATH_MAX];
    snprintf(assign_dir, sizeof(assign_dir), "%s/MUOS/info/assign",
             device.STORAGE.ROM.MOUNT);

    td = opendir(assign_dir);
    if (td == NULL) {
        lv_obj_clear_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    char **file_names = NULL;
    size_t file_count = 0;

    while ((tf = readdir(td))) {
        if (tf->d_type == DT_REG) {
            char filename[FILENAME_MAX];
            snprintf(filename, sizeof(filename), "%s/%s", assign_dir, tf->d_name);

            char *last_dot = strrchr(tf->d_name, '.');
            if (last_dot != NULL) {
                *last_dot = '\0';
            }

            file_names = realloc(file_names, (file_count + 1) * sizeof(char *));
            file_names[file_count] = strdup(tf->d_name);
            file_count++;
        }
    }

    closedir(td);
    qsort(file_names, file_count, sizeof(char *), str_compare);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    if (file_count > 0) {
        for (size_t i = 0; i < file_count; i++) {
            char *base_filename = file_names[i];
            ui_count++;

            lv_obj_t * ui_pnlCore = lv_obj_create(ui_pnlContent);
            apply_theme_list_panel(&theme, &device, ui_pnlCore);

            lv_obj_t * ui_lblCoreItem = lv_label_create(ui_pnlCore);
            apply_theme_list_item(&theme, ui_lblCoreItem, base_filename, false, false);

            lv_obj_t * ui_lblCoreItemGlyph = lv_img_create(ui_pnlCore);
            apply_theme_list_glyph(&theme, ui_lblCoreItemGlyph, mux_prog, "system");

            lv_group_add_obj(ui_group, ui_lblCoreItem);
            lv_group_add_obj(ui_group_glyph, ui_lblCoreItemGlyph);
            lv_group_add_obj(ui_group_panel, ui_pnlCore);

            apply_size_to_content(&theme, ui_pnlContent, ui_lblCoreItem, ui_lblCoreItemGlyph, base_filename);
        }
        if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
    }
}

void create_core_items(const char *target) {
    char filename[FILENAME_MAX];
    snprintf(filename, sizeof(filename), "%s/MUOS/info/assign/%s.ini",
             device.STORAGE.ROM.MOUNT, target);

    int cores;
    char **core_headers = read_assign_ini(filename, &cores);
    if (core_headers == NULL) {
        lv_obj_clear_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    char *assign_default = NULL;
    mini_t * assign_ini = mini_try_load(filename);

    if (assign_ini) {
        const char *a_def = mini_get_string(assign_ini, "global", "default", "");
        assign_default = malloc(strlen(a_def) + 1);
        if (assign_default) strcpy(assign_default, a_def);
        mini_free(assign_ini);
    }

    qsort(core_headers, cores, sizeof(char *), str_compare);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    const char *skip_entries[] = {
            "bios", "global"
    };

    for (int i = 0; i < cores; ++i) {
        int skip = 0;
        for (int k = 0; k < sizeof(skip_entries) / sizeof(skip_entries[0]); k++) {
            if (strcasecmp(core_headers[i], skip_entries[k]) == 0) {
                skip = 1;
                break;
            }
        }

        if (skip) {
            LOG_INFO(mux_prog, "Skipping Non-Assignable Core: %s", core_headers[i]);
            continue;
        }

        LOG_SUCCESS(mux_prog, "Generating Item For Core: %s", core_headers[i]);

        ui_count++;

        lv_obj_t * ui_pnlCore = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(&theme, &device, ui_pnlCore);

        lv_obj_t * ui_lblCoreItem = lv_label_create(ui_pnlCore);
        apply_theme_list_item(&theme, ui_lblCoreItem, core_headers[i], false, false);

        lv_obj_t * ui_lblCoreItemGlyph = lv_img_create(ui_pnlCore);

        char *glyph = (strcasecmp(core_headers[i], assign_default) == 0) ? "default" : "core";
        apply_theme_list_glyph(&theme, ui_lblCoreItemGlyph, mux_prog, glyph);

        lv_group_add_obj(ui_group, ui_lblCoreItem);
        lv_group_add_obj(ui_group_glyph, ui_lblCoreItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlCore);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblCoreItem, ui_lblCoreItemGlyph, core_headers[i]);

        free(core_headers[i]);
    }

    free(assign_default);

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
    free(core_headers);
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0);
    for (int step = 0; step < steps; ++step) {
        if (current_item_index > 0) {
            current_item_index--;
            nav_prev(ui_group, 1);
            nav_prev(ui_group_glyph, 1);
            nav_prev(ui_group_panel, 1);
        }
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound, 0);
    }
    for (int step = 0; step < steps; ++step) {
        if (current_item_index < (ui_count - 1)) {
            current_item_index++;
            nav_next(ui_group, 1);
            nav_next(ui_group_glyph, 1);
            nav_next(ui_group_panel, 1);
        }
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
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
        int num_events = epoll_wait(epoll_fd, events, device.DEVICE.EVENT, config.SETTINGS.ADVANCED.ACCELERATE);
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
                                } else if (ev.code == device.RAW_INPUT.BUTTON.Y) {
                                    if (strcasecmp(rom_system, "none") != 0) {
                                        LOG_INFO(mux_prog, "Parent Core Assignment Triggered");

                                        play_sound("confirm", nav_sound, 1);

                                        char chosen_core_ini[FILENAME_MAX];
                                        snprintf(chosen_core_ini, sizeof(chosen_core_ini),
                                                 "%s/MUOS/info/assign/%s.ini",
                                                 device.STORAGE.ROM.MOUNT, rom_system);

                                        mini_t * chosen_core = mini_load(chosen_core_ini);

                                        const char *raw_core = mini_get_string(
                                                chosen_core, str_trim(lv_label_get_text(element_focused)),
                                                "core", "none");

                                        int name_cache = mini_get_int(chosen_core, "global", "cache", 0);

                                        static char core_catalogue[MAX_BUFFER_SIZE];
                                        strcpy(core_catalogue, get_ini_string(chosen_core, "global",
                                                                              "catalogue", rom_system));

                                        create_core_assignment(raw_core, core_catalogue, rom_name,
                                                               name_cache, PARENT);

                                        mini_free(chosen_core);

                                        safe_quit = 1;
                                    }
                                } else if (ev.code == device.RAW_INPUT.BUTTON.X) {
                                    if (strcasecmp(rom_system, "none") != 0) {
                                        LOG_INFO(mux_prog, "Directory Core Assignment Triggered");

                                        play_sound("confirm", nav_sound, 1);

                                        char chosen_core_ini[FILENAME_MAX];
                                        snprintf(chosen_core_ini, sizeof(chosen_core_ini),
                                                 "%s/MUOS/info/assign/%s.ini",
                                                 device.STORAGE.ROM.MOUNT, rom_system);

                                        mini_t * chosen_core = mini_load(chosen_core_ini);

                                        const char *raw_core = mini_get_string(
                                                chosen_core, str_trim(lv_label_get_text(element_focused)),
                                                "core", "none");

                                        int name_cache = mini_get_int(chosen_core, "global", "cache", 0);

                                        static char core_catalogue[MAX_BUFFER_SIZE];
                                        strcpy(core_catalogue, get_ini_string(chosen_core, "global",
                                                                              "catalogue", rom_system));

                                        create_core_assignment(raw_core, core_catalogue, rom_name,
                                                               name_cache, DIRECTORY);

                                        mini_free(chosen_core);

                                        safe_quit = 1;
                                    }
                                } else if (ev.code == NAV_A) {
                                    if (strcasecmp(rom_system, "none") == 0) {
                                        load_assign(rom_name, rom_dir, str_trim(lv_label_get_text(element_focused)));
                                    } else {
                                        LOG_INFO(mux_prog, "Single Core Assignment Triggered");

                                        play_sound("confirm", nav_sound, 1);

                                        char chosen_core_ini[FILENAME_MAX];
                                        snprintf(chosen_core_ini, sizeof(chosen_core_ini),
                                                 "%s/MUOS/info/assign/%s.ini",
                                                 device.STORAGE.ROM.MOUNT, rom_system);

                                        mini_t * chosen_core = mini_load(chosen_core_ini);

                                        const char *raw_core = mini_get_string(
                                                chosen_core, str_trim(lv_label_get_text(element_focused)),
                                                "core", "none");

                                        int name_cache = mini_get_int(chosen_core, "global", "cache", 0);

                                        static char core_catalogue[MAX_BUFFER_SIZE];
                                        strcpy(core_catalogue, get_ini_string(chosen_core, "global",
                                                                              "catalogue", rom_system));

                                        create_core_assignment(raw_core, core_catalogue, rom_name,
                                                               name_cache, SINGLE);

                                        mini_free(chosen_core);
                                    }
                                    safe_quit = 1;
                                } else if (ev.code == NAV_B) {
                                    play_sound("back", nav_sound, 1);

                                    if (strcasecmp(rom_system, "none") == 0) {
                                        FILE * file = fopen(MUOS_SYS_LOAD, "w");
                                        fprintf(file, "%s", "");
                                        fclose(file);
                                    } else {
                                        load_assign(rom_name, rom_dir, "none");
                                    }

                                    remove(MUOS_SAA_LOAD);
                                    safe_quit = 1;
                                } else if (ev.code == device.RAW_INPUT.BUTTON.L1) {
                                    if (current_item_index >= 0 && current_item_index < ui_count) {
                                        list_nav_prev(theme.MUX.ITEM.COUNT);
                                    }
                                } else if (ev.code == device.RAW_INPUT.BUTTON.R1) {
                                    if (current_item_index >= 0 && current_item_index < ui_count) {
                                        list_nav_next(theme.MUX.ITEM.COUNT);
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
                        if (ev.code == ABS_Y) {
                            JOYUP_pressed = 0;
                            JOYDOWN_pressed = 0;
                            nav_hold = 0;
                            break;
                        }
                        if (ev.code == NAV_DPAD_VER || ev.code == NAV_ANLG_VER) {
                            if ((ev.value >= ((device.INPUT.AXIS_MAX) * -1) &&
                                 ev.value <= ((device.INPUT.AXIS_MIN) * -1)) ||
                                ev.value == -1) {
                                if (current_item_index == 0) {
                                    current_item_index = ui_count - 1;
                                    nav_prev(ui_group, 1);
                                    nav_prev(ui_group_glyph, 1);
                                    nav_prev(ui_group_panel, 1);
                                    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count,
                                                           current_item_index, ui_pnlContent);
                                } else if (current_item_index > 0) {
                                    JOYUP_pressed = (ev.value != 0);
                                    list_nav_prev(1);
                                }
                            } else if ((ev.value >= (device.INPUT.AXIS_MIN) &&
                                        ev.value <= (device.INPUT.AXIS_MAX)) ||
                                       ev.value == 1) {
                                if (current_item_index == ui_count - 1) {
                                    current_item_index = 0;
                                    nav_next(ui_group, 1);
                                    nav_next(ui_group_glyph, 1);
                                    nav_next(ui_group_panel, 1);
                                    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count,
                                                           current_item_index, ui_pnlContent);
                                } else if (current_item_index < ui_count) {
                                    JOYDOWN_pressed = (ev.value != 0);
                                    list_nav_next(1);
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

        if (JOYUP_pressed || JOYDOWN_pressed) {
            if (nav_hold > 2) {
                if (JOYUP_pressed && current_item_index > 0) {
                    list_nav_prev(1);
                }
                if (JOYDOWN_pressed && current_item_index < ui_count) {
                    list_nav_next(1);
                }
            }
            nav_hold++;
        } else {
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

    lv_label_set_text(ui_lblNavA, _("Confirm"));
    lv_label_set_text(ui_lblNavB, _("Back"));

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
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    if (strcasecmp(rom_system, "none") != 0) {
        lv_label_set_text(ui_lblNavA, _("Individual"));
        lv_label_set_text(ui_lblNavX, _("Directory"));
        lv_label_set_text(ui_lblNavY, _("Recursive"));

        lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_FLOATING);

        lv_obj_clear_flag(ui_lblNavY, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblNavY, LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_FLOATING);

        lv_obj_move_foreground(ui_lblNavAGlyph);
        lv_obj_move_foreground(ui_lblNavA);
        lv_obj_move_foreground(ui_lblNavXGlyph);
        lv_obj_move_foreground(ui_lblNavX);
        lv_obj_move_foreground(ui_lblNavYGlyph);
        lv_obj_move_foreground(ui_lblNavY);
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

    char *cmd_help = "\nmuOS Extras - Core Assignment\nUsage: %s <-acds>\n\nOptions:\n"
                     "\t-a Auto assign content directory check\n"
                     "\t-c Name of content file\n"
                     "\t-d Name of content directory\n"
                     "\t-s Name of content system (use 'none' for root)\n\n";

    int opt;
    while ((opt = getopt(argc, argv, "a:c:d:s:")) != -1) {
        switch (opt) {
            case 'a':
                auto_assign = optarg;
                break;
            case 'c':
                rom_name = optarg;
                break;
            case 'd':
                rom_dir = optarg;
                break;
            case 's':
                rom_system = optarg;
                break;
            default:
                fprintf(stderr, cmd_help, argv[0]);
                return 1;
        }
    }

    if (auto_assign == NULL || rom_name == NULL || rom_dir == NULL || rom_system == NULL) {
        fprintf(stderr, cmd_help, argv[0]);
        return 1;
    }

    load_config(&config);

    LOG_INFO(mux_prog, "Assign Core ROM_NAME: \"%s\"", rom_name);
    LOG_INFO(mux_prog, "Assign Core ROM_DIR: \"%s\"", rom_dir);
    LOG_INFO(mux_prog, "Assign Core ROM_SYS: \"%s\"", rom_system);

    if (atoi(auto_assign) && !file_exist(MUOS_SAA_LOAD)) {
        LOG_INFO(mux_prog, "Automatic Assign Core Initiated");

        char core_file[MAX_BUFFER_SIZE];
        snprintf(core_file, sizeof(core_file), "%s/info/core/%s/core.cfg",
                 STORAGE_PATH, get_last_subdir(rom_dir, '/', 4));

        if (file_exist(core_file)) {
            return 0;
        }

        int auto_assign_good = 0;

        char assign_file[MAX_BUFFER_SIZE];
        snprintf(assign_file, sizeof(assign_file), "%s/MUOS/info/assign.json",
                 device.STORAGE.ROM.MOUNT);

        if (json_valid(read_text_from_file(assign_file))) {
            static char assign_check[MAX_BUFFER_SIZE];
            snprintf(assign_check, sizeof(assign_check), "%s",
                     str_tolower(get_last_dir(rom_dir)));
            str_remchars(assign_check, " -_+");

            struct json auto_assign_config = json_object_get(
                    json_parse(read_text_from_file(assign_file)),
                    assign_check);

            if (json_exists(auto_assign_config)) {
                char ass_config[MAX_BUFFER_SIZE];
                json_string_copy(auto_assign_config, ass_config, sizeof(ass_config));

                LOG_INFO(mux_prog, "<Automatic Assign> Core Assigned: %s", ass_config);

                char assigned_core_ini[MAX_BUFFER_SIZE];
                snprintf(assigned_core_ini, sizeof(assigned_core_ini), "%s/MUOS/info/assign/%s",
                         device.STORAGE.ROM.MOUNT, ass_config);

                LOG_INFO(mux_prog, "<Automatic Assign> Obtaining Core INI: %s", assigned_core_ini);

                mini_t * core_config_ini = mini_load(assigned_core_ini);

                static char def_core[MAX_BUFFER_SIZE];
                strcpy(def_core, get_ini_string(core_config_ini, "global", "default", "none"));

                LOG_INFO(mux_prog, "<Automatic Assign> Default Core: %s", ass_config);

                if (strcmp(def_core, "none") != 0) {
                    static char auto_core[MAX_BUFFER_SIZE];
                    strcpy(auto_core, get_ini_string(core_config_ini, def_core, "core", "invalid"));

                    LOG_INFO(mux_prog, "<Automatic Assign> Assigned Core To: %s", auto_core);

                    if (strcmp(def_core, "invalid") != 0) {
                        static char core_catalogue[MAX_BUFFER_SIZE];
                        strcpy(core_catalogue, get_ini_string(core_config_ini, "global", "catalogue", "none"));

                        int name_cache = mini_get_int(core_config_ini, "global", "cache", 0);

                        LOG_INFO(mux_prog, "<Automatic Assign> Core Cache: %d", name_cache);
                        LOG_INFO(mux_prog, "<Automatic Assign> Core Catalogue: %s", core_catalogue);

                        create_core_assignment(auto_core, core_catalogue, rom_name, name_cache, DIRECTORY_NO_WIPE);

                        auto_assign_good = 1;
                        LOG_SUCCESS(mux_prog, "<Automatic Assign> Successful");
                    }
                }

                mini_free(core_config_ini);
            } else {
                if (strcmp(rom_system, "none") == 0) return 0;
            }
        }

        if (auto_assign_good) return 0;
    }

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
    load_language(mux_prog);

    ui_common_screen_init(&theme, &device, "");
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
            LOG_INFO(mux_prog, "SDL Init Success");
            nav_sound = 1;
        } else {
            LOG_ERROR(mux_prog, "SDL Failed To Init");
        }
    }

    lv_label_set_text(ui_lblScreenMessage, _("No Cores Found..."));

    if (strcasecmp(rom_system, "none") == 0) {
        create_system_items();
    } else {
        create_core_items(rom_system);
    }

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
        LOG_ERROR(mux_prog, "Failed to open joystick device!");
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

    if (ui_count > 0) {
        if (strcasecmp(rom_system, "none") == 0) {
            LOG_SUCCESS(mux_prog, "%d System%s Detected", ui_count, ui_count == 1 ? "" : "s");
        } else {
            LOG_SUCCESS(mux_prog, "%d Core%s Detected", ui_count, ui_count == 1 ? "" : "s");
        }
        char title[MAX_BUFFER_SIZE];
        snprintf(title, sizeof(title), "%s - %s", _("ASSIGN"), get_last_dir(rom_dir));
        lv_label_set_text(ui_lblTitle, title);
    } else {
        LOG_ERROR(mux_prog, "No Cores Detected - Check Directory!");
        lv_obj_clear_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
    }

    while (!safe_quit) {
        lv_task_handler();
        usleep(device.SCREEN.WAIT);
    }

    pthread_cancel(joystick_thread);

    close(js_fd);

    LOG_SUCCESS(mux_prog, "Safe Quit!");

    return 0;
}

uint32_t mux_tick(void) {
    struct timespec tv_now;
    clock_gettime(CLOCK_REALTIME, &tv_now);

    uint64_t now_ms = ((uint64_t) tv_now.tv_sec * 1000) + (tv_now.tv_nsec / 1000000);
    start_ms = start_ms || now_ms;

    return (uint32_t)(now_ms - start_ms);
}
