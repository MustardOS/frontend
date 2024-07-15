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
#include "../common/common.h"
#include "../common/help.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/glyph.h"
#include "../common/json/json.h"
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

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;

int ui_count = 0;
int current_item_index = 0;
int first_open = 1;
int content_panel_y = 0;

char *auto_assign;
char *rom_dir;
char *rom_system;

void show_help() {
    char *title = "ASSIGN CORE";
    char *message = MUXASSIGN_GENERIC;

    if (strlen(message) <= 1) {
        message = NO_HELP_FOUND;
    }

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent, title, message);
}

void create_core_assignment(const char *core, char *sys, int cache) {
    char core_dir[MAX_BUFFER_SIZE];
    snprintf(core_dir, sizeof(core_dir), "%s/MUOS/info/core/%s/",
             device.STORAGE.ROM.MOUNT, get_last_subdir(rom_dir, '/', 4));

    create_directories(core_dir);

    /*
     * The following is a temporary solution until core locking has been implemented.
     * Will be removed in the future
     * TODO: Individual core locking mechanism
     */
    delete_files_of_type(core_dir, "cfg", NULL);

    char core_file[MAX_BUFFER_SIZE];
    snprintf(core_file, sizeof(core_file), "%s/MUOS/info/core/%s/core.cfg",
             device.STORAGE.ROM.MOUNT, get_last_subdir(rom_dir, '/', 4));

    FILE * file = fopen(core_file, "w");
    if (file == NULL) {
        printf("Error opening file at: %s\n", core_file);
        return;
    }

    fprintf(file, "%s\n%s\n%d\n", core, str_trim(sys), cache);
    fclose(file);

    char pico8_splore[MAX_BUFFER_SIZE];
    sprintf(pico8_splore, "%s/Splore.p8", rom_dir);
    if (strcasecmp(core, "ext-pico8") == 0 && !file_exist(pico8_splore)) {
        char pico8_splore_create[MAX_BUFFER_SIZE];
        sprintf(pico8_splore_create, "touch %s", pico8_splore);
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
        lv_obj_clear_flag(ui_lblCoreMessage, LV_OBJ_FLAG_HIDDEN);
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

    if (file_count > 0) {
        for (size_t i = 0; i < file_count; i++) {
            char *base_filename = file_names[i];
            ui_count++;

            lv_obj_t * ui_pnlCore = lv_obj_create(ui_pnlContent);
            lv_obj_set_width(ui_pnlCore, device.MUX.WIDTH);
            lv_obj_set_height(ui_pnlCore, device.MUX.ITEM.HEIGHT);
            lv_obj_set_scrollbar_mode(ui_pnlCore, LV_SCROLLBAR_MODE_OFF);
            lv_obj_set_style_align(ui_pnlCore, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(ui_pnlCore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(ui_pnlCore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(ui_pnlCore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(ui_pnlCore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(ui_pnlCore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(ui_pnlCore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(ui_pnlCore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_column(ui_pnlCore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_obj_t * ui_lblCoreItem = lv_label_create(ui_pnlCore);
            lv_label_set_text(ui_lblCoreItem, base_filename);

            lv_obj_set_width(ui_lblCoreItem, device.MUX.WIDTH);
            lv_obj_set_height(ui_lblCoreItem, device.MUX.ITEM.HEIGHT);

            lv_obj_set_style_border_width(ui_lblCoreItem, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_side(ui_lblCoreItem, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_grad_color(ui_lblCoreItem, lv_color_hex(theme.SYSTEM.BACKGROUND),
                                           LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_main_stop(ui_lblCoreItem, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_grad_dir(ui_lblCoreItem, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_obj_set_style_bg_color(ui_lblCoreItem, lv_color_hex(theme.LIST_DEFAULT.BACKGROUND),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(ui_lblCoreItem, theme.LIST_DEFAULT.BACKGROUND_ALPHA,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_main_stop(ui_lblCoreItem, theme.LIST_DEFAULT.GRADIENT_START,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_grad_stop(ui_lblCoreItem, theme.LIST_DEFAULT.GRADIENT_STOP,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(ui_lblCoreItem, lv_color_hex(theme.LIST_DEFAULT.INDICATOR),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(ui_lblCoreItem, theme.LIST_DEFAULT.INDICATOR_ALPHA,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(ui_lblCoreItem, lv_color_hex(theme.LIST_DEFAULT.TEXT),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(ui_lblCoreItem, theme.LIST_DEFAULT.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_obj_set_style_bg_color(ui_lblCoreItem, lv_color_hex(theme.LIST_FOCUS.BACKGROUND),
                                      LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_bg_opa(ui_lblCoreItem, theme.LIST_FOCUS.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_bg_main_stop(ui_lblCoreItem, theme.LIST_FOCUS.GRADIENT_START,
                                          LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_bg_grad_stop(ui_lblCoreItem, theme.LIST_FOCUS.GRADIENT_STOP,
                                          LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_border_color(ui_lblCoreItem, lv_color_hex(theme.LIST_FOCUS.INDICATOR),
                                          LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_border_opa(ui_lblCoreItem, theme.LIST_FOCUS.INDICATOR_ALPHA,
                                        LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_text_color(ui_lblCoreItem, lv_color_hex(theme.LIST_FOCUS.TEXT),
                                        LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_text_opa(ui_lblCoreItem, theme.LIST_FOCUS.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);

            lv_obj_set_style_pad_left(ui_lblCoreItem, 32, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(ui_lblCoreItem, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(ui_lblCoreItem, theme.FONT.LIST_PAD_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(ui_lblCoreItem, theme.FONT.LIST_PAD_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_obj_set_style_text_line_space(ui_lblCoreItem, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_long_mode(ui_lblCoreItem, LV_LABEL_LONG_WRAP);

            lv_obj_t * ui_lblCoreItemGlyph = lv_label_create(ui_pnlCore);
            lv_label_set_text(ui_lblCoreItemGlyph, "\uF233");

            lv_obj_set_width(ui_lblCoreItemGlyph, device.MUX.WIDTH);
            lv_obj_set_height(ui_lblCoreItemGlyph, device.MUX.ITEM.HEIGHT);

            lv_obj_set_style_border_width(ui_lblCoreItemGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_obj_set_style_bg_opa(ui_lblCoreItemGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(ui_lblCoreItemGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(ui_lblCoreItemGlyph, lv_color_hex(theme.LIST_DEFAULT.TEXT),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_opa(ui_lblCoreItemGlyph, theme.LIST_DEFAULT.TEXT_ALPHA,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_obj_set_style_bg_opa(ui_lblCoreItemGlyph, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_border_opa(ui_lblCoreItemGlyph, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_text_color(ui_lblCoreItemGlyph, lv_color_hex(theme.LIST_FOCUS.TEXT),
                                        LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_text_opa(ui_lblCoreItemGlyph, theme.LIST_FOCUS.TEXT_ALPHA,
                                      LV_PART_MAIN | LV_STATE_FOCUSED);

            lv_obj_set_style_pad_left(ui_lblCoreItemGlyph, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(ui_lblCoreItemGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(ui_lblCoreItemGlyph, theme.FONT.LIST_ICON_PAD_TOP,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(ui_lblCoreItemGlyph, theme.FONT.LIST_ICON_PAD_BOTTOM,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_obj_set_style_text_align(ui_lblCoreItemGlyph, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(ui_lblCoreItemGlyph, &ui_font_AwesomeSmall, LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_group_add_obj(ui_group, ui_lblCoreItem);
            lv_group_add_obj(ui_group_glyph, ui_lblCoreItemGlyph);
        }
    }
}

void create_core_items(const char *target) {
    char filename[FILENAME_MAX];
    snprintf(filename, sizeof(filename), "%s/MUOS/info/assign/%s.ini",
             device.STORAGE.ROM.MOUNT, target);

    int cores;
    char **core_headers = read_assign_ini(filename, &cores);
    if (core_headers == NULL) {
        lv_obj_clear_flag(ui_lblCoreMessage, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    qsort(core_headers, cores, sizeof(char *), str_compare);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();

    const char *skip_entries[] = {
            "bios", "global"
    };

    for (int i = 0; i < cores; ++i) {
        int do_skip = 0;
        for (int k = 0; k < sizeof(skip_entries) / sizeof(skip_entries[0]); k++) {
            if (strcasecmp(core_headers[i], skip_entries[k]) == 0) {
                do_skip = 1;
            }
        }

        if (do_skip) continue;

        ui_count++;

        lv_obj_t * ui_pnlCore = lv_obj_create(ui_pnlContent);
        lv_obj_set_width(ui_pnlCore, device.MUX.WIDTH);
        lv_obj_set_height(ui_pnlCore, device.MUX.ITEM.HEIGHT);
        lv_obj_set_scrollbar_mode(ui_pnlCore, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_align(ui_pnlCore, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_pnlCore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ui_pnlCore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(ui_pnlCore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(ui_pnlCore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(ui_pnlCore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_pnlCore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_row(ui_pnlCore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_column(ui_pnlCore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t * ui_lblCoreItem = lv_label_create(ui_pnlCore);
        lv_label_set_text(ui_lblCoreItem, core_headers[i]);

        lv_obj_set_width(ui_lblCoreItem, device.MUX.WIDTH);
        lv_obj_set_height(ui_lblCoreItem, device.MUX.ITEM.HEIGHT);

        lv_obj_set_style_border_width(ui_lblCoreItem, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(ui_lblCoreItem, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_color(ui_lblCoreItem, lv_color_hex(theme.SYSTEM.BACKGROUND),
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_main_stop(ui_lblCoreItem, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_dir(ui_lblCoreItem, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_bg_color(ui_lblCoreItem, lv_color_hex(theme.LIST_DEFAULT.BACKGROUND),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_lblCoreItem, theme.LIST_DEFAULT.BACKGROUND_ALPHA,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_main_stop(ui_lblCoreItem, theme.LIST_DEFAULT.GRADIENT_START,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_stop(ui_lblCoreItem, theme.LIST_DEFAULT.GRADIENT_STOP,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(ui_lblCoreItem, lv_color_hex(theme.LIST_DEFAULT.INDICATOR),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(ui_lblCoreItem, theme.LIST_DEFAULT.INDICATOR_ALPHA,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_lblCoreItem, lv_color_hex(theme.LIST_DEFAULT.TEXT),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_lblCoreItem, theme.LIST_DEFAULT.TEXT_ALPHA,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_bg_color(ui_lblCoreItem, lv_color_hex(theme.LIST_FOCUS.BACKGROUND),
                                  LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(ui_lblCoreItem, theme.LIST_FOCUS.BACKGROUND_ALPHA,
                                LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_main_stop(ui_lblCoreItem, theme.LIST_FOCUS.GRADIENT_START,
                                      LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_grad_stop(ui_lblCoreItem, theme.LIST_FOCUS.GRADIENT_STOP,
                                      LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(ui_lblCoreItem, lv_color_hex(theme.LIST_FOCUS.INDICATOR),
                                      LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_border_opa(ui_lblCoreItem, theme.LIST_FOCUS.INDICATOR_ALPHA,
                                    LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(ui_lblCoreItem, lv_color_hex(theme.LIST_FOCUS.TEXT),
                                    LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_text_opa(ui_lblCoreItem, theme.LIST_FOCUS.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);

        lv_obj_set_style_pad_left(ui_lblCoreItem, 32, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(ui_lblCoreItem, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(ui_lblCoreItem, theme.FONT.LIST_PAD_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_lblCoreItem, theme.FONT.LIST_PAD_BOTTOM,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_text_line_space(ui_lblCoreItem, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_long_mode(ui_lblCoreItem, LV_LABEL_LONG_WRAP);

        lv_obj_t * ui_lblCoreItemGlyph = lv_label_create(ui_pnlCore);
        lv_label_set_text(ui_lblCoreItemGlyph, "\uF6D1");

        lv_obj_set_width(ui_lblCoreItemGlyph, device.MUX.WIDTH);
        lv_obj_set_height(ui_lblCoreItemGlyph, device.MUX.ITEM.HEIGHT);

        lv_obj_set_style_border_width(ui_lblCoreItemGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_bg_opa(ui_lblCoreItemGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(ui_lblCoreItemGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_lblCoreItemGlyph, lv_color_hex(theme.LIST_DEFAULT.TEXT),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_lblCoreItemGlyph, theme.LIST_DEFAULT.TEXT_ALPHA,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_bg_opa(ui_lblCoreItemGlyph, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_border_opa(ui_lblCoreItemGlyph, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(ui_lblCoreItemGlyph, lv_color_hex(theme.LIST_FOCUS.TEXT),
                                    LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_text_opa(ui_lblCoreItemGlyph, theme.LIST_FOCUS.TEXT_ALPHA,
                                  LV_PART_MAIN | LV_STATE_FOCUSED);

        lv_obj_set_style_pad_left(ui_lblCoreItemGlyph, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(ui_lblCoreItemGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(ui_lblCoreItemGlyph, theme.FONT.LIST_ICON_PAD_TOP,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_lblCoreItemGlyph, theme.FONT.LIST_ICON_PAD_BOTTOM,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_text_align(ui_lblCoreItemGlyph, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_lblCoreItemGlyph, &ui_font_AwesomeSmall, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_group_add_obj(ui_group, ui_lblCoreItem);
        lv_group_add_obj(ui_group_glyph, ui_lblCoreItemGlyph);

        free(core_headers[i]);
    }

    free(core_headers);
}

void list_nav_prev(int steps) {
    for (int step = 0; step < steps; ++step) {
        if (current_item_index >= 1 && ui_count > device.MUX.ITEM.COUNT) {
            current_item_index--;
            nav_prev(ui_group, 1);
            nav_prev(ui_group_glyph, 1);
            if (current_item_index > device.MUX.ITEM.PREV_LOW &&
                current_item_index < (ui_count - device.MUX.ITEM.PREV_HIGH)) {
                content_panel_y -= device.MUX.ITEM.PANEL;
                lv_obj_scroll_to_y(ui_pnlContent, content_panel_y, LV_ANIM_OFF);
            }
        } else if (current_item_index >= 0 && ui_count <= device.MUX.ITEM.COUNT) {
            if (current_item_index > 0) {
                current_item_index--;
                nav_prev(ui_group, 1);
                nav_prev(ui_group_glyph, 1);
            }
        }
    }

    play_sound("navigate", nav_sound);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    for (int step = 0; step < steps; ++step) {
        if (current_item_index < (ui_count - 1) && ui_count > device.MUX.ITEM.COUNT) {
            if (current_item_index < (ui_count - 1)) {
                current_item_index++;
                nav_next(ui_group, 1);
                nav_next(ui_group_glyph, 1);
                if (current_item_index >= device.MUX.ITEM.NEXT_HIGH &&
                    current_item_index < (ui_count - device.MUX.ITEM.NEXT_LOW)) {
                    content_panel_y += device.MUX.ITEM.PANEL;
                    lv_obj_scroll_to_y(ui_pnlContent, content_panel_y, LV_ANIM_OFF);
                }
            }
        } else if (current_item_index < ui_count && ui_count <= device.MUX.ITEM.COUNT) {
            if (current_item_index < (ui_count - 1)) {
                current_item_index++;
                nav_next(ui_group, 1);
                nav_next(ui_group_glyph, 1);
            }
        }
    }

    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound);
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
                                    play_sound("confirm", nav_sound);

                                    if (strcasecmp(rom_system, "none") == 0) {
                                        load_assign(rom_dir, str_trim(lv_label_get_text(element_focused)));
                                    } else {
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

                                        create_core_assignment(raw_core, core_catalogue, name_cache);

                                        mini_free(chosen_core);
                                    }

                                    safe_quit = 1;
                                } else if (ev.code == NAV_B) {
                                    play_sound("back", nav_sound);

                                    if (strcasecmp(rom_system, "none") == 0) {
                                        FILE * file = fopen(MUOS_SYS_LOAD, "w");
                                        fprintf(file, "%s", "");
                                        fclose(file);
                                    } else {
                                        load_assign(rom_dir, "none");
                                    }

                                    remove(MUOS_SAA_LOAD);
                                    safe_quit = 1;
                                } else if (ev.code == device.RAW_INPUT.BUTTON.L1) {
                                    if (current_item_index >= 0 && current_item_index < ui_count) {
                                        list_nav_prev(device.MUX.ITEM.COUNT);
                                        lv_task_handler();
                                    }
                                } else if (ev.code == device.RAW_INPUT.BUTTON.R1) {
                                    if (current_item_index >= 0 && current_item_index < ui_count) {
                                        list_nav_next(device.MUX.ITEM.COUNT);
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
                                    play_sound("confirm", nav_sound);
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
                                    int y = (ui_count - device.MUX.ITEM.COUNT) * device.MUX.ITEM.PANEL;
                                    lv_obj_scroll_to_y(ui_pnlContent, y, LV_ANIM_OFF);
                                    content_panel_y = y;
                                    current_item_index = ui_count - 1;
                                    nav_prev(ui_group, 1);
                                    nav_prev(ui_group_glyph, 1);
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
                                    lv_obj_scroll_to_y(ui_pnlContent, 0, LV_ANIM_OFF);
                                    content_panel_y = 0;
                                    current_item_index = 0;
                                    nav_next(ui_group, 1);
                                    nav_next(ui_group_glyph, 1);
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

        if (ui_count > device.MUX.ITEM.COUNT && (JOYUP_pressed || JOYDOWN_pressed)) {
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

    lv_label_set_text(ui_lblNavA, "Confirm");
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
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    char *overlay = load_overlay_image();
    if (strlen(overlay) > 0 && theme.MISC.IMAGE_OVERLAY) {
        lv_obj_t * overlay_img = lv_img_create(ui_scrAssign);
        lv_img_set_src(overlay_img, overlay);
        lv_obj_move_foreground(overlay_img);
    }

    if (TEST_IMAGE) display_testing_message(ui_scrAssign);
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
                    ui_scrAssign, ui_group, theme.MISC.ANIMATED_BACKGROUND));

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
                     load_static_image(ui_scrAssign, ui_group));

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
    load_device(&device);
    srand(time(NULL));

    char *cmd_help = "\nmuOS Extras - Core Assignment\nUsage: %s <-ads>\n\nOptions:\n"
                     "\t-a Auto assign content directory check\n"
                     "\t-d Name of content directory\n"
                     "\t-s Name of content system (use 'none' for root)\n\n";

    int opt;
    while ((opt = getopt(argc, argv, "a:d:s:")) != -1) {
        switch (opt) {
            case 'a':
                auto_assign = optarg;
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

    if (auto_assign == NULL || rom_dir == NULL || rom_system == NULL) {
        fprintf(stderr, cmd_help, argv[0]);
        return 1;
    }

    printf("ASSIGN CORE ROM_DIR: \"%s\"\n", rom_dir);
    printf("ASSIGN CORE ROM_SYS: \"%s\"\n", rom_system);

    if (atoi(auto_assign) && !file_exist(MUOS_SAA_LOAD)) {
        printf("ASSIGN AUTO INITIATED\n");

        char core_file[MAX_BUFFER_SIZE];
        snprintf(core_file, sizeof(core_file), "%s/MUOS/info/core/%s/core.cfg",
                 device.STORAGE.ROM.MOUNT, get_last_subdir(rom_dir, '/', 4));

        if (file_exist(core_file)) {
            return 0;
        }

        int auto_assign_good = 0;

        char assign_file[MAX_BUFFER_SIZE];
        snprintf(assign_file, sizeof(assign_file), "%s/MUOS/info/assign.json",
                 device.STORAGE.ROM.MOUNT);

        if (json_valid(read_text_from_file(assign_file))) {
            struct json auto_assign_config = json_object_get(
                    json_parse(read_text_from_file(assign_file)),
                    str_tolower(str_remchar(get_last_dir(rom_dir), ' ')));

            if (json_exists(auto_assign_config)) {
                char ass_config[MAX_BUFFER_SIZE];
                json_string_copy(auto_assign_config, ass_config, sizeof(ass_config));

                printf("ASSIGN AUTO CORE: %s\n", ass_config);

                char assigned_core_ini[MAX_BUFFER_SIZE];
                snprintf(assigned_core_ini, sizeof(assigned_core_ini), "%s/MUOS/info/assign/%s",
                         device.STORAGE.ROM.MOUNT, ass_config);

                printf("OBTAINING CORE INI: %s\n", assigned_core_ini);

                mini_t * core_config_ini = mini_load(assigned_core_ini);

                static char def_core[MAX_BUFFER_SIZE];
                strcpy(def_core, get_ini_string(core_config_ini, "global", "default", "none"));

                printf("DEFAULT CORE: %s\n", def_core);

                if (strcmp(def_core, "none") != 0) {
                    static char auto_core[MAX_BUFFER_SIZE];
                    strcpy(auto_core, get_ini_string(core_config_ini, def_core, "core", "invalid"));

                    printf("CORE TO BE ASSIGNED: %s\n", auto_core);

                    if (strcmp(def_core, "invalid") != 0) {
                        static char core_catalogue[MAX_BUFFER_SIZE];
                        strcpy(core_catalogue, get_ini_string(core_config_ini, "global", "catalogue", "none"));

                        int name_cache = mini_get_int(core_config_ini, "global", "cache", 0);

                        printf("ASSIGN CORE CACHE: %d\n", name_cache);
                        printf("ASSIGN CORE CATALOGUE: %s\n", core_catalogue);

                        create_core_assignment(auto_core, core_catalogue, name_cache);

                        auto_assign_good = 1;
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

    ui_init();
    init_elements();

    lv_obj_set_user_data(ui_scrAssign, basename(argv[0]));

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

    current_wall = load_wallpaper(ui_scrAssign, NULL, theme.MISC.ANIMATED_BACKGROUND);
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

    load_font_text(basename(argv[0]), ui_scrAssign);

    if (config.SETTINGS.GENERAL.SOUND == 2) {
        nav_sound = 1;
    }

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

    if (ui_count > 0) {
        char title[MAX_BUFFER_SIZE];
        snprintf(title, sizeof(title), "ASSIGN - %s", get_last_dir(rom_dir));
        lv_label_set_text(ui_lblTitle, title);
    } else {
        lv_obj_clear_flag(ui_lblCoreMessage, LV_OBJ_FLAG_HIDDEN);
    }

    if (ui_count > device.MUX.ITEM.COUNT) {
        lv_obj_t * last_item = lv_obj_get_child(ui_pnlContent, -1);
        lv_obj_set_height(last_item, lv_obj_get_height(last_item) + 50); // Don't bother asking...
    }

    init_elements();
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
