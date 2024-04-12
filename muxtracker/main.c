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
#include "../common/common.h"
#include "../common/help.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/mini/mini.h"

static int js_fd;

int turbo_mode = 0;
int msgbox_active = 0;
int input_disable = 0;
int SD2_found = 0;
int nav_sound = 0;
int safe_quit = 0;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;
mini_t *muos_config;

// Place as many NULL as there are options!
lv_obj_t *labels[] = {};
unsigned int label_count = sizeof(labels) / sizeof(labels[0]);

lv_obj_t *msgbox_element = NULL;

int progress_onscreen = -1;

lv_group_t *rom_image_box_group;
lv_group_t *rom_image_wall_group;
lv_group_t *ui_group;

int ui_count = 0;
int current_item_index = 0;
int first_open = 1;
int nav_moved = 1;
int content_panel_y = 34;

char *current_wall = "";

char *rom_system;
char *rom_second;
char *rom_launch;

typedef struct {
    const char *name;
    const char *value;
} OptionSend;

void get_rom_details(char *rom_name) {
    char rom_activity_path[MAX_BUFFER_SIZE];
    snprintf(rom_activity_path, sizeof(rom_activity_path), "/%s/activity/%s.txt", MUOS_INFO_PATH, rom_name);
    rom_system = read_line_from_file(rom_activity_path, 1);
    rom_second = read_line_from_file(rom_activity_path, 2);
    rom_launch = read_line_from_file(rom_activity_path, 3);
}

void create_metadata_directories() {
    char base_dir[MAX_BUFFER_SIZE];
    char content_dir[MAX_BUFFER_SIZE];

    snprintf(base_dir, sizeof(base_dir), "/%s/content/%s", MUOS_INFO_PATH, rom_system);
    snprintf(content_dir, sizeof(content_dir), "%s", base_dir);

    char *sys_dir = strtok(base_dir, "/");
    char dir_path[MAX_BUFFER_SIZE] = "/";

    while (sys_dir != NULL) {
        strcat(dir_path, sys_dir);

        if (access(dir_path, F_OK) == -1) {
            if (mkdir(dir_path, 0777) != 0) {
                printf("Error creating '%s' directory\n", dir_path);
            } else {
                printf("Created '%s' directory\n", dir_path);
            }
        } else {
            printf("Directory '%s' already exists. Skipping.\n", dir_path);
        }

        strcat(dir_path, "/");
        sys_dir = strtok(NULL, "/");
    }

    const char *subdirs[] = {"config", "image", "meta"};

    for (size_t i = 0; i < sizeof(subdirs) / sizeof(subdirs[0]); ++i) {
        snprintf(dir_path, sizeof(dir_path), "%s/%s", content_dir, subdirs[i]);

        if (access(dir_path, F_OK) == -1) {
            if (mkdir(dir_path, 0777) != 0) {
                printf("Error creating '%s' directory\n", dir_path);
            } else {
                printf("Created '%s' directory\n", dir_path);
            }
        } else {
            printf("Directory '%s' already exists. Skipping.\n", dir_path);
        }
    }
}

void create_metadata_file(const char *filename) {
    create_metadata_directories();

    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("Error opening the file");
        return;
    }

    fprintf(file, "No description found\n");
    fclose(file);
}

void image_refresh() {
    char *rom_name = lv_label_get_text(lv_group_get_focused(ui_group));
    char rom_image[MAX_BUFFER_SIZE];

    get_rom_details(rom_name);

    const char *image_type[] = {"box", "preview"};

    for (int i = 0; i < sizeof(image_type) / sizeof(image_type[0]); i++) {
        snprintf(rom_image, sizeof(rom_image),
                 "/%s/content/%s/image/%s/%s.png",
                 MUOS_INFO_PATH, rom_system, image_type[i], rom_name);

        if (file_exist(rom_image)) {
            char rom_image_path[MAX_BUFFER_SIZE];
            snprintf(rom_image_path, sizeof(rom_image_path),
                     "M:%s/content/%s/image/%s/%s.png",
                     MUOS_INFO_PATH, rom_system, image_type[i], rom_name);

            if (i == 0) {
                lv_img_set_src(ui_imgBox, rom_image_path);
            } else {
                lv_img_set_src(ui_imgPreviewImage, rom_image_path);
            }
        } else {
            if (i == 0) {
                lv_img_set_src(ui_imgBox, &ui_img_nothing_png);
            } else {
                lv_img_set_src(ui_imgPreviewImage, &ui_img_nothing_png);
            }
        }
    }
}

char *name_matches_name_txt(const char *base_filename) {
    static FILE *file = NULL;
    static char line[MAX_BUFFER_SIZE];
    static char matched_value[MAX_BUFFER_SIZE];

    if (file == NULL) {
        char name_txt_path[PATH_MAX];
        snprintf(name_txt_path, sizeof(name_txt_path), "/%s/name.txt", MUOS_INFO_PATH);
        file = fopen(name_txt_path, "r");
        if (file == NULL) {
            return NULL;
        }
    } else {
        fseek(file, 0, SEEK_SET);
    }

    while (fgets(line, sizeof(line), file)) {
        char *delimiter = strchr(line, '=');
        if (delimiter) {
            *delimiter = '\0';
            char *key = line;
            char *value = delimiter + 1;

            value[strcspn(value, "\r\n")] = '\0';

            if (strcasecmp(key, base_filename) == 0) {
                strncpy(matched_value, value, sizeof(matched_value));
                return matched_value;
            }
        }
    }

    return NULL;
}

void create_rom_items() {
    DIR *rd;
    struct dirent *rf;

    char rom_dir[PATH_MAX];
    snprintf(rom_dir, sizeof(rom_dir), "/%s/activity/", MUOS_INFO_PATH);

    rd = opendir(rom_dir);
    if (rd == NULL) {
        lv_obj_clear_flag(ui_lblRomMessage, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    char **file_names = NULL;
    size_t file_count = 0;

    while ((rf = readdir(rd))) {
        if (rf->d_type == DT_REG) {
            char filename[FILENAME_MAX];
            snprintf(filename, sizeof(filename), "/%s/activity/%s", MUOS_INFO_PATH, rf->d_name);

            char *last_dot = strrchr(rf->d_name, '.');
            if (last_dot != NULL) {
                *last_dot = '\0';
            }

            file_names = realloc(file_names, (file_count + 1) * sizeof(char *));
            file_names[file_count] = strdup(rf->d_name);
            file_count++;
        }
    }

    if (file_count > 7) {
        file_names = realloc(file_names, (file_count + 1) * sizeof(char *));
        file_names[file_count] = strdup(DUMMY_ROM);
        file_count++;
    }

    closedir(rd);
    //qsort(file_names, file_count, sizeof(char *), time_compare_for_history);

    ui_group = lv_group_create();

    for (size_t i = 0; i < file_count; i++) {
        char *base_filename = file_names[i];

        ui_count++;

        lv_obj_t * ui_lblRom = lv_label_create(ui_pnlContent);
        lv_obj_t * ui_lblStat = lv_label_create(ui_pnlContent);

        char *matched_name = name_matches_name_txt(base_filename);
        if (matched_name != NULL) {
            lv_label_set_text(ui_lblRom, matched_name);
        } else {
            lv_label_set_text(ui_lblRom, base_filename);
        }

        lv_label_set_text(ui_lblStat, "   00:00:00 in 0 plays");

        lv_obj_set_width(ui_lblRom, 640);
        lv_obj_set_width(ui_lblStat, 640);
        lv_obj_set_height(ui_lblRom, 26);
        lv_obj_set_height(ui_lblStat, 26);

        lv_obj_set_style_border_width(ui_lblRom, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(ui_lblRom, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_color(ui_lblRom, lv_color_hex(theme.SYSTEM.BACKGROUND),
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_main_stop(ui_lblRom, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_dir(ui_lblRom, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_bg_color(ui_lblRom, lv_color_hex(theme.LIST_DEFAULT.BACKGROUND),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_lblRom, theme.LIST_DEFAULT.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_main_stop(ui_lblRom, theme.LIST_DEFAULT.GRADIENT_START, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_stop(ui_lblRom, theme.LIST_DEFAULT.GRADIENT_STOP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(ui_lblRom, lv_color_hex(theme.LIST_DEFAULT.INDICATOR),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(ui_lblRom, theme.LIST_DEFAULT.INDICATOR_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_lblRom, lv_color_hex(theme.LIST_DEFAULT.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_lblRom, theme.LIST_DEFAULT.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_text_color(ui_lblStat, lv_color_hex(theme.LIST_DEFAULT.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_lblStat, theme.LIST_DEFAULT.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_bg_color(ui_lblRom, lv_color_hex(theme.LIST_FOCUS.BACKGROUND),
                                  LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(ui_lblRom, theme.LIST_FOCUS.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_main_stop(ui_lblRom, theme.LIST_FOCUS.GRADIENT_START, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_grad_stop(ui_lblRom, theme.LIST_FOCUS.GRADIENT_STOP, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(ui_lblRom, lv_color_hex(theme.LIST_FOCUS.INDICATOR),
                                      LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_border_opa(ui_lblRom, theme.LIST_FOCUS.INDICATOR_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(ui_lblRom, lv_color_hex(theme.LIST_FOCUS.TEXT), LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_text_opa(ui_lblRom, theme.LIST_FOCUS.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);

        lv_obj_set_style_pad_left(ui_lblRom, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(ui_lblRom, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(ui_lblRom, theme.FONT.LIST_PAD_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_lblRom, theme.FONT.LIST_PAD_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_pad_left(ui_lblStat, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(ui_lblStat, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(ui_lblStat, theme.FONT.LIST_PAD_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_lblStat, theme.FONT.LIST_PAD_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_text_line_space(ui_lblRom, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_long_mode(ui_lblRom, LV_LABEL_LONG_WRAP);

        if (strcasecmp(base_filename, DUMMY_ROM) == 0) {
            lv_obj_add_flag(ui_lblRom, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_state(ui_lblRom, LV_STATE_DISABLED);
            lv_obj_add_flag(ui_lblStat, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_state(ui_lblStat, LV_STATE_DISABLED);
        }

        lv_group_add_obj(ui_group, ui_lblRom);

        free(base_filename);
    }

    free(file_names);
}

void list_nav_prev(int steps) {
    for (int step = 0; step < steps; ++step) {
        if (current_item_index >= 1 && ui_count > 7) {
            current_item_index--;
            nav_prev(ui_group, 1);
            if (current_item_index > 2 && current_item_index < (ui_count - 5)) {
                content_panel_y += 56;
            }
        } else if (current_item_index >= 0 && ui_count <= 7) {
            if (current_item_index > 0) {
                current_item_index--;
                nav_prev(ui_group, 1);
            }
        }
    }

    image_refresh();
    play_sound("navigate", nav_sound);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    for (int step = 0; step < steps; ++step) {
        if (current_item_index < (ui_count - 2) && ui_count > 7) {
            if (current_item_index < (ui_count - 1)) {
                current_item_index++;
                nav_next(ui_group, 1);
                if (current_item_index >= 4 && current_item_index < (ui_count - 4)) {
                    content_panel_y -= 56;
                }
            }
        } else if (current_item_index < ui_count && ui_count <= 8) {
            if (current_item_index < (ui_count - 1)) {
                current_item_index++;
                nav_next(ui_group, 1);
            }
        }
    }

    image_refresh();
    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound);
    }
    nav_moved = 1;
}

void *joystick_task() {
    struct input_event ev;

    while (1) {
        if (input_disable) {
            continue;
        }
        read(js_fd, &ev, sizeof(struct input_event));
        switch (ev.type) {
            case EV_KEY:
                if (ev.value == 1) {
                    if (msgbox_active) {
                        switch (ev.code) {
                            case JOY_B:
                            case JOY_MENU:
                                play_sound("confirm", nav_sound);

                                msgbox_active = 0;
                                lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
                                break;
                            case JOY_A:
                                play_sound("confirm", nav_sound);

                                if (lv_obj_has_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN)) {
                                    lv_obj_add_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
                                    lv_obj_clear_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
                                } else {
                                    lv_obj_add_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
                                    lv_obj_clear_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
                                }
                                break;
                            default:
                                break;
                        }
                    } else {
                        switch (ev.code) {
                            case JOY_MENU:
                                play_sound("confirm", nav_sound);

                                char *name = lv_label_get_text(lv_group_get_focused(ui_group));

                                char rom_text_file[MAX_BUFFER_SIZE];
                                snprintf(rom_text_file, sizeof(rom_text_file),
                                         "/%s/content/%s/meta/%s.txt", MUOS_INFO_PATH, rom_system, name);

                                if (!file_exist(rom_text_file)) {
                                    create_metadata_file(rom_text_file);
                                }

                                lv_obj_clear_flag(ui_pnlHelpExtra, LV_OBJ_FLAG_HIDDEN);
                                show_rom_info(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpPreviewHeader,
                                              ui_lblHelpDescription, name, format_meta_text(rom_text_file));
                                break;
                            case JOY_B:
                                play_sound("back", nav_sound);
                                input_disable = 1;

                                safe_quit = 1;
                                break;
                            case JOY_X:
                                play_sound("confirm", nav_sound);
                                input_disable = 1;

                                char rom_file[MAX_BUFFER_SIZE];
                                snprintf(rom_file, sizeof(rom_file), "/%s/activity/%s.act", MUOS_INFO_PATH,
                                         lv_label_get_text(lv_group_get_focused(ui_group)));

                                remove(rom_file);

                                write_text_to_file("/tmp/mux_suppress", "1", "w");
                                write_text_to_file("/tmp/mux_reload", "1", "w");
                                safe_quit = 1;
                                break;
                            case JOY_L1:
                                if (current_item_index > 0) {
                                    list_nav_prev(ITEM_SKIP);
                                }
                                break;
                            case JOY_R1:
                                if (current_item_index < ((ui_count > 7) ? (ui_count - 2) : (ui_count - 1))) {
                                    list_nav_next(ITEM_SKIP);
                                }
                                break;
                        }
                    }
                }
            case EV_ABS:
                if (msgbox_active) {
                    break;
                }
                switch (theme.MISC.NAVIGATION_TYPE) {
                    case 0:
                        break;
                    case 1:
                        break;
                }
                if (ev.code == ABS_HAT0X || ev.code == ABS_Z) {
                    switch (ev.value) {
                        case -4096:
                        case -1:
                            if (current_item_index > 0) {
                                list_nav_prev(ITEM_SKIP);
                            }
                            break;
                        case 1:
                        case 4096:
                            if (current_item_index < ((ui_count > 7) ? (ui_count - 2) : (ui_count - 1))) {
                                list_nav_next(ITEM_SKIP);
                            }
                            break;
                    }
                }
                if (ev.code == ABS_HAT0Y || ev.code == ABS_RX) {
                    switch (ev.value) {
                        case -4096:
                        case -1:
                            if (current_item_index == 0) {
                                if (ui_count > 7) {
                                    list_nav_next((ui_count - 2));
                                    break;
                                } else {
                                    list_nav_next(ui_count);
                                    break;
                                }
                            } else if (current_item_index > 0) {
                                list_nav_prev(1);
                                break;
                            }
                            break;
                        case 1:
                        case 4096:
                            if (ui_count > 7 && current_item_index == (ui_count - 2)) {
                                list_nav_prev((ui_count - 2));
                                break;
                            } else if (ui_count <= 7 && current_item_index == (ui_count - 1)) {
                                list_nav_prev((ui_count - 1));
                                break;
                            } else if (current_item_index < ((ui_count > 7) ? (ui_count - 2) : (ui_count - 1))) {
                                list_nav_next(1);
                                break;
                            }
                            break;
                        default:
                            break;
                    }
                }
            default:
                break;
        }
    }
}

void init_elements() {
    lv_obj_move_foreground(ui_pnlFooter);
    lv_obj_move_foreground(ui_pnlHeader);
    lv_obj_move_foreground(ui_pnlHelp);

    if (bar_footer) {
        lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (bar_header) {
        lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    process_visual_element("clock", ui_lblDatetime);
    process_visual_element("battery", ui_staCapacity);
    process_visual_element("network", ui_staNetwork);
    process_visual_element("bluetooth", ui_staBluetooth);

    lv_label_set_text(ui_lblMessage, osd_message);

    lv_label_set_text(ui_lblNavB, "Back");
    lv_label_set_text(ui_lblNavX, "Remove");
    lv_label_set_text(ui_lblNavMenu, "Info");

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavCGlyph,
            ui_lblNavC,
            ui_lblNavYGlyph,
            ui_lblNavY,
            ui_lblNavZGlyph,
            ui_lblNavZ
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void glyph_task() {
    // TODO: Bluetooth connectivity!

/*
    if (is_network_connected() > 0) {
        lv_obj_set_style_text_color(ui_staNetwork, lv_color_hex(theme.NETWORK.ACTIVE), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staNetwork, theme.NETWORK.ACTIVE_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    else {
        lv_obj_set_style_text_color(ui_staNetwork, lv_color_hex(theme.NETWORK.NORMAL), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staNetwork, theme.NETWORK.NORMAL_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
*/

    if (atoi(read_text_from_file(BATT_CHARGER))) {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.BATTERY.ACTIVE),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.BATTERY.ACTIVE_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (read_battery_capacity() <= 15) {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.BATTERY.LOW), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.BATTERY.LOW_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.BATTERY.NORMAL),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.BATTERY.NORMAL_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            static char old_wall[MAX_BUFFER_SIZE];
            static char new_wall[MAX_BUFFER_SIZE];

            snprintf(old_wall, sizeof(old_wall), "%s", current_wall);
            snprintf(new_wall, sizeof(new_wall), "%s", load_wallpaper(
                    ui_scrTracker, ui_group, theme.MISC.ANIMATED_BACKGROUND));

            if (strcmp(new_wall, old_wall) != 0) {
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
        }
        lv_obj_invalidate(ui_pnlContent);
        lv_obj_invalidate(ui_pnlBox);
        lv_task_handler();
        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    int osd_display = -1;
    int opt;
    while ((opt = getopt(argc, argv, "m:")) != -1) {
        switch (opt) {
            case 'm':
                osd_display = atoi(optarg);
                break;
            default:
                fprintf(stderr, "\nmuOS Extras - Activity Tracker\nUsage: %s <-m>\n\nOptions:\n"
                                "\t-m Suppress starting message\n\n", argv[0]);
                return 1;
        }
    }

    if (osd_display == -1) {
        fprintf(stderr, "\nmuOS Extras - Activity Tracker\nUsage: %s <-m>\n\nOptions:\n"
                        "\t-m Suppress starting message\n\n", argv[0]);
        return 1;
    }

    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/system/bin", 1);
    setenv("NO_COLOR", "1", 1);

    lv_init();
    fbdev_init();

    static lv_color_t buf1[DISP_BUF_SIZE];
    static lv_color_t buf2[DISP_BUF_SIZE];
    static lv_disp_draw_buf_t disp_buf;

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    lv_disp_drv_register(&disp_drv);

    ui_init();
    muos_config = mini_try_load(MUOS_CONFIG_FILE);

    init_elements();

    lv_obj_set_user_data(ui_scrTracker, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());
    lv_label_set_text(ui_staCapacity, get_capacity());

    load_theme(&theme, basename(argv[0]));
    apply_theme();

    current_wall = load_wallpaper(ui_scrTracker, NULL, theme.MISC.ANIMATED_BACKGROUND);
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

    load_font(basename(argv[0]), ui_scrTracker);

    if (get_ini_int(muos_config, "settings.general", "sound", LABEL) == 2) {
        nav_sound = 1;
    }

    char txtPreview[MAX_BUFFER_SIZE];
    char txtInfo[MAX_BUFFER_SIZE];
    char B_A[snprintf(NULL, 0, "%u", theme.NAV.A.TEXT) + 1];
    sprintf(B_A, "%u", theme.NAV.A.TEXT);

    snprintf(txtPreview, sizeof(txtPreview), "Press  #%s ✁#  to switch to preview image", B_A);
    snprintf(txtInfo, sizeof(txtInfo), "Press  #%s ✁#  to switch to information", B_A);

    lv_label_set_text(ui_lblPreviewHeader, txtPreview);
    lv_label_set_text(ui_lblInfoHeader, txtInfo);

    create_rom_items();

    struct dt_task_param dt_par;
    struct bat_task_param bat_par;
    struct osd_task_param osd_par;

    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;
    osd_par.lblMessage = ui_lblMessage;
    osd_par.pnlMessage = ui_pnlMessage;
    osd_par.count = 0;

    js_fd = open(JOY_DEVICE, O_RDONLY);
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

    lv_timer_t *glyph_timer = lv_timer_create(glyph_task, UINT16_MAX / 32, NULL);
    lv_timer_ready(glyph_timer);

    lv_timer_t *ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
    lv_timer_ready(ui_refresh_timer);

    if (ui_count > 0) {
        lv_obj_set_y(ui_pnlContent, 34);
        lv_refr_now(NULL);
        image_refresh();
    } else {
        lv_obj_clear_flag(ui_lblRomMessage, LV_OBJ_FLAG_HIDDEN);
    }

    pthread_t joystick_thread;
    pthread_create(&joystick_thread, NULL, (void *(*)(void *)) joystick_task, NULL);

    init_elements();
    while (!safe_quit) {
        usleep(SCREEN_WAIT);
    }

    mini_free(muos_config);

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
