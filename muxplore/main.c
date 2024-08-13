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
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "../common/common.h"
#include "../common/help.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/glyph.h"
#include "../common/array.h"
#include "../common/collection.h"
#include "../common/json/json.h"
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

struct mux_config config;
struct mux_device device;

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

size_t item_count = 0;
content_item *items = NULL;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;

int sd_card;
char *sd_dir = NULL;

static char SD1[MAX_BUFFER_SIZE];
static char SD2[MAX_BUFFER_SIZE];
static char E_USB[MAX_BUFFER_SIZE];

char *current_wall = "";

char *prev_dir;
char *curr_dir;

int sys_index = -1;

int ui_count = 0;
int ui_file_count = 0;
int current_item_index = 0;
int content_file_index = 0;
int first_open = 1;
int nav_moved = 1;
int counter_fade = 0;
int fade_timeout = 3;
int swap_timeout = 3;
int starter_image = 0;

static char current_meta_text[MAX_BUFFER_SIZE];
static char current_content_label[MAX_BUFFER_SIZE];
static char box_image_previous_path[MAX_BUFFER_SIZE];
static char preview_image_previous_path[MAX_BUFFER_SIZE];

lv_timer_t *datetime_timer;
lv_timer_t *capacity_timer;
lv_timer_t *osd_timer;
lv_timer_t *glyph_timer;
lv_timer_t *ui_refresh_timer;

const char *store_catalogue;
const char *store_favourite;

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
        snprintf(content_core, sizeof(content_core), "%s/MUOS/info/core/core.cfg",
                 store_favourite);
    } else {
        snprintf(content_core, sizeof(content_core), "%s/MUOS/info/core/%s/core.cfg",
                 store_favourite, get_last_subdir(sd_dir, '/', 4));
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

    const char *content_label = items[current_item_index].name;

    switch (module) {
        case ROOT:
            if (strcasecmp(content_label, "SD1 (mmc)") == 0) {
                snprintf(content_desc, sizeof(content_desc), "%s/MUOS/info/catalogue/Root/text/sd1.txt",
                         store_catalogue);
            } else if (strcasecmp(content_label, "SD2 (sdcard)") == 0) {
                snprintf(content_desc, sizeof(content_desc), "%s/MUOS/info/catalogue/Root/text/sd2.txt",
                         store_catalogue);
            }
            break;
        case FAVOURITE:
            char f_core_file[MAX_BUFFER_SIZE];
            char f_pointer[MAX_BUFFER_SIZE];

            snprintf(f_core_file, sizeof(f_core_file), "%s/MUOS/info/favourite/%s.cfg",
                     store_favourite, strip_ext(items[current_item_index].name));

            snprintf(f_pointer, sizeof(f_pointer), "%s/MUOS/info/core/%s",
                     store_favourite, get_last_subdir(read_text_from_file(f_core_file), '/', 6));

            snprintf(content_desc, sizeof(content_desc), "%s/MUOS/info/catalogue/%s/text/%s.txt",
                     store_catalogue, read_line_from_file(f_pointer, 3),
                     strip_ext(read_line_from_file(f_pointer, 7)));
            break;
        case HISTORY:
            char h_core_file[MAX_BUFFER_SIZE];
            char h_pointer[MAX_BUFFER_SIZE];

            snprintf(h_core_file, sizeof(h_core_file), "%s/MUOS/info/history/%s.cfg",
                     store_favourite, strip_ext(items[current_item_index].name));

            snprintf(h_pointer, sizeof(h_pointer), "%s/MUOS/info/core/%s",
                     store_favourite, get_last_subdir(read_text_from_file(h_core_file), '/', 6));

            snprintf(content_desc, sizeof(content_desc), "%s/MUOS/info/catalogue/%s/text/%s.txt",
                     store_catalogue, read_line_from_file(h_pointer, 3),
                     strip_ext(read_line_from_file(h_pointer, 7)));
            break;
        default:
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
                snprintf(content_desc, sizeof(content_desc), "%s/MUOS/info/catalogue/Root/text/%s.txt",
                         store_catalogue, content_label);
            } else {
                char *desc_name = strip_ext(items[current_item_index].name);

                char core_file[MAX_BUFFER_SIZE];
                snprintf(core_file, sizeof(core_file), "%s/MUOS/info/core/%s/core.cfg",
                         store_favourite, get_last_subdir(sd_dir, '/', 4));

                printf("TRYING TO READ CORE CONFIG META: %s\n", core_file);
                char *core_desc = read_line_from_file(core_file, 2);
                if (strlen(core_desc) <= 1 && items[current_item_index].content_type == ROM) {
                    printf("CORE IS NOT SET - TEXT NOT LOADED\n");
                    return "No Information Found";
                }
                printf("TEXT IS STORED AT: %s\n", core_desc);

                if (items[current_item_index].content_type == FOLDER) {
                    snprintf(content_desc, sizeof(content_desc), "%s/MUOS/info/catalogue/Folder/text/%s.txt",
                             store_catalogue, content_label);
                } else {
                    snprintf(content_desc, sizeof(content_desc), "%s/MUOS/info/catalogue/%s/text/%s.txt",
                             store_catalogue, core_desc, desc_name);
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

void reset_label_long_mode() {
    lv_label_set_long_mode(lv_group_get_focused(ui_group), LV_LABEL_LONG_DOT);
}

void set_label_long_mode() {
    lv_task_handler();

    char *content_label = lv_label_get_text(lv_group_get_focused(ui_group));

    size_t len = strlen(content_label);
    bool ends_with_ellipse = len > 3 && strcmp(&content_label[len - 3], "...") == 0;

    if (strcasecmp(items[current_item_index].display_name, content_label) != 0 && ends_with_ellipse) {
        lv_label_set_long_mode(lv_group_get_focused(ui_group), LV_LABEL_LONG_SCROLL_CIRCULAR);
    }
}

void update_file_counter() {
    if ((ui_count > 0 && ui_file_count == 0 && config.VISUAL.COUNTERFOLDER) ||
        (ui_file_count > 0 && config.VISUAL.COUNTERFILE)) {
        fade_timeout = 3;
        lv_obj_clear_flag(ui_lblCounter, LV_OBJ_FLAG_HIDDEN);
        counter_fade = (theme.COUNTER.BORDER_ALPHA + theme.COUNTER.BACKGROUND_ALPHA + theme.COUNTER.TEXT_ALPHA);
        if (counter_fade > 255) counter_fade = 255;
        lv_obj_set_style_opa(ui_lblCounter, counter_fade, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text_fmt(ui_lblCounter, "%d%s%d",
                              current_item_index + 1, theme.COUNTER.TEXT_SEPARATOR, ui_count);
    } else {
        lv_obj_add_flag(ui_lblCounter, LV_OBJ_FLAG_HIDDEN);
    }
}

void image_refresh(char *image_type) {
    if (strcasecmp(image_type, "box") == 0 && config.VISUAL.BOX_ART == 8) {
        printf("BOX ART IS SET TO DISABLED\n");
        return;
    }

    char image[MAX_BUFFER_SIZE];
    char image_path[MAX_BUFFER_SIZE];

    const char *content_label = items[current_item_index].name;

    switch (module) {
        case ROOT:
            if (strcasecmp(content_label, "SD1 (mmc)") == 0) {
                snprintf(image, sizeof(image), "%s/MUOS/info/catalogue/Root/%s/sd1.png",
                         store_catalogue, image_type);
                snprintf(image_path, sizeof(image_path), "M:%s/MUOS/info/catalogue/Root/%s/sd1.png",
                         store_catalogue, image_type);
            } else if (strcasecmp(content_label, "SD2 (sdcard)") == 0) {
                snprintf(image, sizeof(image), "%s/MUOS/info/catalogue/Root/%s/sd2.png",
                         store_catalogue, image_type);
                snprintf(image_path, sizeof(image_path), "M:%s/MUOS/info/catalogue/Root/%s/sd2.png",
                         store_catalogue, image_type);
            } else if (strcasecmp(content_label, "USB (external)") == 0) {
                snprintf(image, sizeof(image), "%s/MUOS/info/catalogue/Root/%s/usb.png",
                         store_catalogue, image_type);
                snprintf(image_path, sizeof(image_path), "M:%s/MUOS/info/catalogue/Root/%s/usb.png",
                         store_catalogue, image_type);
            }
            break;
        case FAVOURITE:
            char f_core_file[MAX_BUFFER_SIZE];
            char f_pointer[MAX_BUFFER_SIZE];

            snprintf(f_core_file, sizeof(f_core_file), "%s/MUOS/info/favourite/%s.cfg",
                     store_favourite, strip_ext(items[current_item_index].name));

            snprintf(f_pointer, sizeof(f_pointer), "%s/MUOS/info/core/%s",
                     store_favourite, get_last_subdir(read_text_from_file(f_core_file), '/', 6));

            char *f_core_artwork = read_line_from_file(f_pointer, 3);
            if (strlen(f_core_artwork) <= 1) {
                printf("CORE IS NOT SET - ARTWORK NOT LOADED\n");
                return;
            }

            char *f_file_name = strip_ext(read_line_from_file(f_pointer, 7));

            snprintf(image, sizeof(image), "%s/MUOS/info/catalogue/%s/%s/%s.png",
                     store_catalogue, f_core_artwork, image_type, f_file_name);
            snprintf(image_path, sizeof(image_path), "M:%s/MUOS/info/catalogue/%s/%s/%s.png",
                     store_catalogue, f_core_artwork, image_type, f_file_name);
            break;
        case HISTORY:
            char h_core_file[MAX_BUFFER_SIZE];
            char h_pointer[MAX_BUFFER_SIZE];

            snprintf(h_core_file, sizeof(h_core_file), "%s/MUOS/info/history/%s.cfg",
                     store_favourite, strip_ext(items[current_item_index].name));

            snprintf(h_pointer, sizeof(h_pointer), "%s/MUOS/info/core/%s",
                     store_favourite, get_last_subdir(read_text_from_file(h_core_file), '/', 6));

            char *h_core_artwork = read_line_from_file(h_pointer, 3);
            if (strlen(h_core_artwork) <= 1) {
                printf("CORE IS NOT SET - ARTWORK NOT LOADED\n");
                return;
            }

            char *h_file_name = strip_ext(read_line_from_file(h_pointer, 7));

            snprintf(image, sizeof(image), "%s/MUOS/info/catalogue/%s/%s/%s.png",
                     store_catalogue, h_core_artwork, image_type, h_file_name);
            snprintf(image_path, sizeof(image_path), "M:%s/MUOS/info/catalogue/%s/%s/%s.png",
                     store_catalogue, h_core_artwork, image_type, h_file_name);
            break;
        default:
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
                snprintf(image, sizeof(image), "%s/MUOS/info/catalogue/Folder/%s/%s.png",
                         store_catalogue, image_type, content_label);
                snprintf(image_path, sizeof(image_path), "M:%s/MUOS/info/catalogue/Folder/%s/%s.png",
                         store_catalogue, image_type, content_label);
            } else {
                char *file_name = strip_ext(items[current_item_index].name);

                char core_file[MAX_BUFFER_SIZE];
                snprintf(core_file, sizeof(core_file), "%s/MUOS/info/core/%s/core.cfg",
                         store_favourite, get_last_subdir(sd_dir, '/', 4));

                char *core_artwork = read_line_from_file(core_file, 2);
                if (strlen(core_artwork) <= 1 && items[current_item_index].content_type == ROM) {
                    printf("CORE IS NOT SET - ARTWORK NOT LOADED\n");
                    return;
                }

                if (items[current_item_index].content_type == FOLDER) {
                    snprintf(image, sizeof(image), "%s/MUOS/info/catalogue/Folder/%s/%s.png",
                             store_catalogue, image_type, content_label);
                    snprintf(image_path, sizeof(image_path), "M:%s/MUOS/info/catalogue/Folder/%s/%s.png",
                             store_catalogue, image_type, content_label);
                } else {
                    snprintf(image, sizeof(image), "%s/MUOS/info/catalogue/%s/%s/%s.png",
                             store_catalogue, core_artwork, image_type, file_name);
                    snprintf(image_path, sizeof(image_path), "M:%s/MUOS/info/catalogue/%s/%s/%s.png",
                             store_catalogue, core_artwork, image_type, file_name);
                }
            }
            break;
    }

    if (strcasecmp(image_type, "preview") == 0) {
        if (strcasecmp(preview_image_previous_path, image) != 0) {
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
        if (strcasecmp(box_image_previous_path, image) != 0) {
            printf("LOADING BOX ARTWORK AT: %s\n", image);

            if (file_exist(image)) {
                starter_image = 1;
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

    char skip_ini[MAX_BUFFER_SIZE];
    snprintf(skip_ini, sizeof(skip_ini), "%s/MUOS/info/skip.ini", device.STORAGE.ROM.MOUNT);
    load_skip_patterns(skip_ini);

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
                char *file_path = (char *) malloc(strlen(entry->d_name) + 2);
                snprintf(file_path, strlen(entry->d_name) + 2, "%s", entry->d_name);

                *file_names = (char **) realloc(*file_names, (*file_count + 1) * sizeof(char *));
                (*file_names)[*file_count] = file_path;
                (*file_count)++;
            }
        }
    }

    ui_file_count = *file_count;
    closedir(dir);
}

void gen_label(int item_type, char *item_glyph, char *item_text, int glyph_pad) {
    lv_obj_t * ui_pnlExplore = lv_obj_create(ui_pnlContent);
    apply_theme_list_panel(&theme, &device, ui_pnlExplore);

    lv_obj_t * ui_lblExploreItem = lv_label_create(ui_pnlExplore);
    const bool apply_visual_label = module != ROOT;
    apply_theme_list_item(&theme, ui_lblExploreItem, item_text, apply_visual_label, true, false);

    lv_obj_t * ui_lblExploreItemGlyph = lv_label_create(ui_pnlExplore);
    apply_theme_list_icon(&theme, &device, &ui_font_AwesomeSmall, ui_lblExploreItemGlyph, item_glyph, glyph_pad);

    lv_group_add_obj(ui_group, ui_lblExploreItem);
    lv_group_add_obj(ui_group_glyph, ui_lblExploreItemGlyph);

    int item_width = apply_size_to_content(&theme, &device, ui_pnlContent, ui_lblExploreItem, item_text);
    apply_align(&theme, &device, ui_lblExploreItemGlyph, ui_lblExploreItem, item_width);

    if (item_type == FOLDER && strcasecmp(item_text, prev_dir) == 0) {
        sys_index = ui_count;
    }
}

void gen_item(char **file_names, int file_count) {
    char init_cache_file[MAX_BUFFER_SIZE];
    char init_meta_dir[MAX_BUFFER_SIZE];

    switch (module) {
        case MMC:
            if (strcasecmp(sd_dir, strip_dir(SD1)) != 0) {
                snprintf(init_cache_file, sizeof(init_cache_file), "%s/MUOS/info/cache/mmc/%s.ini",
                         device.STORAGE.ROM.MOUNT, strchr(strdup(sd_dir), '/') + strlen(SD1));
                snprintf(init_meta_dir, sizeof(init_meta_dir), "%s/MUOS/info/core/%s/",
                         store_favourite, strchr(strdup(sd_dir), '/') + strlen(SD1));
            } else {
                snprintf(init_cache_file, sizeof(init_cache_file), "%s/MUOS/info/cache/root_mmc.ini",
                         device.STORAGE.ROM.MOUNT);
                snprintf(init_meta_dir, sizeof(init_meta_dir), "%s/MUOS/info/core/",
                         store_favourite);
            }
            break;
        case SDCARD:
            if (strcasecmp(sd_dir, strip_dir(SD2)) != 0) {
                snprintf(init_cache_file, sizeof(init_cache_file), "%s/MUOS/info/cache/sdcard/%s.ini",
                         device.STORAGE.ROM.MOUNT, strchr(strdup(sd_dir), '/') + strlen(SD2));
                snprintf(init_meta_dir, sizeof(init_meta_dir), "%s/MUOS/info/core/%s/",
                         store_favourite, strchr(strdup(sd_dir), '/') + strlen(SD2));
            } else {
                snprintf(init_cache_file, sizeof(init_cache_file), "%s/MUOS/info/cache/root_sdcard.ini",
                         device.STORAGE.ROM.MOUNT);
                snprintf(init_meta_dir, sizeof(init_meta_dir), "%s/MUOS/info/core/",
                         store_favourite);
            }
            break;
        case USB:
            if (strcasecmp(sd_dir, strip_dir(E_USB)) != 0) {
                snprintf(init_cache_file, sizeof(init_cache_file), "%s/MUOS/info/cache/usb/%s.ini",
                         device.STORAGE.ROM.MOUNT, strchr(strdup(sd_dir), '/') + strlen(E_USB));
                snprintf(init_meta_dir, sizeof(init_meta_dir), "%s/MUOS/info/core/%s/",
                         store_favourite, strchr(strdup(sd_dir), '/') + strlen(E_USB));
            } else {
                snprintf(init_cache_file, sizeof(init_cache_file), "%s/MUOS/info/cache/root_usb.ini",
                         device.STORAGE.ROM.MOUNT);
                snprintf(init_meta_dir, sizeof(init_meta_dir), "%s/MUOS/info/core/",
                         store_favourite);
            }
            break;
        default:
            break;
    }

    create_directories(strip_dir(init_cache_file));
    create_directories(init_meta_dir);

    char local_name_cache[MAX_BUFFER_SIZE];
    snprintf(local_name_cache, sizeof(local_name_cache), "%score.cfg", init_meta_dir);

    int require_local_name_cache = atoi(read_line_from_file(local_name_cache, 3));

    int is_cache = 0;
    if (file_exist(init_cache_file)) {
        is_cache = 1;
    }

    int fn_valid = 0;
    struct json fn_json;

    char name_file[MAX_BUFFER_SIZE];
    snprintf(name_file, sizeof(name_file), "%s/MUOS/info/name.json",
             device.STORAGE.ROM.MOUNT);

    if (json_valid(read_text_from_file(name_file))) {
        fn_valid = 1;
        fn_json = json_parse(read_text_from_file(name_file));
    }

    for (int i = 0; i < file_count; i++) {
        char curr_item[MAX_BUFFER_SIZE];
        char fn_name[MAX_BUFFER_SIZE];
        char cache_fn_name[MAX_BUFFER_SIZE];

        if (require_local_name_cache || module == FAVOURITE || module == HISTORY) {
            if (is_cache) {
                snprintf(fn_name, sizeof(fn_name), "%s", read_line_from_file(init_cache_file, i + 1));
            } else {
                if (fn_valid) {
                    struct json good_name_json = json_object_get(fn_json, strip_ext(file_names[i]));
                    if (json_exists(good_name_json)) {
                        json_string_copy(good_name_json, fn_name, sizeof(fn_name));
                        snprintf(cache_fn_name, sizeof(cache_fn_name), "%s\n", fn_name);
                        write_text_to_file(init_cache_file, cache_fn_name, "a");
                    } else {
                        snprintf(fn_name, sizeof(fn_name), "%s", strip_ext((char *) file_names[i]));
                        snprintf(cache_fn_name, sizeof(cache_fn_name), "%s\n", fn_name);
                        write_text_to_file(init_cache_file, cache_fn_name, "a");
                        printf("MISSING LABEL: %s", cache_fn_name);
                    }
                }
            }
        } else {
            snprintf(fn_name, sizeof(fn_name), "%s", strip_ext((char *) file_names[i]));
        }

        snprintf(curr_item, sizeof(curr_item), "%s :: %d", fn_name, ui_count);

        ui_count++;
        content_item *new_item = add_item(&items, &item_count, file_names[i], fn_name, ROM);
        adjust_visual_label(new_item->display_name, config.VISUAL.NAME, config.VISUAL.DASH);
    }

    switch (module) {
        case HISTORY:
            sort_items_time(items, item_count);
            break;
        default:
            sort_items(items, item_count);
            break;
    }

    puts("START GEN");
    for (size_t i = 0; i < item_count; i++) {
        if (items[i].content_type == ROM) {
            char fav_dir[PATH_MAX];
            snprintf(fav_dir, sizeof(fav_dir), "%s/MUOS/info/favourite/%s.cfg",
                     store_favourite, strip_ext(items[i].name));

            char hist_dir[PATH_MAX];
            snprintf(hist_dir, sizeof(hist_dir), "%s/MUOS/info/history/%s.cfg",
                     store_favourite, strip_ext(items[i].name));

            char *glyph_icon;
            int glyph_pad;
            if (file_exist(fav_dir)) {
                glyph_icon = "\uF005";
                glyph_pad = 10;
            } else if (file_exist(hist_dir)) {
                glyph_icon = "\uE5A0";
                glyph_pad = 12;
            } else {
                glyph_icon = "\uF15B";
                glyph_pad = 12;
            }

            gen_label(ROM, glyph_icon, items[i].display_name, glyph_pad);
        }
    }
    puts("FINISH GEN");
}

void create_root_items(char *dir_name) {
    char spec_dir[PATH_MAX];

    char **dir_names = NULL;
    int dir_count = 0;

    char **file_names = NULL;
    int file_count = 0;

    switch (module) {
        case FAVOURITE:
            snprintf(spec_dir, sizeof(spec_dir), "%s/MUOS/info/%s",
                     store_favourite, dir_name);

            lv_label_set_text(ui_lblTitle, "FAVOURITES");
            break;
        case HISTORY:
            snprintf(spec_dir, sizeof(spec_dir), "%s/MUOS/info/%s",
                     store_favourite, dir_name);

            lv_label_set_text(ui_lblTitle, "HISTORY");
            break;
        default:
            snprintf(spec_dir, sizeof(spec_dir), "%s/MUOS/info/%s",
                     device.STORAGE.ROM.MOUNT, dir_name);

            lv_label_set_text(ui_lblTitle, "EXPLORE");
            break;
    }

    add_directory_and_file_names(spec_dir, &dir_names, &dir_count, &file_names, &file_count);

    if (dir_count > 0 || file_count > 0) {
        gen_item(file_names, file_count);
    }
}

void create_explore_items(void *count) {
    int *ui_count_ptr = (int *) count;

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

    int fn_valid = 0;
    struct json fn_json;
    
    char folder_name_file[MAX_BUFFER_SIZE];
    snprintf(folder_name_file, sizeof(folder_name_file), "%s/MUOS/info/folder_name.json",
             device.STORAGE.ROM.MOUNT);

    if (json_valid(read_text_from_file(folder_name_file))) {
        fn_valid = 1;
        fn_json = json_parse(read_text_from_file(folder_name_file));
    }

    if (dir_count > 0 || file_count > 0) {
        for (int i = 0; i < dir_count; i++) {
            content_item *new_item = NULL;
            char good_name[MAX_BUFFER_SIZE];
            if (fn_valid) {
                struct json good_name_json = json_object_get(fn_json, dir_names[i]);
                if (json_exists(good_name_json)) {
                    json_string_copy(good_name_json, good_name, sizeof(good_name));
                    new_item = add_item(&items, &item_count, dir_names[i], good_name, FOLDER);
                } 
            }
            if (new_item == NULL) new_item = add_item(&items, &item_count, dir_names[i], dir_names[i], FOLDER);
            adjust_visual_label(new_item->display_name, config.VISUAL.NAME, config.VISUAL.DASH);

            free(dir_names[i]);
        }
        sort_items(items, item_count);
        for (int i = 0; i < dir_count; i++) {
            gen_label(FOLDER, "\uF07B", items[i].display_name, 12);
            (*ui_count_ptr)++;
        }

        free(dir_names);

        gen_item(file_names, file_count);
        image_refresh("box");
        nav_moved = 1;
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
            add_item(&items, &item_count, "SD1 (mmc)", "SD1 (mmc)", FOLDER);
            add_item(&items, &item_count, "SD2 (sdcard)", "SD2 (sdcard)", FOLDER);
            gen_label(FOLDER, "\uF07B", "SD1 (mmc)", 12);
            gen_label(FOLDER, "\uF07B", "SD2 (sdcard)", 12);
            ui_count += 2;
            nav_moved = 1;
            break;
        case 8:
            write_text_to_file("/tmp/explore_card", "usb", "w");
            write_text_to_file("/tmp/explore_dir", strip_dir(E_USB), "w");
            write_text_to_file("/tmp/single_card", "", "w");
            load_mux("explore");
            safe_quit = 1;
            break;
        case 10:
            add_item(&items, &item_count, "SD1 (mmc)", "SD1 (mmc)", FOLDER);
            add_item(&items, &item_count, "USB (external)", "USB (external)", FOLDER);
            gen_label(FOLDER, "\uF07B", "SD1 (mmc)", 12);
            gen_label(FOLDER, "\uF07B", "USB (external)", 12);
            ui_count += 2;
            nav_moved = 1;
            break;
        case 12:
            add_item(&items, &item_count, "SD2 (sdcard)", "SD2 (sdcard)", FOLDER);
            add_item(&items, &item_count, "USB (external)", "USB (external)", FOLDER);
            gen_label(FOLDER, "\uF07B", "SD2 (sdcard)", 12);
            gen_label(FOLDER, "\uF07B", "USB (external)", 12);
            ui_count += 2;
            nav_moved = 1;
            break;
        case 14:
            add_item(&items, &item_count, "SD1 (mmc)", "SD1 (mmc)", FOLDER);
            add_item(&items, &item_count, "SD2 (sdcard)", "SD2 (sdcard)", FOLDER);
            add_item(&items, &item_count, "USB (external)", "USB (external)", FOLDER);
            gen_label(FOLDER, "\uF07B", "SD1 (mmc)", 12);
            gen_label(FOLDER, "\uF07B", "SD2 (sdcard)", 12);
            gen_label(FOLDER, "\uF07B", "USB (external)", 12);
            ui_count += 3;
            nav_moved = 1;
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

int load_content(int add_favourite) {
    char *assigned_core = load_content_core(0);
    printf("ASSIGNED CORE: %s\n", assigned_core);

    if (assigned_core == NULL) {
        return 0;
    }

    char content_loader_file[MAX_BUFFER_SIZE];
    snprintf(content_loader_file, sizeof(content_loader_file), "%s/MUOS/info/core/%s/%s.cfg",
             store_favourite, get_last_subdir(sd_dir, '/', 4), strip_ext(items[current_item_index].name));

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
        snprintf(content_loader_data, sizeof(content_loader_data), "%s\n%s\n/mnt/%s/ROMS/\n%s\n%s\n",
                 strip_ext(items[current_item_index].name), assigned_core, curr_sd,
                 get_last_subdir(sd_dir, '/', 4),
                 items[current_item_index].name);

        write_text_to_file(content_loader_file, content_loader_data, "w");
        printf("\nCONFIG DATA\n%s\n", content_loader_data);
    }

    if (file_exist(content_loader_file)) {
        char add_to_hf[MAX_BUFFER_SIZE];
        char *hf_type;

        char fav_dir[MAX_BUFFER_SIZE];
        char his_dir[MAX_BUFFER_SIZE];
        snprintf(fav_dir, sizeof(fav_dir), "%s/MUOS/info/favourite", store_favourite);
        snprintf(his_dir, sizeof(his_dir), "%s/MUOS/info/history", store_favourite);

        if (add_favourite) {
            hf_type = fav_dir;
        } else {
            hf_type = his_dir;
        }

        snprintf(add_to_hf, sizeof(add_to_hf), "%s/%s.cfg", hf_type, strip_ext(items[current_item_index].name));

        char pointer[MAX_BUFFER_SIZE];
        snprintf(pointer, sizeof(pointer), "%s/MUOS/info/core/%s/%s.cfg",
                 store_favourite, get_last_subdir(sd_dir, '/', 4), strip_ext(items[current_item_index].name));

        if (add_favourite) {
            write_text_to_file(add_to_hf, pointer, "w");

            char *hf_msg;
            if (file_exist(add_to_hf)) {
                hf_msg = "Added to favourites!";
            } else {
                hf_msg = "Could not add to favourites!";
            }

            lv_label_set_text(ui_lblMessage, hf_msg);
            lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);

            return 1;
        } else {
            printf("TRYING TO LOAD CONTENT...\n");
/*
            char act_file[MAX_BUFFER_SIZE];
            char act_content[MAX_BUFFER_SIZE];
            snprintf(act_file, sizeof(act_file), "%s/MUOS/info/activity/%s.act",
                     device.STORAGE.ROM.MOUNT, strip_ext(items[current_item_index].name));
            snprintf(act_content, sizeof(act_content), "%s\n%s\n%s",
                     strip_ext(items[current_item_index].name), curr_sd, read_line_from_file(content_loader_file, 5));
            prepare_activity_file(act_content, act_file);
*/
            write_text_to_file(add_to_hf, pointer, "w");
            write_text_to_file(MUOS_ROM_LOAD, read_text_from_file(content_loader_file), "w");
        }

        printf("CONTENT LOADED SUCCESSFULLY\n");
        return 1;
    }

    lv_label_set_text(ui_lblMessage, "Could not load content - No core is associated");
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);

    return 0;
}

int load_cached_content(const char *content_name, char *cache_type, int add_favourite) {
    char pointer_file[MAX_BUFFER_SIZE];
    snprintf(pointer_file, sizeof(pointer_file), "%s/MUOS/info/%s/%s",
             store_favourite, cache_type, content_name);

    char cache_file[MAX_BUFFER_SIZE];
    snprintf(cache_file, sizeof(cache_file), "%s",
             read_line_from_file(pointer_file, 1));

    if (file_exist(cache_file)) {
        char add_to_hf[MAX_BUFFER_SIZE];

        if (add_favourite) {
            snprintf(add_to_hf, sizeof(add_to_hf), "%s/MUOS/info/favourite/%s",
                     store_favourite, content_name);

            write_text_to_file(add_to_hf, read_text_from_file(pointer_file), "w");

            return 1;
        } else {
            char *assigned_core = read_line_from_file(cache_file, 2);
            printf("ASSIGNED CORE: %s\n", assigned_core);
            printf("CONFIG FILE: %s\n", cache_file);

            snprintf(add_to_hf, sizeof(add_to_hf), "%s/MUOS/info/history/%s",
                     store_favourite, content_name);

            printf("TRYING TO LOAD CONTENT...\n");
/*
        char act_file[MAX_BUFFER_SIZE];
        char act_content[MAX_BUFFER_SIZE];
        snprintf(act_file, sizeof(act_file), "%s/MUOS/info/activity/%s.act",
                 device.STORAGE.ROM.MOUNT, content_name);
        snprintf(act_content, sizeof(act_content), "%s\n%s\n%s",
                 content_name, curr_sd, read_line_from_file(cache_file, 5));
        prepare_activity_file(act_content, act_file);
*/
            write_text_to_file(add_to_hf, read_text_from_file(pointer_file), "w");
            write_text_to_file(MUOS_ROM_LOAD, read_text_from_file(cache_file), "w");

            printf("CONTENT LOADED SUCCESSFULLY\n");
            return 1;
        }
    }

    lv_label_set_text(ui_lblMessage, "Could not load content!");
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);

    return 0;
}

void list_nav_prev(int steps) {
    for (int step = 0; step < steps; ++step) {
        reset_label_long_mode();
        if (current_item_index > 0) {
            current_item_index--;
            nav_prev(ui_group, 1);
            nav_prev(ui_group_glyph, 1);
        }
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count,
                           current_item_index, ui_pnlContent, NULL, NULL);
    play_sound("navigate", nav_sound, 0);
    image_refresh("box");
    set_label_long_mode();
    nav_moved = 1;
}

void list_nav_next(int steps) {
    for (int step = 0; step < steps; ++step) {
        reset_label_long_mode();
        if (current_item_index < (ui_count - 1)) {
            current_item_index++;
            nav_next(ui_group, 1);
            nav_next(ui_group_glyph, 1);
        }
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count,
                           current_item_index, ui_pnlContent, NULL, NULL);
    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound, 0);
    }
    image_refresh("box");
    set_label_long_mode();
    nav_moved = 1;
}

void cache_message(char *n_dir) {
    char cache_file[MAX_BUFFER_SIZE];
    switch (module) {
        case MMC:
            snprintf(cache_file, sizeof(cache_file), "%s/MUOS/info/cache/mmc/%s.ini", device.STORAGE.ROM.MOUNT,
                     get_last_subdir(n_dir, '/', 4));
            break;
        case SDCARD:
            snprintf(cache_file, sizeof(cache_file), "%s/MUOS/info/cache/sdcard/%s.ini", device.STORAGE.ROM.MOUNT,
                     get_last_subdir(n_dir, '/', 4));
            break;
        case USB:
            snprintf(cache_file, sizeof(cache_file), "%s/MUOS/info/cache/usb/%s.ini", device.STORAGE.ROM.MOUNT,
                     get_last_subdir(n_dir, '/', 4));
            break;
        case FAVOURITE:
            snprintf(cache_file, sizeof(cache_file), "%s/MUOS/info/cache/favourite.ini", device.STORAGE.ROM.MOUNT);
            break;
        case HISTORY:
            snprintf(cache_file, sizeof(cache_file), "%s/MUOS/info/cache/history.ini", device.STORAGE.ROM.MOUNT);
            break;
        default:
            break;
    }

    lv_label_set_text(ui_lblMessage, "Loading...");
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
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

                switch (ev.type) {
                    case EV_KEY:
                        if (ev.value == 1) {
                            if (msgbox_active) {
                                if (ev.code == NAV_B || ev.code == device.RAW_INPUT.BUTTON.MENU_SHORT) {
                                    play_sound("confirm", nav_sound, 1);
                                    msgbox_active = 0;
                                    progress_onscreen = 0;
                                    lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
                                } else if (ev.code == NAV_A) {
                                    play_sound("confirm", nav_sound, 1);
                                    if (lv_obj_has_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN)) {
                                        lv_obj_add_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
                                    } else {
                                        lv_obj_add_flag(ui_pnlHelpPreview, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_pnlHelpMessage, LV_OBJ_FLAG_HIDDEN);
                                    }
                                }
                            } else {
                                if (ev.code == device.RAW_INPUT.BUTTON.MENU_LONG) {
                                    JOYHOTKEY_pressed = 1;
                                    JOYUP_pressed = 0;
                                    JOYDOWN_pressed = 0;
                                    nav_delay = UINT8_MAX;
                                    nav_hold = 0;
                                } else if (ev.code == NAV_A) {
                                    if (ui_count == 0) {
                                        goto nothing_ever_happens;
                                    }

                                    play_sound("confirm", nav_sound, 1);

                                    char *content_label = items[current_item_index].name;

                                    switch (module) {
                                        case ROOT:
                                            if (strcasecmp(content_label, "SD1 (mmc)") == 0) {
                                                write_text_to_file("/tmp/explore_card", "mmc", "w");
                                                write_text_to_file("/tmp/explore_dir", strip_dir(SD1), "w");
                                                load_mux("explore");
                                                break;
                                            } else if (strcasecmp(content_label, "SD2 (sdcard)") == 0) {
                                                write_text_to_file("/tmp/explore_card", "sdcard", "w");
                                                write_text_to_file("/tmp/explore_dir", strip_dir(SD2), "w");
                                                load_mux("explore");
                                                break;
                                            } else if (strcasecmp(content_label, "USB (external)") == 0) {
                                                write_text_to_file("/tmp/explore_card", "usb", "w");
                                                write_text_to_file("/tmp/explore_dir", strip_dir(E_USB), "w");
                                                load_mux("explore");
                                                break;
                                            }
                                            break;
                                        default:
                                            char f_content[MAX_BUFFER_SIZE];
                                            snprintf(f_content, sizeof(f_content), "%s.cfg", strip_ext(items[current_item_index].name));

                                            switch (module) {
                                                case MMC:
                                                case SDCARD:
                                                case USB:
                                                    if (items[current_item_index].content_type == FOLDER) {
                                                        char n_dir[MAX_BUFFER_SIZE];
                                                        snprintf(n_dir, sizeof(n_dir), "%s/%s",
                                                                 sd_dir,
                                                                 items[current_item_index].name);

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
                                                        write_text_to_file(MUOS_IDX_LOAD, c_index, "w");

                                                        if (load_content(0)) {
                                                            static char launch_script[MAX_BUFFER_SIZE];
                                                            snprintf(launch_script, sizeof(launch_script),
                                                                     "%s/script/mux/launch.sh", INTERNAL_PATH);

                                                            write_text_to_file("/tmp/manual_launch", "1", "w");

                                                            system(launch_script);
                                                        }
                                                        break;
                                                    }
                                                    break;
                                                case FAVOURITE:
                                                    if (load_cached_content(f_content, "favourite", 0)) {
                                                        write_text_to_file("/tmp/explore_card", "favourite", "w");
                                                        write_text_to_file("/tmp/explore_dir", "", "w");
                                                        write_text_to_file("/tmp/manual_launch", "1", "w");
                                                    } else {
                                                        goto fail_quit;
                                                    }
                                                    break;
                                                case HISTORY:
                                                    if (load_cached_content(f_content, "history", 0)) {
                                                        write_text_to_file("/tmp/explore_card", "history", "w");
                                                        write_text_to_file("/tmp/explore_dir", "", "w");
                                                        write_text_to_file("/tmp/manual_launch", "1", "w");
                                                    } else {
                                                        goto fail_quit;
                                                    }
                                                    break;
                                                default:
                                                    break;
                                            }
                                            break;
                                    }
                                    safe_quit = 1;
                                    fail_quit:
                                } else if (ev.code == NAV_B) {
                                    play_sound("back", nav_sound, 1);

                                    switch (module) {
                                        case FAVOURITE:
                                            write_text_to_file("/tmp/explore_card", "root", "w");
                                            write_text_to_file("/tmp/explore_dir", "", "w");
                                            load_mux("launcher");
                                            break;
                                        case HISTORY:
                                            write_text_to_file("/tmp/explore_card", "root", "w");
                                            write_text_to_file("/tmp/explore_dir", "", "w");
                                            load_mux("launcher");
                                            break;
                                        default:
                                            if (sd_dir != NULL) {
                                                char *b_dir = strrchr(sd_dir, '/');
                                                if (b_dir != NULL) {
                                                    if (strcasecmp(str_tolower(b_dir), "/roms") == 0) {
                                                        if (file_exist("/tmp/single_card")) {
                                                            write_text_to_file("/tmp/explore_card", "root", "w");
                                                            write_text_to_file("/tmp/explore_dir", "", "w");
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
                                } else if (ev.code == device.RAW_INPUT.BUTTON.X) {
                                    if (ui_count == 0) {
                                        goto nothing_ever_happens;
                                    }

                                    char n_dir[MAX_BUFFER_SIZE];
                                    snprintf(n_dir, sizeof(n_dir), "%s", sd_dir);

                                    char f_content[MAX_BUFFER_SIZE];
                                    snprintf(f_content, sizeof(f_content), "%s.cfg", strip_ext(items[current_item_index].name));

                                    char cache_file[MAX_BUFFER_SIZE];
                                    switch (module) {
                                        case MMC:
                                            play_sound("confirm", nav_sound, 1);

                                            snprintf(cache_file, sizeof(cache_file), "%s/MUOS/info/cache/mmc/%s.ini",
                                                     device.STORAGE.ROM.MOUNT, get_last_subdir(n_dir, '/', 4));

                                            write_text_to_file("/tmp/explore_card", "mmc", "w");
                                            break;
                                        case SDCARD:
                                            play_sound("confirm", nav_sound, 1);

                                            snprintf(cache_file, sizeof(cache_file),
                                                     "%s/MUOS/info/cache/sdcard/%s.ini",
                                                     device.STORAGE.ROM.MOUNT, get_last_subdir(n_dir, '/', 4));

                                            write_text_to_file("/tmp/explore_card", "sdcard", "w");
                                            break;
                                        case USB:
                                            play_sound("confirm", nav_sound, 1);

                                            snprintf(cache_file, sizeof(cache_file), "%s/MUOS/info/cache/usb/%s.ini",
                                                     device.STORAGE.ROM.MOUNT, get_last_subdir(n_dir, '/', 4));

                                            write_text_to_file("/tmp/explore_card", "usb", "w");
                                            break;
                                        case FAVOURITE:
                                            play_sound("confirm", nav_sound, 1);

                                            snprintf(cache_file, sizeof(cache_file), "%s/MUOS/info/favourite/%s.cfg",
                                                     store_favourite, strip_ext(f_content));

                                            remove(cache_file);
                                            write_text_to_file("/tmp/mux_reload", "1", "w");

                                            goto ttq;
                                            break;
                                        case HISTORY:
                                            play_sound("confirm", nav_sound, 1);

                                            snprintf(cache_file, sizeof(cache_file), "%s/MUOS/info/history/%s.cfg",
                                                     store_favourite, strip_ext(f_content));

                                            remove(cache_file);
                                            write_text_to_file("/tmp/mux_reload", "1", "w");

                                            goto ttq;
                                            break;
                                        default:
                                            goto nothing_ever_happens;
                                    }

                                    char c_dir[MAX_BUFFER_SIZE];
                                    snprintf(c_dir, sizeof(c_dir), "%s/MUOS/info/core/%s",
                                             store_favourite, get_last_subdir(n_dir, '/', 4));
                                    const char *exception_list[] = {"core.cfg", NULL};

                                    if (file_exist(cache_file)) {
                                        remove(cache_file);
                                        delete_files_of_type(c_dir, "cfg", exception_list);
                                        cache_message(n_dir);
                                    }

                                    write_text_to_file("/tmp/explore_dir", n_dir, "w");
                                    load_mux("explore");

                                    ttq:
                                    safe_quit = 1;
                                } else if (ev.code == device.RAW_INPUT.BUTTON.Y) {
                                    play_sound("confirm", nav_sound, 1);

                                    char f_content[MAX_BUFFER_SIZE];
                                    snprintf(f_content, sizeof(f_content), "%s.cfg", strip_ext(items[current_item_index].name));

                                    switch (module) {
                                        case MMC:
                                        case SDCARD:
                                        case USB:
                                            if (items[current_item_index].content_type == FOLDER) {
                                                lv_label_set_text(ui_lblMessage,
                                                                  "Directories cannot be added to Favourites");
                                                lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
                                                break;
                                            } else {
                                                if (load_content(1)) {
                                                    lv_label_set_text(ui_lblMessage, "Added to Favourites");
                                                    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
                                                } else {
                                                    lv_label_set_text(ui_lblMessage, "Error adding to Favourites");
                                                    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
                                                }
                                                if (file_exist(MUOS_ROM_LOAD)) {
                                                    remove(MUOS_ROM_LOAD);
                                                }
                                                break;
                                            }
                                            break;
                                        case HISTORY:
                                            if (load_cached_content(f_content, "history", 1)) {
                                                lv_label_set_text(ui_lblMessage, "Added to Favourites");
                                                lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
                                            } else {
                                                lv_label_set_text(ui_lblMessage, "Error adding to Favourites");
                                                lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
                                            }
                                            if (file_exist(MUOS_ROM_LOAD)) {
                                                remove(MUOS_ROM_LOAD);
                                            }
                                            break;
                                        default:
                                            break;
                                    }
                                } else if (ev.code == device.RAW_INPUT.BUTTON.START) {
                                    switch (module) {
                                        case MMC:
                                        case SDCARD:
                                        case USB:
                                            play_sound("confirm", nav_sound, 1);

                                            write_text_to_file("/tmp/explore_card", "root", "w");
                                            remove("/tmp/explore_dir");
                                            load_mux("explore");

                                            safe_quit = 1;
                                            break;
                                        default:
                                            break;
                                    }
                                } else if (ev.code == device.RAW_INPUT.BUTTON.SELECT) {
                                    if (module != ROOT && module != FAVOURITE && module != HISTORY
                                        && !strcasecmp(get_last_dir(sd_dir), "ROMS") == 0) {
                                        play_sound("confirm", nav_sound, 1);

                                        switch (module) {
                                            case MMC:
                                            case SDCARD:
                                            case USB:
                                                write_text_to_file(MUOS_SAA_LOAD, "1", "w");
                                                load_content_core(1);
                                                break;
                                            default:
                                                break;
                                        }
                                    }
                                } else if (ev.code == device.RAW_INPUT.BUTTON.L1) {
                                    if (current_item_index != 0 && current_item_index < ui_count) {
                                        list_nav_prev(theme.MUX.ITEM.COUNT);
                                        lv_task_handler();
                                    }
                                } else if (ev.code == device.RAW_INPUT.BUTTON.R1) {
                                    if (current_item_index >= 0 && current_item_index != ui_count - 1) {
                                        list_nav_next(theme.MUX.ITEM.COUNT);
                                        lv_task_handler();
                                    }
                                }
                            }
                        } else {
                            if (ev.code == device.RAW_INPUT.BUTTON.MENU_SHORT) {
                                JOYHOTKEY_pressed = 0;
                                if (progress_onscreen == -1) {
                                    if (ui_count == 0) {
                                        goto nothing_ever_happens;
                                    }

                                    play_sound("confirm", nav_sound, 1);
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
                                                  items[current_item_index].display_name,
                                                  load_content_description());
                                }
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
                                    reset_label_long_mode();
                                    current_item_index = ui_count - 1;
                                    nav_prev(ui_group, 1);
                                    nav_prev(ui_group_glyph, 1);
                                    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count,
                                                           current_item_index, ui_pnlContent, NULL, NULL);
                                    nav_moved = 1;
                                    image_refresh("box");
                                    set_label_long_mode();
                                    update_file_counter();
                                    lv_task_handler();
                                } else if (current_item_index > 0) {
                                    JOYUP_pressed = (ev.value != 0);
                                    list_nav_prev(1);
                                    nav_moved = 1;
                                    lv_task_handler();
                                }
                            } else if ((ev.value >= (device.INPUT.AXIS_MIN >> 2) &&
                                        ev.value <= (device.INPUT.AXIS_MAX >> 2)) ||
                                       ev.value == 1) {
                                if (current_item_index == ui_count - 1) {
                                    reset_label_long_mode();
                                    current_item_index = 0;
                                    nav_next(ui_group, 1);
                                    nav_next(ui_group_glyph, 1);
                                    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count,
                                                           current_item_index, ui_pnlContent, NULL, NULL);
                                    nav_moved = 1;
                                    image_refresh("box");
                                    set_label_long_mode();
                                    update_file_counter();
                                    lv_task_handler();
                                } else if (current_item_index < ui_count - 1) {
                                    JOYDOWN_pressed = (ev.value != 0);
                                    list_nav_next(1);
                                    nav_moved = 1;
                                    lv_task_handler();
                                }
                            } else {
                                JOYUP_pressed = 0;
                                JOYDOWN_pressed = 0;
                            }
                        }
                    default:
                    nothing_ever_happens:
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
                if (JOYDOWN_pressed && current_item_index < ui_count - 1) {
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

void set_nav_text(const char *nav_a, const char *nav_b, const char *nav_x, const char *nav_y, const char *nav_menu) {
    lv_label_set_text(ui_lblNavA, nav_a);
    lv_label_set_text(ui_lblNavB, nav_b);
    if (nav_x) lv_label_set_text(ui_lblNavX, nav_x);
    if (nav_y) lv_label_set_text(ui_lblNavY, nav_y);
    lv_label_set_text(ui_lblNavMenu, nav_menu);
}

void set_nav_flag(lv_obj_t *nav_keep[], size_t keep_size, lv_obj_t *nav_hide[], size_t hide_size) {
    for (size_t i = 0; i < hide_size; i++) {
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }
    for (size_t i = 0; i < keep_size; i++) {
        lv_obj_clear_flag(nav_keep[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(nav_keep[i], LV_OBJ_FLAG_FLOATING);
    }
}

void init_elements() {
    switch (config.VISUAL.BOX_ART) {
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
            lv_obj_set_height(ui_pnlBox, device.MUX.HEIGHT);
            lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
            lv_obj_move_background(ui_pnlBox);
            lv_obj_move_background(ui_pnlWall);
            break;
        case 7: // Fullscreen + Front
            lv_obj_set_height(ui_pnlBox, device.MUX.HEIGHT);
            lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
            lv_obj_move_foreground(ui_pnlBox);
            break;
        case 8: // Disabled
            lv_obj_add_flag(ui_pnlBox, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_pnlBox, LV_OBJ_FLAG_FLOATING);
            break;
    }

    lv_obj_move_foreground(ui_pnlFooter);
    lv_obj_move_foreground(ui_pnlHeader);
    lv_obj_move_foreground(ui_lblCounter);
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

    char *overlay = load_overlay_image();
    if (strlen(overlay) > 0 && theme.MISC.IMAGE_OVERLAY) {
        lv_obj_t * overlay_img = lv_img_create(ui_scrExplore);
        lv_img_set_src(overlay_img, overlay);
        lv_obj_move_foreground(overlay_img);
    }

    if (TEST_IMAGE) display_testing_message(ui_scrExplore);
}

void init_footer_elements() {
    switch (module) {
        case ROOT: {
            set_nav_text("Open", "Back", NULL, NULL, "Info");
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
            set_nav_flag(nav_keep, sizeof(nav_keep) / sizeof(nav_keep[0]), nav_hide,
                         sizeof(nav_hide) / sizeof(nav_hide[0]));
            break;
        }
        case MMC:
        case SDCARD:
        case USB:
        case FAVOURITE:
        case HISTORY: {
            set_nav_text("Open", "Back", module == ROOT ? NULL : "Refresh",
                         module == ROOT ? NULL : "Favourite", "Info");
            if (lv_group_get_obj_count(ui_group) <= 0) {
                lv_obj_t *nav_keep[] = {ui_lblNavBGlyph, ui_lblNavB};
                lv_obj_t *nav_hide[] = {
                        ui_lblNavAGlyph, ui_lblNavA,
                        ui_lblNavCGlyph, ui_lblNavC,
                        ui_lblNavXGlyph, ui_lblNavX,
                        ui_lblNavYGlyph, ui_lblNavY,
                        ui_lblNavZGlyph, ui_lblNavZ,
                        ui_lblNavMenuGlyph, ui_lblNavMenu
                };
                set_nav_flag(nav_keep, sizeof(nav_keep) / sizeof(nav_keep[0]), nav_hide,
                             sizeof(nav_hide) / sizeof(nav_hide[0]));
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
                set_nav_flag(nav_keep, sizeof(nav_keep) / sizeof(nav_keep[0]), nav_hide,
                             sizeof(nav_hide) / sizeof(nav_hide[0]));
            }
            break;
        }
        default:
            break;
    }

    // We'll fucking run it again to ensure 'Favourite' isn't visible in favourites (redundant!)
    switch (module) {
        case FAVOURITE:
            lv_obj_add_flag(ui_lblNavY, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavY, LV_OBJ_FLAG_FLOATING);
            lv_obj_add_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_FLOATING);
            lv_label_set_text(ui_lblNavX, "Remove");
            break;
        case HISTORY:
            lv_label_set_text(ui_lblNavX, "Remove");
            break;
        default:
            break;
    }
}

void init_fonts() {
    load_font_text(mux_prog, ui_scrExplore);
    load_font_section(mux_prog, FONT_PANEL_FOLDER, ui_pnlContent);
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

    if (!nav_moved & !fade_timeout) {
        if (counter_fade > 0) {
            lv_obj_set_style_opa(ui_lblCounter, counter_fade - theme.COUNTER.TEXT_FADE_TIME,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
            counter_fade -= theme.COUNTER.TEXT_FADE_TIME;
        }
        if (counter_fade < 0) {
            lv_obj_add_flag(ui_lblCounter, LV_OBJ_FLAG_HIDDEN);
            counter_fade = 0;
        }
    } else {
        fade_timeout--;
    }

    if (!swap_timeout) {
        if (module != ROOT && module != FAVOURITE && module != HISTORY) {
            char *last_dir = get_last_dir(sd_dir);
            if (strcasecmp(lv_label_get_text(ui_lblTitle), last_dir) == 0) {
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
            } else {
                if (!strcasecmp(last_dir, "ROMS") == 0) {
                    lv_label_set_text(ui_lblTitle, last_dir);
                }
            }
        }
        swap_timeout = 3;
    } else {
        swap_timeout--;
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
                    ui_scrExplore, ui_group, theme.MISC.ANIMATED_BACKGROUND));

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
                if (!starter_image) {
                    lv_img_set_src(ui_imgBox, &ui_img_nothing_png);
                    starter_image = 0; // Reset back for static image loading
                }
            }
        }
        image_refresh("box");
        lv_obj_invalidate(ui_pnlBox);

        const char *content_label = lv_obj_get_user_data(lv_group_get_focused(ui_group));
        snprintf(current_content_label, sizeof(current_content_label), "%s", content_label);

        if (!lv_obj_has_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
        }

        update_file_counter();

        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    load_device(&device);
    srand(time(NULL));

    char *cmd_help = "\nmuOS Extras - System List\nUsage: %s <-im>\n\nOptions:\n"
                     "\t-i Index of content to skip to\n"
                     "\t-m List Module:\n"
                     "\t\troot - List SD Cards\n"
                     "\t\tmmc - List directories and files from SD1\n"
                     "\t\tsdcard - List directories and files from SD2\n"
                     "\t\tusb - List directories and files from USB\n"
                     "\t\tfavourite - List favourite items\n"
                     "\t\thistory - List history items\n\n";

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
                fprintf(stderr, cmd_help, argv[0]);
                return 1;
        }
    }

    if (sys_index == -1) {
        fprintf(stderr, cmd_help, argv[0]);
        return 1;
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

    if (file_exist("/tmp/manual_launch")) {
        remove("/tmp/manual_launch");
    }

    store_catalogue = get_default_storage(config.STORAGE.CATALOGUE);
    store_favourite = get_default_storage(config.STORAGE.FAV);

    snprintf(SD1, sizeof(SD1), "%s/ROMS/", device.STORAGE.ROM.MOUNT);
    snprintf(SD2, sizeof(SD2), "%s/ROMS/", device.STORAGE.SDCARD.MOUNT);
    snprintf(E_USB, sizeof(E_USB), "%s/ROMS/", device.STORAGE.USB.MOUNT);

    lv_obj_set_user_data(ui_scrExplore, mux_prog);

    lv_label_set_text(ui_lblDatetime, get_datetime());
    lv_label_set_text(ui_staCapacity, get_capacity());

    load_theme(&theme, &config, &device, mux_prog);
    load_glyph(&glyph, &device, mux_prog);

    apply_theme();
    init_fonts();

    struct dt_task_param dt_par;
    struct bat_task_param bat_par;

    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;

    datetime_timer = lv_timer_create(datetime_task, UINT16_MAX / 2, &dt_par);
    lv_timer_ready(datetime_timer);

    capacity_timer = lv_timer_create(capacity_task, UINT16_MAX / 2, &bat_par);
    lv_timer_ready(capacity_timer);

    glyph_timer = lv_timer_create(glyph_task, UINT16_MAX / 64, NULL);
    lv_timer_ready(glyph_timer);

    ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
    lv_timer_ready(ui_refresh_timer);

    init_elements();

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

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();

    if (file_exist(MUOS_PDI_LOAD)) {
        prev_dir = read_text_from_file(MUOS_PDI_LOAD);
    }

    pthread_t gen_item_thread;

    switch (module) {
        case ROOT:
            pthread_create(&gen_item_thread, NULL, (void *) explore_root, (void *) NULL);

            char *SD2_lc = str_tolower(strcpy(malloc(strlen(SD2) + 1), SD2));
            char *USB_lc = str_tolower(strcpy(malloc(strlen(E_USB) + 1), E_USB));

            if (str_startswith(USB_lc, str_tolower(prev_dir))) {
                sys_index = 2;
            } else if (str_startswith(SD2_lc, str_tolower(prev_dir))) {
                sys_index = 1;
            } else {
                sys_index = 0;
            }

            free(SD2_lc);
            free(USB_lc);

            write_text_to_file(MUOS_PDI_LOAD, "explore", "w");
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
                    pthread_create(&gen_item_thread, NULL, (void *) create_explore_items,
                                   (void *) &ui_count);
                } else {
                    pthread_create(&gen_item_thread, NULL, (void *) explore_root, (void *) NULL);
                }
            } else {
                switch (module) {
                    case MMC:
                        sd_dir = strip_dir(SD1);
                        pthread_create(&gen_item_thread, NULL, (void *) create_explore_items,
                                       (void *) &ui_count);
                        break;
                    case SDCARD:
                        sd_dir = strip_dir(SD2);
                        pthread_create(&gen_item_thread, NULL, (void *) create_explore_items,
                                       (void *) &ui_count);
                        break;
                    case USB:
                        sd_dir = strip_dir(E_USB);
                        pthread_create(&gen_item_thread, NULL, (void *) create_explore_items,
                                       (void *) &ui_count);
                        break;
                    default:
                        pthread_create(&gen_item_thread, NULL, (void *) explore_root, NULL);
                        break;
                }
            }
            if (sd_dir != NULL) {
                write_text_to_file(MUOS_PDI_LOAD, get_last_dir(sd_dir), "w");
            }
            if (strcasecmp(read_text_from_file(MUOS_PDI_LOAD), "roms") == 0) {
                write_text_to_file(MUOS_PDI_LOAD, get_last_subdir(sd_dir, '/', 4), "w");
            }
            break;
        case FAVOURITE:
            pthread_create(&gen_item_thread, NULL, (void *) create_root_items, "favourite");
            write_text_to_file(MUOS_PDI_LOAD, "favourite", "w");
            break;
        case HISTORY:
            pthread_create(&gen_item_thread, NULL, (void *) create_root_items, "history");
            write_text_to_file(MUOS_PDI_LOAD, "history", "w");
            break;
    }

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

    pthread_join(gen_item_thread, NULL);
    init_footer_elements();
    if (ui_count > 0) {
        lv_obj_update_layout(ui_pnlContent);
        if (sys_index > -1 && sys_index <= ui_count && current_item_index < ui_count) {
            list_nav_next(sys_index);
        } else {
            image_refresh("box");
        }
    } else {
        nav_moved = 0;
        lv_obj_clear_flag(ui_lblExploreMessage, LV_OBJ_FLAG_HIDDEN);
    }
    pthread_cancel(gen_item_thread);

    update_file_counter();

    pthread_t joystick_thread;
    pthread_create(&joystick_thread, NULL, (void *) joystick_task, NULL);

    while (!safe_quit) {
        usleep(device.SCREEN.WAIT);
    }

    pthread_cancel(joystick_thread);
    free_items(items, item_count);
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
