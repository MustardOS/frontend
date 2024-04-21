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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <libgen.h>
#include "../common/common.h"
#include "../common/help.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/glyph.h"
#include "../common/array.h"
#include "../common/mini/mini.h"

char *mux_prog;
struct glyph_config glyph;
struct theme_config theme;

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
mini_t *muos_config;

// Place as many NULL as there are options!
lv_obj_t *labels[] = {};
unsigned int label_count = sizeof(labels) / sizeof(labels[0]);

lv_obj_t *msgbox_element = NULL;

int progress_onscreen = -1;

enum list_module {
    ROOT,
    MMC,
    SDCARD,
    USB,
    FAVOURITE,
    HISTORY
} module;

struct items content_items;
struct items named_items;
struct items named_index;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;

int sd_card;
char *sd_dir = NULL;

char *SD1 = "/mnt/mmc/ROMS/";
char *SD2 = "/mnt/sdcard/ROMS/";
char *E_USB = "/mnt/usb/ROMS/";

char *current_wall = "";

char *prev_dir;
char *curr_dir;
char *next_dir;

int prev_index;

int ui_count = 0;
int ui_file_count = 0;
int current_item_index = 0;
int content_file_index = 0;
int first_open = 1;
int nav_moved = 1;
int content_panel_y = 0;

static char current_meta_text[MAX_BUFFER_SIZE];
static char current_content_label[MAX_BUFFER_SIZE];
static char box_image_previous_path[MAX_BUFFER_SIZE];
static char preview_image_previous_path[MAX_BUFFER_SIZE];

lv_timer_t *datetime_timer;
lv_timer_t *capacity_timer;
lv_timer_t *osd_timer;
lv_timer_t *glyph_timer;
lv_timer_t *ui_refresh_timer;

char *load_friendly_names() {
    char *load_cache_file = malloc(MAX_BUFFER_SIZE);

    if (load_cache_file == NULL) {
        return MUOS_NAME_FILE;
    }

    switch (module) {
        case MMC:
            if (strcasecmp(sd_dir, strip_dir(SD1)) != 0) {
                snprintf(load_cache_file, MAX_BUFFER_SIZE, "%s/mmc/%s.ini",
                         MUOS_CACHE_DIR, get_last_subdir(sd_dir, '/', 4));
            } else {
                snprintf(load_cache_file, MAX_BUFFER_SIZE, "%s/root_mmc.ini",
                         MUOS_CACHE_DIR);
            }
            break;
        case SDCARD:
            if (strcasecmp(sd_dir, strip_dir(SD2)) != 0) {
                snprintf(load_cache_file, MAX_BUFFER_SIZE, "%s/sdcard/%s.ini",
                         MUOS_CACHE_DIR, get_last_subdir(sd_dir, '/', 4));
            } else {
                snprintf(load_cache_file, MAX_BUFFER_SIZE, "%s/root_sdcard.ini",
                         MUOS_CACHE_DIR);
            }
            break;
        case USB:
            if (strcasecmp(sd_dir, strip_dir(E_USB)) != 0) {
                snprintf(load_cache_file, MAX_BUFFER_SIZE, "%s/usb/%s.ini",
                         MUOS_CACHE_DIR, get_last_subdir(sd_dir, '/', 4));
            } else {
                snprintf(load_cache_file, MAX_BUFFER_SIZE, "%s/root_usb.ini",
                         MUOS_CACHE_DIR);
            }
            break;
        case FAVOURITE:
            snprintf(load_cache_file, MAX_BUFFER_SIZE, "%s/favourite.ini",
                     MUOS_CACHE_DIR);
            break;
        case HISTORY:
            snprintf(load_cache_file, MAX_BUFFER_SIZE, "%s/history.ini",
                     MUOS_CACHE_DIR);
            break;
        default:
            snprintf(load_cache_file, MAX_BUFFER_SIZE, "%s",
                     MUOS_NAME_FILE);
            break;
    }

    if (file_exist(load_cache_file)) {
        return load_cache_file;
    } else {
        free(load_cache_file);
        return MUOS_NAME_FILE;
    }
}

char *load_content_core(int force) {
    char content_core[MAX_BUFFER_SIZE];

    char *card_full;
    switch (module) {
        case MMC:
            card_full = SD1;
            break;
        case SDCARD:
            card_full = SD2;
            break;
        case USB:
            card_full = E_USB;
            break;
        default:
            return NULL;
    }

    if (strcasecmp(get_last_subdir(sd_dir, '/', 4), strip_dir(card_full)) == 0) {
        snprintf(content_core, sizeof(content_core), "%s/core.cfg",
                 MUOS_CORE_DIR);
    } else {
        snprintf(content_core, sizeof(content_core), "%s/%s/core.cfg",
                 MUOS_CORE_DIR, get_last_subdir(sd_dir, '/', 4));
    }

    if (file_exist(content_core) && !force) {
        printf("LOADING CORE AT: %s\n", content_core);
        return read_text_from_file(content_core);
    } else {
        load_assign(sd_dir, "none");
        safe_quit = 1;
    }

    return NULL;
}

char *load_content_description() {
    char content_desc[MAX_BUFFER_SIZE];
    char *content_label = lv_label_get_text(lv_group_get_focused(ui_group));

    switch (module) {
        case ROOT:
            if (strcasecmp(content_label, "SD1 (mmc)") == 0) {
                snprintf(content_desc, sizeof(content_desc), "%s/Root/text/sd1.txt",
                         MUOS_CATALOGUE_DIR);
            } else if (strcasecmp(content_label, "SD2 (sdcard)") == 0) {
                snprintf(content_desc, sizeof(content_desc), "%s/Root/text/sd2.txt",
                         MUOS_CATALOGUE_DIR);
            }
            break;
        case FAVOURITE:
            char f_core_file[MAX_BUFFER_SIZE];
            snprintf(f_core_file, sizeof(f_core_file), "%s/%s.cfg",
                     MUOS_FAVOURITE_DIR, content_label);
            snprintf(content_desc, sizeof(content_desc), "%s/%s/text/%s.txt",
                     MUOS_CATALOGUE_DIR, read_line_from_file(f_core_file, 5),
                     strip_ext(read_line_from_file(f_core_file, 6)));
            break;
        case HISTORY:
            char h_core_file[MAX_BUFFER_SIZE];
            snprintf(h_core_file, sizeof(h_core_file), "%s/%s.cfg",
                     MUOS_FAVOURITE_DIR, content_label);
            snprintf(content_desc, sizeof(content_desc), "%s/%s/text/%s.txt",
                     MUOS_CATALOGUE_DIR, read_line_from_file(h_core_file, 5),
                     strip_ext(read_line_from_file(h_core_file, 6)));
            break;
        default:
            content_file_index = atoi(get_string_at_index(&named_index, current_item_index));

            char *card_full;
            switch (module) {
                case MMC:
                    card_full = SD1;
                    break;
                case SDCARD:
                    card_full = SD2;
                    break;
                case USB:
                    card_full = E_USB;
                    break;
                default:
                    return NULL;
                    break;
            }

            if (strcasecmp(get_last_subdir(sd_dir, '/', 4), strip_dir(card_full)) == 0) {
                snprintf(content_desc, sizeof(content_desc), "%s/Root/text/%s.txt",
                         MUOS_CATALOGUE_DIR, content_label);
            } else {
                char *desc_name = strip_ext(get_string_at_index(&content_items, content_file_index));

                char core_file[MAX_BUFFER_SIZE];
                snprintf(core_file, sizeof(core_file), "%s/%s/core.cfg",
                         MUOS_CORE_DIR, get_last_subdir(sd_dir, '/', 4));

                printf("TRYING TO READ CORE CONFIG META: %s\n", core_file);
                char *core_desc = read_line_from_file(core_file, 2);
                if (strlen(core_desc) <= 1 && strcasecmp(desc_name, DUMMY_DIR) != 0) {
                    printf("CORE IS NOT SET - TEXT NOT LOADED\n");
                    return "No Information Found";
                }
                printf("TEXT IS STORED AT: %s\n", core_desc);

                if (strcasecmp(desc_name, DUMMY_DIR) == 0) {
                    snprintf(content_desc, sizeof(content_desc), "%s/Folder/text/%s.txt",
                             MUOS_CATALOGUE_DIR, lv_label_get_text(lv_group_get_focused(ui_group)));
                } else {
                    snprintf(content_desc, sizeof(content_desc), "%s/%s/text/%s.txt",
                             MUOS_CATALOGUE_DIR, core_desc, desc_name);
                }
            }
            break;
    }

    if (file_exist(content_desc)) {
        snprintf(current_meta_text, sizeof(current_meta_text), "%s", format_meta_text(content_desc));
        return current_meta_text;
    }

    snprintf(current_meta_text, sizeof(current_meta_text), " ");
    return "No Information Found";
}

void image_refresh(char *image_type) {
    char image[MAX_BUFFER_SIZE];
    char image_path[MAX_BUFFER_SIZE];

    char *content_label = lv_label_get_text(lv_group_get_focused(ui_group));

    switch (module) {
        case ROOT:
            if (strcasecmp(content_label, "SD1 (mmc)") == 0) {
                snprintf(image, sizeof(image), "%s/Root/%s/sd1.png",
                         MUOS_CATALOGUE_DIR, image_type);
                snprintf(image_path, sizeof(image_path), "M:%s/Root/%s/sd1.png",
                         MUOS_CATALOGUE_PATH, image_type);
            } else if (strcasecmp(content_label, "SD2 (sdcard)") == 0) {
                snprintf(image, sizeof(image), "%s/Root/%s/sd2.png",
                         MUOS_CATALOGUE_DIR, image_type);
                snprintf(image_path, sizeof(image_path), "M:%s/Root/%s/sd2.png",
                         MUOS_CATALOGUE_PATH, image_type);
            } else if (strcasecmp(content_label, "USB (external)") == 0) {
                snprintf(image, sizeof(image), "%s/Root/%s/usb.png",
                         MUOS_CATALOGUE_DIR, image_type);
                snprintf(image_path, sizeof(image_path), "M:%s/Root/%s/usb.png",
                         MUOS_CATALOGUE_PATH, image_type);
            }
            break;
        case FAVOURITE:
            char f_core_file[MAX_BUFFER_SIZE];
            snprintf(f_core_file, sizeof(f_core_file), "%s/%s.cfg",
                     MUOS_FAVOURITE_DIR, content_label);

            char *f_core_artwork = read_line_from_file(f_core_file, 3);
            if (strlen(f_core_artwork) <= 1) {
                printf("CORE IS NOT SET - ARTWORK NOT LOADED\n");
                return;
            }

            char *f_file_name = strip_ext(read_line_from_file(f_core_file, 6));

            snprintf(image, sizeof(image), "%s/%s/%s/%s.png",
                     MUOS_CATALOGUE_DIR, f_core_artwork, image_type, f_file_name);
            snprintf(image_path, sizeof(image_path), "M:%s/%s/%s/%s.png",
                     MUOS_CATALOGUE_PATH, f_core_artwork, image_type, f_file_name);
            break;
        case HISTORY:
            char h_core_file[MAX_BUFFER_SIZE];
            snprintf(h_core_file, sizeof(h_core_file), "%s/%s.cfg",
                     MUOS_HISTORY_DIR, content_label);

            char *h_core_artwork = read_line_from_file(h_core_file, 3);
            if (strlen(h_core_artwork) <= 1) {
                printf("CORE IS NOT SET - ARTWORK NOT LOADED\n");
                return;
            }

            char *h_file_name = strip_ext(read_line_from_file(h_core_file, 6));

            snprintf(image, sizeof(image), "%s/%s/%s/%s.png",
                     MUOS_CATALOGUE_DIR, h_core_artwork, image_type, h_file_name);
            snprintf(image_path, sizeof(image_path), "M:%s/%s/%s/%s.png",
                     MUOS_CATALOGUE_PATH, h_core_artwork, image_type, h_file_name);
            break;
        default:
            content_file_index = atoi(get_string_at_index(&named_index, current_item_index));

            char *card_full;
            switch (module) {
                case MMC:
                    card_full = SD1;
                    break;
                case SDCARD:
                    card_full = SD2;
                    break;
                case USB:
                    card_full = E_USB;
                    break;
                default:
                    return;
            }

            if (strcasecmp(get_last_subdir(sd_dir, '/', 4), strip_dir(card_full)) == 0) {
                snprintf(image, sizeof(image), "%s/Folder/%s/%s.png",
                         MUOS_CATALOGUE_DIR, image_type, content_label);
                snprintf(image_path, sizeof(image_path), "M:%s/Folder/%s/%s.png",
                         MUOS_CATALOGUE_PATH, image_type, content_label);
            } else {
                char *file_name = strip_ext(get_string_at_index(&content_items, content_file_index));

                char core_file[MAX_BUFFER_SIZE];
                snprintf(core_file, sizeof(core_file), "%s/%s/core.cfg",
                         MUOS_CORE_DIR, get_last_subdir(sd_dir, '/', 4));

                char *core_artwork = read_line_from_file(core_file, 2);
                if (strlen(core_artwork) <= 1 && strcasecmp(file_name, DUMMY_DIR) != 0) {
                    printf("CORE IS NOT SET - ARTWORK NOT LOADED\n");
                    return;
                }

                if (strcasecmp(file_name, DUMMY_DIR) == 0) {
                    snprintf(image, sizeof(image), "%s/Folder/%s/%s.png",
                             MUOS_CATALOGUE_DIR, image_type,
                             lv_label_get_text(lv_group_get_focused(ui_group)));
                    snprintf(image_path, sizeof(image_path), "M:%s/Folder/%s/%s.png",
                             MUOS_CATALOGUE_PATH, image_type,
                             lv_label_get_text(lv_group_get_focused(ui_group)));
                } else {
                    snprintf(image, sizeof(image), "%s/%s/%s/%s.png",
                             MUOS_CATALOGUE_DIR, core_artwork, image_type, file_name);
                    snprintf(image_path, sizeof(image_path), "M:%s/%s/%s/%s.png",
                             MUOS_CATALOGUE_PATH, core_artwork, image_type, file_name);
                }
            }
            break;
    }

    if (strcasecmp(image_type, "preview") == 0) {
        if (strcmp(preview_image_previous_path, image) != 0) {
            printf("LOADING PREVIEW ARTWORK AT: %s\n", image);

            if (file_exist(image)) {
                lv_img_set_src(ui_imgHelpPreviewImage, image_path);
                snprintf(preview_image_previous_path, sizeof(preview_image_previous_path), "%s", image);
            } else {
                lv_img_set_src(ui_imgHelpPreviewImage, &ui_img_nothing_png);
                snprintf(preview_image_previous_path, sizeof(preview_image_previous_path), " ");
            }
        }
    } else {
        if (strcmp(box_image_previous_path, image) != 0) {
            printf("LOADING BOX ARTWORK AT: %s\n", image);

            if (file_exist(image)) {
                lv_img_set_src(ui_imgBox, image_path);
                snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", image);
            } else {
                lv_img_set_src(ui_imgBox, &ui_img_nothing_png);
                snprintf(box_image_previous_path, sizeof(box_image_previous_path), " ");
            }
        }
    }
}

void add_directory_and_file_names(const char *base_dir, char ***dir_names, int *dir_count,
                                  char ***file_names, int *file_count) {
    struct dirent *entry;
    DIR *dir = opendir(base_dir);

    if (!dir) {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (!should_skip(entry->d_name)) {
            char full_path[PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, entry->d_name);
            if (entry->d_type == DT_DIR) {
                if (strcasecmp(entry->d_name, ".") != 0 && strcasecmp(entry->d_name, "..") != 0) {
                    char *subdir_path = (char *) malloc(strlen(entry->d_name) + 2);
                    snprintf(subdir_path, strlen(entry->d_name) + 2, "%s", entry->d_name);

                    *dir_names = (char **) realloc(*dir_names, (*dir_count + 1) * sizeof(char *));
                    (*dir_names)[*dir_count] = subdir_path;
                    (*dir_count)++;
                }
            } else if (entry->d_type == DT_REG) {
                struct stat file_stat;
                if (stat(full_path, &file_stat) != -1 && file_stat.st_size > 0) {
                    char *file_path = (char *) malloc(strlen(entry->d_name) + 2);
                    snprintf(file_path, strlen(entry->d_name) + 2, "%s", entry->d_name);

                    *file_names = (char **) realloc(*file_names, (*file_count + 1) * sizeof(char *));
                    (*file_names)[*file_count] = file_path;
                    (*file_count)++;
                    ui_file_count++;
                }
            }
        }
    }

    closedir(dir);
}

void gen_label(char *item_glyph, char *item_text) {
    lv_obj_t * ui_pnlExplore = lv_obj_create(ui_pnlContent);
    lv_obj_set_width(ui_pnlExplore, 640);
    lv_obj_set_height(ui_pnlExplore, 28);
    lv_obj_set_scrollbar_mode(ui_pnlExplore, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_align(ui_pnlExplore, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlExplore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlExplore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlExplore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlExplore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlExplore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlExplore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlExplore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlExplore, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_lblExploreItem = lv_label_create(ui_pnlExplore);
    lv_label_set_text(ui_lblExploreItem, item_text);

    lv_obj_set_width(ui_lblExploreItem, 640);
    lv_obj_set_height(ui_lblExploreItem, 28);

    lv_obj_set_style_border_width(ui_lblExploreItem, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblExploreItem, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblExploreItem, lv_color_hex(theme.SYSTEM.BACKGROUND),
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblExploreItem, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblExploreItem, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_lblExploreItem, lv_color_hex(theme.LIST_DEFAULT.BACKGROUND),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblExploreItem, theme.LIST_DEFAULT.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblExploreItem, theme.LIST_DEFAULT.GRADIENT_START,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblExploreItem, theme.LIST_DEFAULT.GRADIENT_STOP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblExploreItem, lv_color_hex(theme.LIST_DEFAULT.INDICATOR),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblExploreItem, theme.LIST_DEFAULT.INDICATOR_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblExploreItem, lv_color_hex(theme.LIST_DEFAULT.TEXT),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblExploreItem, theme.LIST_DEFAULT.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_lblExploreItem, lv_color_hex(theme.LIST_FOCUS.BACKGROUND),
                              LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(ui_lblExploreItem, theme.LIST_FOCUS.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_main_stop(ui_lblExploreItem, theme.LIST_FOCUS.GRADIENT_START, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_grad_stop(ui_lblExploreItem, theme.LIST_FOCUS.GRADIENT_STOP, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(ui_lblExploreItem, lv_color_hex(theme.LIST_FOCUS.INDICATOR),
                                  LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(ui_lblExploreItem, theme.LIST_FOCUS.INDICATOR_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(ui_lblExploreItem, lv_color_hex(theme.LIST_FOCUS.TEXT),
                                LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblExploreItem, theme.LIST_FOCUS.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_set_style_pad_left(ui_lblExploreItem, 32, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblExploreItem, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblExploreItem, theme.FONT.LIST_PAD_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblExploreItem, theme.FONT.LIST_PAD_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_line_space(ui_lblExploreItem, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_long_mode(ui_lblExploreItem, LV_LABEL_LONG_WRAP);

    lv_obj_t * ui_lblExploreItemGlyph = lv_label_create(ui_pnlExplore);
    lv_label_set_text(ui_lblExploreItemGlyph, item_glyph);

    lv_obj_set_width(ui_lblExploreItemGlyph, 640);
    lv_obj_set_height(ui_lblExploreItemGlyph, 28);

    lv_obj_set_style_border_width(ui_lblExploreItemGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_opa(ui_lblExploreItemGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblExploreItemGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblExploreItemGlyph, lv_color_hex(theme.LIST_DEFAULT.TEXT),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblExploreItemGlyph, theme.LIST_DEFAULT.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_opa(ui_lblExploreItemGlyph, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(ui_lblExploreItemGlyph, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(ui_lblExploreItemGlyph, lv_color_hex(theme.LIST_FOCUS.TEXT),
                                LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblExploreItemGlyph, theme.LIST_FOCUS.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_set_style_pad_left(ui_lblExploreItemGlyph, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblExploreItemGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblExploreItemGlyph, theme.FONT.LIST_ICON_PAD_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblExploreItemGlyph, theme.FONT.LIST_ICON_PAD_BOTTOM,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_align(ui_lblExploreItemGlyph, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblExploreItemGlyph, &ui_font_AwesomeSmall, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_group_add_obj(ui_group, ui_lblExploreItem);
    lv_group_add_obj(ui_group_glyph, ui_lblExploreItemGlyph);
}

pthread_mutex_t named_index_mutex = PTHREAD_MUTEX_INITIALIZER;

struct ThreadArgs {
    int start_index;
    int end_index;
};

void *process_items(void *arg) {
    struct ThreadArgs *args = (struct ThreadArgs *) arg;

    for (int i = args->start_index; i < args->end_index; i++) {
        char n_index[MAX_BUFFER_SIZE];
        snprintf(n_index, sizeof(n_index), "%d", get_label_placement(named_items.array[i]));

        pthread_mutex_lock(&named_index_mutex);
        push_string(&named_index, n_index);
        pthread_mutex_unlock(&named_index_mutex);

        char *item_name = strip_label_placement(named_items.array[i]);
        if (strcasecmp(item_name, DUMMY_DIR) != 0) {
            gen_label("\uF15B", item_name);
        }
    }

    pthread_exit(NULL);
}

void gen_item(char **file_names, int file_count) {
    char init_cache_file[MAX_BUFFER_SIZE];
    char init_meta_dir[MAX_BUFFER_SIZE];

    switch (module) {
        case MMC:
            if (strcasecmp(sd_dir, strip_dir(SD1)) != 0) {
                snprintf(init_cache_file, sizeof(init_cache_file), "%s/mmc/%s.ini",
                         MUOS_CACHE_DIR, strchr(strdup(sd_dir), '/') + strlen(SD1));
                snprintf(init_meta_dir, sizeof(init_meta_dir), "%s/%s/",
                         MUOS_CORE_DIR, strchr(strdup(sd_dir), '/') + strlen(SD1));
            } else {
                snprintf(init_cache_file, sizeof(init_cache_file), "%s/root_mmc.ini",
                         MUOS_CACHE_DIR);
                snprintf(init_meta_dir, sizeof(init_meta_dir), "%s/",
                         MUOS_CORE_DIR);
            }
            break;
        case SDCARD:
            if (strcasecmp(sd_dir, strip_dir(SD2)) != 0) {
                snprintf(init_cache_file, sizeof(init_cache_file), "%s/sdcard/%s.ini",
                         MUOS_CACHE_DIR, strchr(strdup(sd_dir), '/') + strlen(SD2));
                snprintf(init_meta_dir, sizeof(init_meta_dir), "%s/%s/",
                         MUOS_CORE_DIR, strchr(strdup(sd_dir), '/') + strlen(SD2));
            } else {
                snprintf(init_cache_file, sizeof(init_cache_file), "%s/root_sdcard.ini",
                         MUOS_CACHE_DIR);
                snprintf(init_meta_dir, sizeof(init_meta_dir), "%s/",
                         MUOS_CORE_DIR);
            }
            break;
        case USB:
            if (strcasecmp(sd_dir, strip_dir(E_USB)) != 0) {
                snprintf(init_cache_file, sizeof(init_cache_file), "%s/usb/%s.ini",
                         MUOS_CACHE_DIR, strchr(strdup(sd_dir), '/') + strlen(E_USB));
                snprintf(init_meta_dir, sizeof(init_meta_dir), "%s/%s/",
                         MUOS_CORE_DIR, strchr(strdup(sd_dir), '/') + strlen(E_USB));
            } else {
                snprintf(init_cache_file, sizeof(init_cache_file), "%s/root_usb.ini",
                         MUOS_CACHE_DIR);
                snprintf(init_meta_dir, sizeof(init_meta_dir), "%s/",
                         MUOS_CORE_DIR);
            }
            break;
        default:
            break;
    }

    create_directories(strip_dir(init_cache_file));
    create_directories(init_meta_dir);

    int is_cache = 0;
    if (file_exist(init_cache_file)) {
        is_cache = 1;
    }

    for (int i = 0; i < file_count; i++) {
        char curr_item[MAX_BUFFER_SIZE];
        const char *fn_name;

        push_string(&content_items, file_names[i]);

        if (is_cache) {
            fn_name = read_line_from_file(init_cache_file, i + 1);
        } else {
            fn_name = get_friendly_name(file_names[i], MUOS_NAME_FILE);
            char good_fn_name[MAX_BUFFER_SIZE];
            snprintf(good_fn_name, sizeof(good_fn_name), "%s\n", fn_name);
            write_text_to_file(init_cache_file, good_fn_name, "a");
        }

        snprintf(curr_item, sizeof(curr_item), "%s :: %d",
                 strip_ext((char *) fn_name), ui_count);

        ui_count++;
        push_string(&named_items, curr_item);
    }

    switch (module) {
        case HISTORY:
            qsort(named_items.array, named_items.size, sizeof(char *), time_compare_for_history);
            break;
        default:
            qsort(named_items.array, named_items.size, sizeof(char *), str_compare);
            break;
    }

    for (int i = 0; i < named_items.size; i++) {
        char n_index[MAX_BUFFER_SIZE];
        snprintf(n_index, sizeof(n_index), "%d", get_label_placement(named_items.array[i]));

        push_string(&named_index, n_index);

        char *item_name = strip_label_placement(named_items.array[i]);
        if (strcasecmp(item_name, DUMMY_DIR) != 0) {
            gen_label("\uF15B", item_name);
        }
    }
}

void create_root_items(char *dir_name) {
    char spec_dir[PATH_MAX];
    snprintf(spec_dir, sizeof(spec_dir), "/%s/%s", MUOS_INFO_PATH, dir_name);

    char **dir_names = NULL;
    int dir_count = 0;

    char **file_names = NULL;
    int file_count = 0;

    switch (module) {
        case FAVOURITE:
            lv_label_set_text(ui_lblTitle, "FAVOURITES");
            break;
        case HISTORY:
            lv_label_set_text(ui_lblTitle, "HISTORY");
            break;
        default:
            lv_label_set_text(ui_lblTitle, "EXPLORE");
            break;
    }

    add_directory_and_file_names(spec_dir, &dir_names, &dir_count, &file_names, &file_count);

    if (dir_count > 0 || file_count > 0) {
        gen_item(file_names, file_count);
    }
}

void create_explore_items() {
    char curr_dir[PATH_MAX];
    snprintf(curr_dir, sizeof(curr_dir), "%s", sd_dir);

    char **dir_names = NULL;
    int dir_count = 0;

    char **file_names = NULL;
    int file_count = 0;

    switch (module) {
        case MMC:
            lv_label_set_text(ui_lblTitle, "EXPLORE (SD1)");
            break;
        case SDCARD:
            lv_label_set_text(ui_lblTitle, "EXPLORE (SD2)");
            break;
        case USB:
            lv_label_set_text(ui_lblTitle, "EXPLORE (USB)");
            break;
        default:
            lv_label_set_text(ui_lblTitle, "EXPLORE");
            break;
    }

    add_directory_and_file_names(curr_dir, &dir_names, &dir_count, &file_names, &file_count);

    if (dir_count > 0 || file_count > 0) {
        qsort(dir_names, dir_count, sizeof(char *), str_compare);
        for (int i = 0; i < dir_count; i++) {
            char curr_dir[MAX_BUFFER_SIZE];
            snprintf(curr_dir, sizeof(curr_dir), "%s :: %d", DUMMY_DIR, ui_count);

            push_string(&named_items, curr_dir);
            push_string(&content_items, DUMMY_DIR);

            gen_label("\uF07B", dir_names[i]);
            ui_count++;

            free(dir_names[i]);
        }
        free(dir_names);

        gen_item(file_names, file_count);
    }
}

void explore_root() {
    lv_label_set_text(ui_lblTitle, "EXPLORE");
    int single_card = 0;

    if (count_items(SD1, DIRECTORIES_ONLY) > 0) {
        single_card += 2;
    }

    if (detect_sd2() && count_items(SD2, DIRECTORIES_ONLY) > 0) {
        single_card += 4;
    }

    if (detect_e_usb() && count_items(E_USB, DIRECTORIES_ONLY) > 0) {
        single_card += 8;
    }

    switch (single_card) {
        case 2:
            write_text_to_file("/tmp/explore_card", "mmc", "w");
            write_text_to_file("/tmp/explore_dir", strip_dir(SD1), "w");
            write_text_to_file("/tmp/single_card", "", "w");
            load_mux("explore");
            safe_quit = 1;
            break;
        case 4:
            write_text_to_file("/tmp/explore_card", "sdcard", "w");
            write_text_to_file("/tmp/explore_dir", strip_dir(SD2), "w");
            write_text_to_file("/tmp/single_card", "", "w");
            load_mux("explore");
            safe_quit = 1;
            break;
        case 6:
            gen_label("\uF07B", "SD1 (mmc)");
            gen_label("\uF07B", "SD2 (sdcard)");
            ui_count += 2;
            break;
        case 8:
            write_text_to_file("/tmp/explore_card", "usb", "w");
            write_text_to_file("/tmp/explore_dir", strip_dir(E_USB), "w");
            write_text_to_file("/tmp/single_card", "", "w");
            load_mux("explore");
            safe_quit = 1;
            break;
        case 10:
            gen_label("\uF07B", "SD1 (mmc)");
            gen_label("\uF07B", "USB (external)");
            ui_count += 2;
            break;
        case 12:
            gen_label("\uF07B", "SD2 (sdcard)");
            gen_label("\uF07B", "USB (external)");
            ui_count += 2;
            break;
        case 14:
            gen_label("\uF07B", "SD1 (mmc)");
            gen_label("\uF07B", "SD2 (sdcard)");
            gen_label("\uF07B", "USB (external)");
            ui_count += 3;
            break;
        default:
            nav_moved = 0;
            lv_obj_clear_flag(ui_lblExploreMessage, LV_OBJ_FLAG_HIDDEN);
            break;
    }
}

void prepare_activity_file(char *act_content, char *act_path) {
    if (!file_exist(act_path)) {
        char activity_content[MAX_BUFFER_SIZE];
        snprintf(activity_content, sizeof(activity_content), "%s\n0\n0", act_content);
        write_text_to_file(act_path, activity_content, "w");
    }
}

int load_content(char *content_name, int content_index, int add_favourite) {
    char *assigned_core = load_content_core(0);
    printf("ASSIGNED CORE: %s\n", assigned_core);

    if (assigned_core == NULL) {
        lv_label_set_text(ui_lblMessage, osd_message);
        osd_message = "Cannot associate core to this folder!";
        lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);

        return 0;
    }

    char content_loader_file[MAX_BUFFER_SIZE];
    snprintf(content_loader_file, sizeof(content_loader_file), "%s/%s/%s.cfg",
             MUOS_CORE_DIR, get_last_subdir(sd_dir, '/', 4), content_name);

    printf("CONFIG FILE: %s\n", content_loader_file);

    char *curr_sd;
    switch (module) {
        case USB:
            curr_sd = "usb";
            break;
        case SDCARD:
            curr_sd = "sdcard";
            break;
        default:
            curr_sd = "mmc";
            break;
    }

    if (!file_exist(content_loader_file)) {
        char content_loader_data[MAX_BUFFER_SIZE];
        snprintf(content_loader_data, sizeof(content_loader_data), "%s\n%s\n/mnt/%s/roms/\n%s\n%s\n",
                 content_name, assigned_core, curr_sd,
                 get_last_subdir(sd_dir, '/', 4),
                 get_string_at_index(&content_items, content_index));

        write_text_to_file(content_loader_file, content_loader_data, "w");
        printf("\nCONFIG DATA\n%s\n", content_loader_data);
    }

    if (file_exist(content_loader_file)) {
        char add_to_hf[MAX_BUFFER_SIZE];
        char *hf_type;

        if (add_favourite) {
            hf_type = MUOS_FAVOURITE_DIR;
        } else {
            hf_type = MUOS_HISTORY_DIR;
        }

        snprintf(add_to_hf, sizeof(add_to_hf), "%s/%s.cfg", hf_type, content_name);

        if (add_favourite) {
            write_text_to_file(add_to_hf, read_text_from_file(content_loader_file), "w");

            if (file_exist(add_to_hf)) {
                osd_message = "Added to favourites!";
            } else {
                osd_message = "Could not add to favourites!";
            }

            lv_label_set_text(ui_lblMessage, osd_message);
            lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);

            return 1;
        } else {
            printf("TRYING TO LOAD CONTENT...\n");

            char act_file[MAX_BUFFER_SIZE];
            char act_content[MAX_BUFFER_SIZE];
            snprintf(act_file, sizeof(act_file), "%s/%s.act",
                     MUOS_ACTIVITY_DIR, content_name);
            snprintf(act_content, sizeof(act_content), "%s\n%s\n%s",
                     content_name, curr_sd, read_line_from_file(content_loader_file, 5));
            prepare_activity_file(act_content, act_file);

            write_text_to_file(add_to_hf, read_text_from_file(content_loader_file), "w");
            write_text_to_file(MUOS_ROM_LOAD, read_text_from_file(content_loader_file), "w");
        }

        printf("CONTENT LOADED SUCCESSFULLY\n");
        return 1;
    }

    osd_message = "Could not load content!";
    lv_label_set_text(ui_lblMessage, osd_message);
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);

    return 0;
}

int load_cached_content(char *content_name, char *cache_type) {
    char cache_file[MAX_BUFFER_SIZE];
    snprintf(cache_file, sizeof(cache_file), "/%s/%s/%s.cfg",
             MUOS_INFO_PATH, cache_type, content_name);

    if (file_exist(cache_file)) {
        char add_to_hf[MAX_BUFFER_SIZE];

        char *assigned_core = read_line_from_file(cache_file, 2);
        printf("ASSIGNED CORE: %s\n", assigned_core);

        printf("CONFIG FILE: %s\n", cache_file);

        snprintf(add_to_hf, sizeof(add_to_hf), "%s/%s.cfg",
                 MUOS_HISTORY_DIR, content_name);

        printf("TRYING TO LOAD CONTENT...\n");

        char *curr_sd;
        switch (module) {
            case USB:
                curr_sd = "usb";
                break;
            case SDCARD:
                curr_sd = "sdcard";
                break;
            default:
                curr_sd = "mmc";
                break;
        }

        char act_file[MAX_BUFFER_SIZE];
        char act_content[MAX_BUFFER_SIZE];
        snprintf(act_file, sizeof(act_file), "%s/%s.act",
                 MUOS_ACTIVITY_DIR, content_name);
        snprintf(act_content, sizeof(act_content), "%s\n%s\n%s",
                 content_name, curr_sd, read_line_from_file(cache_file, 5));
        prepare_activity_file(act_content, act_file);

        write_text_to_file(add_to_hf, read_text_from_file(cache_file), "w");
        write_text_to_file(MUOS_ROM_LOAD, read_text_from_file(cache_file), "w");

        printf("CONTENT LOADED SUCCESSFULLY\n");
        return 1;
    }

    osd_message = "Could not load content!";
    lv_label_set_text(ui_lblMessage, osd_message);
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);

    return 0;
}

void list_nav_prev(int steps) {
    for (int step = 0; step < steps; ++step) {
        if (current_item_index >= 1 && ui_count > 13) {
            current_item_index--;
            nav_prev(ui_group, 1);
            nav_prev(ui_group_glyph, 1);
            if (current_item_index > 5 && current_item_index < (ui_count - 7)) {
                content_panel_y -= 30;
                lv_obj_scroll_to_y(ui_pnlContent, content_panel_y, LV_ANIM_OFF);
            }
        } else if (current_item_index >= 0 && ui_count <= 13) {
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
        if (current_item_index < (ui_count - 1) && ui_count > 13) {
            if (current_item_index < (ui_count - 1)) {
                current_item_index++;
                nav_next(ui_group, 1);
                nav_next(ui_group_glyph, 1);
                if (current_item_index >= 7 && current_item_index < (ui_count - 6)) {
                    content_panel_y += 30;
                    lv_obj_scroll_to_y(ui_pnlContent, content_panel_y, LV_ANIM_OFF);
                }
            }
        } else if (current_item_index < ui_count && ui_count <= 13) {
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

void cache_message(char *n_dir) {
    char cache_file[MAX_BUFFER_SIZE];
    switch (module) {
        case MMC:
            snprintf(cache_file, sizeof(cache_file), "%s/mmc/%s.ini", MUOS_CACHE_DIR,
                     get_last_subdir(n_dir, '/', 4));
            break;
        case SDCARD:
            snprintf(cache_file, sizeof(cache_file), "%s/sdcard/%s.ini", MUOS_CACHE_DIR,
                     get_last_subdir(n_dir, '/', 4));
            break;
        case USB:
            snprintf(cache_file, sizeof(cache_file), "%s/usb/%s.ini", MUOS_CACHE_DIR,
                     get_last_subdir(n_dir, '/', 4));
            break;
        case FAVOURITE:
            snprintf(cache_file, sizeof(cache_file), "%s/favourite.ini", MUOS_CACHE_DIR);
            break;
        case HISTORY:
            snprintf(cache_file, sizeof(cache_file), "%s/history.ini", MUOS_CACHE_DIR);
            break;
        default:
            break;
    }
    if (file_exist(cache_file)) {
        printf("LOADING CACHE AT: %s\n", cache_file);
    } else {
        printf("NO CACHE FOUND AT: %s\n", cache_file);
        osd_message = "Caching Directory...";
        lv_label_set_text(ui_lblMessage, osd_message);
        lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
        usleep(UINT16_MAX);
    }
}

void *joystick_task() {
    struct input_event ev;
    int epoll_fd;
    struct epoll_event event, events[MAX_EVENTS];

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
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, nav_delay);
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

                switch (ev.type) {
                    case EV_KEY:
                        if (ev.value == 1) {
                            if (msgbox_active) {
                                if (ev.code == NAV_B || ev.code == JOY_MENU) {
                                    play_sound("confirm", nav_sound);
                                    msgbox_active = 0;
                                    progress_onscreen = 0;
                                    lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
                                    lv_img_set_src(ui_imgHelpPreviewImage, &ui_img_nothing_png);
                                } else if (ev.code == NAV_A) {
                                    play_sound("confirm", nav_sound);
                                    if (lv_obj_has_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN)) {
                                        lv_obj_add_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
                                    } else {
                                        lv_obj_add_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
                                    }
                                }
                            } else {
                                if (ev.code == JOY_MENU) {
                                    JOYHOTKEY_pressed = 1;
                                } else if (ev.code == NAV_A) {
                                    if (ui_count == 0) {
                                        goto nothing_ever_happens;
                                    }

                                    play_sound("confirm", nav_sound);

                                    switch (module) {
                                        case ROOT:
                                            if (strcasecmp(lv_label_get_text(lv_group_get_focused(ui_group)),
                                                           "SD1 (mmc)") ==
                                                0) {
                                                write_text_to_file("/tmp/explore_card", "mmc", "w");
                                                write_text_to_file("/tmp/explore_dir", strip_dir(SD1), "w");
                                                load_mux("explore");
                                                break;
                                            } else if (strcasecmp(lv_label_get_text(lv_group_get_focused(ui_group)),
                                                                  "SD2 (sdcard)") == 0) {
                                                write_text_to_file("/tmp/explore_card", "sdcard", "w");
                                                write_text_to_file("/tmp/explore_dir", strip_dir(SD2), "w");
                                                load_mux("explore");
                                                break;
                                            } else if (strcasecmp(lv_label_get_text(lv_group_get_focused(ui_group)),
                                                                  "USB (external)") == 0) {
                                                write_text_to_file("/tmp/explore_card", "usb", "w");
                                                write_text_to_file("/tmp/explore_dir", strip_dir(E_USB), "w");
                                                load_mux("explore");
                                                break;
                                            }
                                            break;
                                        default:
                                            char *f_content = get_string_at_index(&content_items, atoi(
                                                    get_string_at_index(&named_index, current_item_index)));
                                            char *f_name = get_string_at_index(&named_items, current_item_index);
                                            printf("CONTENT FILENAME: %s\n", f_content);
                                            printf("CONTENT RAW NAME: %s\n", f_name);

                                            switch (module) {
                                                case MMC:
                                                case SDCARD:
                                                case USB:
                                                    if (strcasecmp(f_content, DUMMY_DIR) == 0) {
                                                        char n_dir[MAX_BUFFER_SIZE];
                                                        snprintf(n_dir, sizeof(n_dir), "%s/%s",
                                                                 sd_dir,
                                                                 lv_label_get_text(lv_group_get_focused(ui_group)));

                                                        write_text_to_file("/tmp/explore_dir", n_dir, "w");
                                                        load_mux("explore");

                                                        switch (module) {
                                                            case MMC:
                                                                cache_message(n_dir);
                                                                break;
                                                            case SDCARD:
                                                                cache_message(n_dir);
                                                                break;
                                                            case USB:
                                                                cache_message(n_dir);
                                                                break;
                                                            default:
                                                                break;
                                                        }
                                                        break;
                                                    } else {
                                                        char c_index[MAX_BUFFER_SIZE];
                                                        snprintf(c_index, sizeof(c_index), "%d",
                                                                 current_item_index);
                                                        write_text_to_file("/tmp/mux_lastindex_rom", c_index, "w");

                                                        if (load_content(f_name, atoi(
                                                                get_string_at_index(&named_index,
                                                                                    current_item_index)), 0)) {
                                                            system(MUOS_CONTENT_LAUNCH);
                                                        }
                                                        break;
                                                    }
                                                    break;
                                                case FAVOURITE:
                                                    load_cached_content(
                                                            lv_label_get_text(lv_group_get_focused(ui_group)),
                                                            "favourite");
                                                    write_text_to_file("/tmp/explore_card", "favourite", "w");
                                                    remove("/tmp/explore_dir");
                                                    break;
                                                case HISTORY:
                                                    load_cached_content(
                                                            lv_label_get_text(lv_group_get_focused(ui_group)),
                                                            "history");
                                                    write_text_to_file("/tmp/explore_card", "history", "w");
                                                    remove("/tmp/explore_dir");
                                                default:
                                                    break;
                                            }
                                            break;
                                    }
                                    safe_quit = 1;
                                } else if (ev.code == NAV_B) {
                                    play_sound("back", nav_sound);

                                    switch (module) {
                                        case FAVOURITE:
                                            write_text_to_file("/tmp/explore_card", "root", "w");
                                            remove("/tmp/explore_dir");
                                            load_mux("launcher");
                                            break;
                                        case HISTORY:
                                            write_text_to_file("/tmp/explore_card", "root", "w");
                                            remove("/tmp/explore_dir");
                                            load_mux("launcher");
                                            break;
                                        default:
                                            if (sd_dir != NULL) {
                                                char *b_dir = strrchr(sd_dir, '/');
                                                if (b_dir != NULL) {
                                                    if (strcasecmp(str_tolower(b_dir), "/roms") == 0) {
                                                        if (file_exist("/tmp/single_card")) {
                                                            write_text_to_file("/tmp/explore_card", "root", "w");
                                                            remove("/tmp/explore_dir");
                                                            remove("/tmp/single_card");
                                                            load_mux("launcher");
                                                        } else {
                                                            write_text_to_file("/tmp/explore_card", "root", "w");
                                                            write_text_to_file("/tmp/explore_dir", "", "w");
                                                            load_mux("explore");
                                                        }
                                                    } else {
                                                        write_text_to_file("/tmp/explore_dir",
                                                                           strndup(sd_dir, b_dir - sd_dir), "w");
                                                        load_mux("explore");
                                                    }

                                                }
                                            }
                                            break;
                                    }
                                    safe_quit = 1;
                                } else if (ev.code == JOY_X) {
                                    if (ui_count == 0) {
                                        goto nothing_ever_happens;
                                    }

                                    char n_dir[MAX_BUFFER_SIZE];
                                    snprintf(n_dir, sizeof(n_dir), "%s", sd_dir);

                                    char cache_file[MAX_BUFFER_SIZE];
                                    switch (module) {
                                        case MMC:
                                            play_sound("confirm", nav_sound);

                                            snprintf(cache_file, sizeof(cache_file), "%s/mmc/%s.ini",
                                                     MUOS_CACHE_DIR, get_last_subdir(n_dir, '/', 4));

                                            write_text_to_file("/tmp/explore_card", "mmc", "w");
                                            break;
                                        case SDCARD:
                                            play_sound("confirm", nav_sound);

                                            snprintf(cache_file, sizeof(cache_file), "%s/sdcard/%s.ini",
                                                     MUOS_CACHE_DIR, get_last_subdir(n_dir, '/', 4));

                                            write_text_to_file("/tmp/explore_card", "sdcard", "w");
                                            break;
                                        case USB:
                                            play_sound("confirm", nav_sound);

                                            snprintf(cache_file, sizeof(cache_file), "%s/usb/%s.ini",
                                                     MUOS_CACHE_DIR, get_last_subdir(n_dir, '/', 4));

                                            write_text_to_file("/tmp/explore_card", "usb", "w");
                                            break;
                                        case FAVOURITE:
                                            play_sound("confirm", nav_sound);

                                            snprintf(cache_file, sizeof(cache_file), "/%s/favourite/%s.cfg",
                                                     MUOS_INFO_PATH,
                                                     lv_label_get_text(lv_group_get_focused(ui_group)));

                                            remove(cache_file);
                                            write_text_to_file("/tmp/mux_reload", "1", "w");

                                            goto ttq;
                                            break;
                                        case HISTORY:
                                            play_sound("confirm", nav_sound);

                                            snprintf(cache_file, sizeof(cache_file), "/%s/history/%s.cfg",
                                                     MUOS_INFO_PATH,
                                                     lv_label_get_text(lv_group_get_focused(ui_group)));

                                            remove(cache_file);
                                            write_text_to_file("/tmp/mux_reload", "1", "w");

                                            goto ttq;
                                            break;
                                        default:
                                            goto nothing_ever_happens;
                                    }

                                    if (file_exist(cache_file)) {
                                        remove(cache_file);
                                        cache_message(n_dir);
                                    }

                                    write_text_to_file("/tmp/explore_dir", n_dir, "w");
                                    load_mux("explore");

                                    ttq:
                                    safe_quit = 1;
                                } else if (ev.code == JOY_Y) {
                                    play_sound("confirm", nav_sound);

                                    switch (module) {
                                        case MMC:
                                        case SDCARD:
                                        case USB:
                                            char *f_content = get_string_at_index(&content_items, atoi(
                                                    get_string_at_index(&named_index, current_item_index)));
                                            char *f_name = get_string_at_index(&named_items, current_item_index);
                                            printf("CONTENT FILENAME: %s\n", f_content);
                                            printf("CONTENT RAW NAME: %s\n", f_name);

                                            if (strcasecmp(f_content, DUMMY_DIR) == 0) {
                                                osd_message = "Directories cannot be added to Favourites";
                                                lv_label_set_text(ui_lblMessage, osd_message);
                                                lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
                                                break;
                                            } else {
                                                if (load_content(f_name,
                                                                 atoi(get_string_at_index(&named_index,
                                                                                          current_item_index)),
                                                                 1)) {
                                                    osd_message = "Added to Favourites";
                                                    lv_label_set_text(ui_lblMessage, osd_message);
                                                    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
                                                } else {
                                                    osd_message = "Error adding to Favourites";
                                                    lv_label_set_text(ui_lblMessage, osd_message);
                                                    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
                                                }
                                                if (file_exist(MUOS_ROM_LOAD)) {
                                                    remove(MUOS_ROM_LOAD);
                                                }
                                                break;
                                            }
                                            break;
                                        default:
                                            break;
                                    }
                                } else if (ev.code == JOY_START) {
                                    switch (module) {
                                        case MMC:
                                        case SDCARD:
                                        case USB:
                                            play_sound("confirm", nav_sound);

                                            write_text_to_file("/tmp/explore_card", "root", "w");
                                            remove("/tmp/explore_dir");
                                            load_mux("explore");

                                            safe_quit = 1;
                                            break;
                                        default:
                                            break;
                                    }
                                } else if (ev.code == JOY_SELECT) {
                                    if (ui_file_count > 0) {
                                        play_sound("confirm", nav_sound);

                                        switch (module) {
                                            case MMC:
                                            case SDCARD:
                                            case USB:
                                                load_content_core(1);
                                                break;
                                            default:
                                                break;
                                        }
                                    }
                                } else if (ev.code == JOY_L1) {
                                    if (current_item_index >= 0 && current_item_index < ui_count) {
                                        list_nav_prev(ITEM_SKIP);
                                        lv_task_handler();
                                    }
                                } else if (ev.code == JOY_R1) {
                                    if (current_item_index >= 0 && current_item_index < ui_count) {
                                        list_nav_next(ITEM_SKIP);
                                        lv_task_handler();
                                    }
                                }
                            }
                        } else {
                            if (ev.code == JOY_MENU) {
                                JOYHOTKEY_pressed = 0;
                                if (progress_onscreen == -1) {
                                    if (ui_count == 0) {
                                        goto nothing_ever_happens;
                                    }

                                    play_sound("confirm", nav_sound);
                                    image_refresh("preview");

                                    lv_obj_add_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
                                    lv_obj_clear_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
                                    lv_obj_clear_flag(ui_pnlHelp, LV_OBJ_FLAG_HIDDEN);

                                    static lv_anim_t desc_anim;
                                    static lv_style_t desc_style;
                                    lv_anim_init(&desc_anim);
                                    lv_anim_set_delay(&desc_anim, 2000);
                                    lv_style_init(&desc_style);
                                    lv_style_set_anim(&desc_style, &desc_anim);
                                    lv_obj_add_style(ui_lblHelpDescription, &desc_style, LV_PART_MAIN);
                                    lv_obj_set_style_anim_speed(ui_lblHelpDescription, 25, LV_PART_MAIN);

                                    show_rom_info(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpPreviewHeader,
                                                  ui_lblHelpDescription,
                                                  lv_label_get_text(lv_group_get_focused(ui_group)),
                                                  load_content_description());
                                }
                            }
                        }
                    case EV_ABS:
                        if (msgbox_active) {
                            break;
                        }
                        if (ev.code == NAV_DPAD_VER || ev.code == NAV_ANLG_VER) {
                            switch (ev.value) {
                                case -4096:
                                case -1:
                                    if (current_item_index == 0) {
                                        int y = (ui_count - 13) * 30;
                                        lv_obj_scroll_to_y(ui_pnlContent, y, LV_ANIM_OFF);
                                        content_panel_y = y;
                                        current_item_index = ui_count - 1;
                                        nav_prev(ui_group, 1);
                                        nav_prev(ui_group_glyph, 1);
                                        image_refresh("box");
                                        lv_task_handler();
                                    } else if (current_item_index > 0) {
                                        JOYUP_pressed = (ev.value != 0);
                                        list_nav_prev(1);
                                        lv_task_handler();
                                    }
                                    break;
                                case 1:
                                case 4096:
                                    if (current_item_index == ui_count - 1) {
                                        lv_obj_scroll_to_y(ui_pnlContent, 0, LV_ANIM_OFF);
                                        content_panel_y = 0;
                                        current_item_index = 0;
                                        nav_next(ui_group, 1);
                                        nav_next(ui_group_glyph, 1);
                                        image_refresh("box");
                                        lv_task_handler();
                                    } else if (current_item_index < ui_count) {
                                        JOYDOWN_pressed = (ev.value != 0);
                                        list_nav_next(1);
                                        lv_task_handler();
                                    }
                                    break;
                                default:
                                    JOYUP_pressed = 0;
                                    JOYDOWN_pressed = 0;
                                    break;
                            }
                        }
                    default:
                    nothing_ever_happens:
                        break;
                }
            }
        }

        if (ui_count > 13 && (JOYUP_pressed || JOYDOWN_pressed)) {
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

        if (ev.type == EV_KEY && ev.value == 1 && (ev.code == JOY_MINUS || ev.code == JOY_PLUS)) {
            progress_onscreen = 1;
            if (lv_obj_has_flag(ui_pnlProgress, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_clear_flag(ui_pnlProgress, LV_OBJ_FLAG_HIDDEN);
            }
            if (JOYHOTKEY_pressed) {
                lv_label_set_text(ui_icoProgress, "\uF185");
                lv_bar_set_value(ui_barProgress, get_brightness_percentage(get_brightness()), LV_ANIM_OFF);
            } else {
                int volume = get_volume_percentage();
                switch (volume) {
                    case 0:
                        lv_label_set_text(ui_icoProgress, "\uF6A9");
                        break;
                    case 1 ... 46:
                        lv_label_set_text(ui_icoProgress, "\uF026");
                        break;
                    case 47 ... 71:
                        lv_label_set_text(ui_icoProgress, "\uF027");
                        break;
                    case 72 ... 100:
                        lv_label_set_text(ui_icoProgress, "\uF028");
                        break;
                }
                lv_bar_set_value(ui_barProgress, volume, LV_ANIM_OFF);
            }
        }

        lv_task_handler();
        usleep(SCREEN_WAIT);
    }
}

void init_elements() {
    switch (get_ini_int(muos_config, "visual", "boxart", LABEL)) {
        case 0: // Bottom + Behind
            lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
            lv_obj_move_background(ui_pnlBox);
            lv_obj_move_background(ui_pnlWall);
            break;
        case 1: // Bottom + Front
            lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
            lv_obj_move_foreground(ui_pnlBox);
            break;
        case 2: // Middle + Behind
            lv_obj_set_align(ui_imgBox, LV_ALIGN_RIGHT_MID);
            lv_obj_move_background(ui_pnlBox);
            lv_obj_move_background(ui_pnlWall);
            break;
        case 3: // Middle + Front
            lv_obj_set_align(ui_imgBox, LV_ALIGN_RIGHT_MID);
            lv_obj_move_foreground(ui_pnlBox);
            break;
        case 4: // Top + Behind
            lv_obj_set_align(ui_imgBox, LV_ALIGN_TOP_RIGHT);
            lv_obj_move_background(ui_pnlBox);
            lv_obj_move_background(ui_pnlWall);
            break;
        case 5: // Top + Front
            lv_obj_set_align(ui_imgBox, LV_ALIGN_TOP_RIGHT);
            lv_obj_move_foreground(ui_pnlBox);
            break;
        case 6: // Fullscreen + Behind
            lv_obj_set_height(ui_pnlBox, SCREEN_HEIGHT);
            lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
            lv_obj_move_background(ui_pnlBox);
            lv_obj_move_background(ui_pnlWall);
            break;
        case 7: // Fullscreen + Front
            lv_obj_set_height(ui_pnlBox, SCREEN_HEIGHT);
            lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
            lv_obj_move_foreground(ui_pnlBox);
            break;
    }

    lv_obj_move_foreground(ui_pnlFooter);
    lv_obj_move_foreground(ui_pnlHeader);
    lv_obj_move_foreground(ui_pnlHelp);
    lv_obj_move_foreground(ui_pnlProgress);

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

    switch (module) {
        case ROOT:
            lv_label_set_text(ui_lblNavA, "Open");
            lv_label_set_text(ui_lblNavB, "Back");
            lv_label_set_text(ui_lblNavMenu, "Info");
            lv_obj_t *nav_keep[] = {
                    ui_lblNavAGlyph, ui_lblNavA,
                    ui_lblNavBGlyph, ui_lblNavB,
                    ui_lblNavMenuGlyph, ui_lblNavMenu
            };
            lv_obj_t *nav_hide[] = {
                    ui_lblNavCGlyph, ui_lblNavC,
                    ui_lblNavXGlyph, ui_lblNavX,
                    ui_lblNavYGlyph, ui_lblNavY,
                    ui_lblNavZGlyph, ui_lblNavZ
            };
            for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
                lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
            }
            for (int i = 0; i < sizeof(nav_keep) / sizeof(nav_keep[0]); i++) {
                lv_obj_clear_flag(nav_keep[i], LV_OBJ_FLAG_HIDDEN);
            }
            break;
        case MMC:
        case SDCARD:
        case USB:
            lv_label_set_text(ui_lblNavA, "Open");
            lv_label_set_text(ui_lblNavB, "Back");
            lv_label_set_text(ui_lblNavX, "Refresh");
            lv_label_set_text(ui_lblNavY, "Favourite");
            lv_label_set_text(ui_lblNavMenu, "Info");
            if (lv_group_get_obj_count(ui_group) <= 0) {
                lv_obj_t *nav_keep[] = {
                        ui_lblNavBGlyph, ui_lblNavB
                };
                lv_obj_t *nav_hide[] = {
                        ui_lblNavAGlyph, ui_lblNavA,
                        ui_lblNavCGlyph, ui_lblNavC,
                        ui_lblNavXGlyph, ui_lblNavX,
                        ui_lblNavYGlyph, ui_lblNavY,
                        ui_lblNavZGlyph, ui_lblNavZ,
                        ui_lblNavMenuGlyph, ui_lblNavMenu
                };
                for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
                    lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
                }
                for (int i = 0; i < sizeof(nav_keep) / sizeof(nav_keep[0]); i++) {
                    lv_obj_clear_flag(nav_keep[i], LV_OBJ_FLAG_HIDDEN);
                }
            } else {
                lv_obj_t *nav_keep[] = {
                        ui_lblNavAGlyph, ui_lblNavA,
                        ui_lblNavBGlyph, ui_lblNavB,
                        ui_lblNavXGlyph, ui_lblNavX,
                        ui_lblNavYGlyph, ui_lblNavY,
                        ui_lblNavMenuGlyph, ui_lblNavMenu
                };
                lv_obj_t *nav_hide[] = {
                        ui_lblNavCGlyph, ui_lblNavC,
                        ui_lblNavZGlyph, ui_lblNavZ
                };
                for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
                    lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
                }
                for (int i = 0; i < sizeof(nav_keep) / sizeof(nav_keep[0]); i++) {
                    lv_obj_clear_flag(nav_keep[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
            break;
        case FAVOURITE:
        case HISTORY:
            lv_label_set_text(ui_lblNavA, "Open");
            lv_label_set_text(ui_lblNavB, "Back");
            lv_label_set_text(ui_lblNavX, "Remove");
            lv_label_set_text(ui_lblNavMenu, "Info");
            if (lv_group_get_obj_count(ui_group) <= 0) {
                lv_obj_t *nav_keep[] = {
                        ui_lblNavBGlyph, ui_lblNavB
                };
                lv_obj_t *nav_hide[] = {
                        ui_lblNavAGlyph, ui_lblNavA,
                        ui_lblNavCGlyph, ui_lblNavC,
                        ui_lblNavXGlyph, ui_lblNavX,
                        ui_lblNavYGlyph, ui_lblNavY,
                        ui_lblNavZGlyph, ui_lblNavZ,
                        ui_lblNavMenuGlyph, ui_lblNavMenu
                };
                for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
                    lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
                }
                for (int i = 0; i < sizeof(nav_keep) / sizeof(nav_keep[0]); i++) {
                    lv_obj_clear_flag(nav_keep[i], LV_OBJ_FLAG_HIDDEN);
                }
            } else {
                lv_obj_t *nav_keep[] = {
                        ui_lblNavAGlyph, ui_lblNavA,
                        ui_lblNavBGlyph, ui_lblNavB,
                        ui_lblNavXGlyph, ui_lblNavX,
                        ui_lblNavMenuGlyph, ui_lblNavMenu
                };
                lv_obj_t *nav_hide[] = {
                        ui_lblNavCGlyph, ui_lblNavC,
                        ui_lblNavYGlyph, ui_lblNavY,
                        ui_lblNavZGlyph, ui_lblNavZ
                };
                for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
                    lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
                }
                for (int i = 0; i < sizeof(nav_keep) / sizeof(nav_keep[0]); i++) {
                    lv_obj_clear_flag(nav_keep[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
            break;
        default:
            break;
    }
}

void init_fonts() {
    load_font_text(mux_prog, ui_scrExplore);
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

    if (progress_onscreen > 0) {
        progress_onscreen -= 1;
    } else {
        if (!lv_obj_has_flag(ui_pnlProgress, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgress, LV_OBJ_FLAG_HIDDEN);
        }
        if (!msgbox_active) {
            progress_onscreen = -1;
        }
    }
}

void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            static char old_wall[MAX_BUFFER_SIZE];
            static char new_wall[MAX_BUFFER_SIZE];

            snprintf(old_wall, sizeof(old_wall), "%s", current_wall);
            snprintf(new_wall, sizeof(new_wall), "%s", load_wallpaper(
                    ui_scrExplore, ui_group, theme.MISC.ANIMATED_BACKGROUND));

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

            static char static_image[MAX_BUFFER_SIZE];
            snprintf(static_image, sizeof(static_image), "%s",
                     load_static_image(ui_scrExplore, ui_group));

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
                        lv_obj_set_height(ui_pnlBox, SCREEN_HEIGHT);
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_background(ui_pnlBox);
                        lv_obj_move_background(ui_pnlWall);
                        break;
                    case 4: // Fullscreen + Front
                        lv_obj_set_height(ui_pnlBox, SCREEN_HEIGHT);
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                }

                lv_img_set_src(ui_imgBox, static_image);
            } else {
                lv_img_set_src(ui_imgBox, &ui_img_nothing_png);
            }
        }
        image_refresh("box");
        lv_obj_invalidate(ui_pnlBox);

        snprintf(current_content_label, sizeof(current_content_label), "%s",
                 lv_label_get_text(lv_group_get_focused(ui_group)));
        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    int sys_index = -1;
    int opt;
    while ((opt = getopt(argc, argv, "i:m:")) != -1) {
        switch (opt) {
            case 'i':
                sys_index = atoi(optarg);
                break;
            case 'm':
                if (strcasecmp(optarg, "root") == 0) {
                    mux_prog = "muxplore";
                    module = ROOT;
                    break;
                } else if (strcasecmp(optarg, "mmc") == 0) {
                    mux_prog = "muxplore";
                    module = MMC;
                    break;
                } else if (strcasecmp(optarg, "sdcard") == 0) {
                    mux_prog = "muxplore";
                    module = SDCARD;
                    break;
                } else if (strcasecmp(optarg, "usb") == 0) {
                    mux_prog = "muxplore";
                    module = USB;
                    break;
                } else if (strcasecmp(optarg, "favourite") == 0) {
                    mux_prog = "muxfavourite";
                    module = FAVOURITE;
                    break;
                } else if (strcasecmp(optarg, "history") == 0) {
                    mux_prog = "muxhistory";
                    module = HISTORY;
                    break;
                } else {
                    printf("Unknown module: %s\n", optarg);
                    return 1;
                }
            default:
                fprintf(stderr, "\nmuOS Extras - System List\nUsage: %s <-im>\n\nOptions:\n"
                                "\t-i Index of content to skip to\n"
                                "\t-m List Module:\n"
                                "\t\troot - List SD Cards\n"
                                "\t\tmmc - List directories and files from SD1\n"
                                "\t\tsdcard - List directories and files from SD2\n"
                                "\t\tusb - List directories and files from USB\n"
                                "\t\tfavourite - List favourite items\n"
                                "\t\thistory - List history items\n"
                                "\n", argv[0]);
                return 1;
        }
    }

    if (sys_index == -1) {
        fprintf(stderr, "\nmuOS Extras - System List\nUsage: %s <-im>\n\nOptions:\n"
                        "\t-i Index of content to skip to\n"
                        "\t-m List Module:\n"
                        "\t\troot - List SD Cards\n"
                        "\t\tmmc - List directories and files from SD1\n"
                        "\t\tsdcard - List directories and files from SD2\n"
                        "\t\tusb - List directories and files from USB\n"
                        "\t\tfavourite - List favourite items\n"
                        "\t\thistory - List history items\n"
                        "\n", argv[0]);
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

    lv_obj_set_user_data(ui_scrExplore, mux_prog);

    lv_label_set_text(ui_lblDatetime, get_datetime());
    lv_label_set_text(ui_staCapacity, get_capacity());

    load_theme(&theme, mux_prog);
    load_glyph(&glyph, mux_prog);

    apply_theme();
    init_fonts();

    switch (theme.MISC.NAVIGATION_TYPE) {
        case 1:
            NAV_DPAD_HOR = ABS_HAT0Y;
            NAV_ANLG_HOR = ABS_RX;
            NAV_DPAD_VER = ABS_HAT0X;
            NAV_ANLG_VER = ABS_Z;
            break;
        default:
            NAV_DPAD_HOR = ABS_HAT0X;
            NAV_ANLG_HOR = ABS_Z;
            NAV_DPAD_VER = ABS_HAT0Y;
            NAV_ANLG_VER = ABS_RX;
    }

    switch (mini_get_int(muos_config, "settings.advanced", "swap", LABEL)) {
        case 1:
            NAV_A = JOY_B;
            NAV_B = JOY_A;
            lv_label_set_text(ui_lblNavAGlyph, "\u21D2");
            lv_label_set_text(ui_lblNavBGlyph, "\u21D3");
            lv_label_set_text(ui_lblPreviewHeaderGlyph, "\u21D2");
            lv_label_set_text(ui_lblHelpPreviewInfoGlyph, "\u21D2");
            break;
        default:
            NAV_A = JOY_A;
            NAV_B = JOY_B;
            lv_label_set_text(ui_lblNavAGlyph, "\u21D3");
            lv_label_set_text(ui_lblNavBGlyph, "\u21D2");
            break;
    }

    current_wall = load_wallpaper(ui_scrExplore, NULL, theme.MISC.ANIMATED_BACKGROUND);
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

    if (get_ini_int(muos_config, "settings.general", "sound", LABEL) == 2) {
        nav_sound = 1;
    }

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();

    initialise_array(&content_items);
    initialise_array(&named_items);
    initialise_array(&named_index);

    switch (module) {
        case ROOT:
            explore_root();
            break;
        case MMC:
        case SDCARD:
        case USB:
            char *ex_file = "/tmp/explore_dir";
            char *ex_path = read_line_from_file(ex_file, 1);
            if (file_exist(ex_file) && ex_path != NULL) {
                int sd1_okay = strncmp(ex_path, strip_dir(SD1), strlen(strip_dir(SD1)));
                int sd2_okay = strncmp(ex_path, strip_dir(SD2), strlen(strip_dir(SD2)));
                int usb_okay = strncmp(ex_path, strip_dir(E_USB), strlen(strip_dir(E_USB)));
                if (sd1_okay == 0 || sd2_okay == 0 || usb_okay == 0) {
                    sd_dir = ex_path;
                    create_explore_items();
                } else {
                    explore_root();
                }
            } else {
                switch (module) {
                    case MMC:
                        sd_dir = strip_dir(SD1);
                        create_explore_items();
                        break;
                    case SDCARD:
                        sd_dir = strip_dir(SD2);
                        create_explore_items();
                        break;
                    case USB:
                        sd_dir = strip_dir(E_USB);
                        create_explore_items();
                        break;
                    default:
                        explore_root();
                        break;
                }
            }
            break;
        case FAVOURITE:
            create_root_items("favourite");
            break;
        case HISTORY:
            create_root_items("history");
            break;
    }

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

    datetime_timer = lv_timer_create(datetime_task, UINT16_MAX / 2, &dt_par);
    lv_timer_ready(datetime_timer);

    capacity_timer = lv_timer_create(capacity_task, UINT16_MAX / 2, &bat_par);
    lv_timer_ready(capacity_timer);

    osd_timer = lv_timer_create(osd_task, UINT16_MAX / 32, &osd_par);
    lv_timer_ready(osd_timer);

    glyph_timer = lv_timer_create(glyph_task, UINT16_MAX / 64, NULL);
    lv_timer_ready(glyph_timer);

    ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
    lv_timer_ready(ui_refresh_timer);

    if (ui_count > 0) {
        if (ui_count > 13) {
            lv_obj_t * last_item = lv_obj_get_child(ui_pnlContent, -1);
            lv_obj_set_height(last_item, lv_obj_get_height(last_item) + 50); // Don't bother asking...
        }
        if (sys_index > -1 && sys_index <= ui_count && current_item_index < ui_count) {
            list_nav_next(sys_index);
        } else {
            image_refresh("box");
        }
    } else {
        nav_moved = 0;
        lv_obj_clear_flag(ui_lblExploreMessage, LV_OBJ_FLAG_HIDDEN);
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
