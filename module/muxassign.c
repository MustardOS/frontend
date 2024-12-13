#include "../lvgl/lvgl.h"
#include "../lvgl/src/drivers/fbdev.h"
#include "../lvgl/src/drivers/evdev.h"
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <libgen.h>
#include "../common/log.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/json/json.h"
#include "../common/input.h"
#include "../common/input/list_nav.h"

char *mux_module;
static int js_fd;
static int js_fd_sys;

int turbo_mode = 0;
int msgbox_active = 0;
int SD2_found = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;

struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int nav_moved = 1;
lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;
lv_obj_t *kiosk_image = NULL;

int progress_onscreen = -1;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

lv_obj_t *ui_mux_panels[5];

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
};

void show_help() {
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(ui_lblTitle)), TS("This is where you can assign a core or "
                                                            "external emulator to content"));
}

void free_lines(char *lines[], int line_count) {
    for (int i = 0; i < line_count; i++) {
        free(lines[i]);
    }
}

void modify_cfg_file(const char *filename, const char *core, const char *sys, const char *cache) {
    printf("Updating file: %s\n", filename);
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Failed to open file");
        return;
    }

    int max_lines = 10;
    char *lines[max_lines];
    int line_count = 0;

    char line[MAX_BUFFER_SIZE];
    while (fgets(line, sizeof(line), file)) {
        if (line_count >= max_lines) {
            fprintf(stderr, "File too large, exceeding max lines limit\n");
            fclose(file);
            return;
        }
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        lines[line_count] = strdup(line);
        line_count++;
    }
    fclose(file);

    if (line_count >= 2) {
        free(lines[1]);
        lines[1] = strdup(core);
    }
    if (line_count >= 3) {
        free(lines[2]);
        lines[2] = strdup(sys);
    }
    if (line_count >= 4) {
        free(lines[3]);
        lines[3] = strdup(cache);
    }

    if (remove(filename) != 0) {
        perror("Failed to delete original file");
        free_lines(lines, line_count);
        return;
    }

    FILE *new_file = fopen(filename, "w");
    if (new_file == NULL) {
        perror("Failed to create new file");
        free_lines(lines, line_count);
        return;
    }

    for (int i = 0; i < line_count; i++) {
        fprintf(new_file, "%s\n", lines[i]);
    }
    free_lines(lines, line_count);

    fclose(new_file);
}

// Function to scan the directory and find .cfg files
void update_cfg_files(const char *dirpath, const char *core, const char *sys, const int cache) {
    char cache_char[MAX_BUFFER_SIZE];
    snprintf(cache_char, sizeof(cache_char), "%d", cache);

    DIR *dir = opendir(dirpath);
    if (dir == NULL) {
        perror("Failed to open directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".cfg") && strcmp(entry->d_name, "core.cfg") != 0) {
            char filepath[PATH_MAX];
            snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, entry->d_name);
            modify_cfg_file(filepath, core, sys, cache_char);
        }
    }

    closedir(dir);
}

void assign_core_single(char *core_dir, const char *core, char *sys, char *rom, int cache) {
    char rom_path[MAX_BUFFER_SIZE];
    snprintf(rom_path, sizeof(rom_path), "%s/%s.cfg",
             core_dir, strip_ext(rom));

    if (file_exist(rom_path)) remove(rom_path);

    FILE *rom_file = fopen(rom_path, "w");
    if (rom_file == NULL) {
        perror("Error opening file rom_path file");
        return;
    }

    LOG_INFO(mux_module, "Single Assign Content:\n\t%s\n\t%s\n\t%s\n\t%d\n\t%s\n\t%s\n\t%s",
             strip_ext(rom), core, str_trim(sys), cache,
             str_replace(rom_dir, get_last_subdir(rom_dir, '/', 4), ""),
             get_last_subdir(rom_dir, '/', 4), rom)
    fprintf(rom_file, "%s\n%s\n%s\n%d\n%s\n%s\n%s\n",
            strip_ext(rom), core, str_trim(sys), cache,
            str_replace(rom_dir, get_last_subdir(rom_dir, '/', 4), ""),
            get_last_subdir(rom_dir, '/', 4), rom);
    fclose(rom_file);
}

void assign_core_directory(char *core_dir, const char *core, char *sys, int cache, int purge) {
    if (purge) {
        delete_files_of_type(core_dir, "/core.cfg", NULL, 0);
        update_cfg_files(core_dir, core, str_trim(sys), cache);
    }

    char core_file[MAX_BUFFER_SIZE];
    snprintf(core_file, sizeof(core_file), "%s/%s/core.cfg",
             INFO_COR_PATH, get_last_subdir(rom_dir, '/', 4));

    FILE *file = fopen(core_file, "w");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    fprintf(file, "%s\n%s\n%d\n", core, str_trim(sys), cache);
    fclose(file);
}

void assign_core_parent(char *core_dir, const char *core, char *sys, int cache) {
    assign_core_directory(core_dir, core, sys, cache, 1);

    char **subdirs = get_subdirectories(rom_dir);
    if (subdirs != NULL) {
        for (int i = 0; subdirs[i] != NULL; i++) {
            char subdir_file[MAX_BUFFER_SIZE];
            snprintf(subdir_file, sizeof(subdir_file), "%s%s/core.cfg", core_dir, subdirs[i]);

            create_directories(strip_dir(subdir_file));

            FILE *subdir_file_handle = fopen(subdir_file, "w");
            if (subdir_file_handle == NULL) {
                perror("Error opening file");
                continue;
            }

            fprintf(subdir_file_handle, "%s\n%s\n%d\n", core, str_trim(sys), cache);
            fclose(subdir_file_handle);

            char core_dir_path[MAX_BUFFER_SIZE];
            snprintf(core_dir_path, sizeof(core_dir_path), "%s%s", core_dir, subdirs[i]);
            update_cfg_files(core_dir_path, core, str_trim(sys), cache);
        }
        free_subdirectories(subdirs);
    }
}

void create_core_assignment(const char *core, char *sys, char *rom, int cache, enum core_gen_type method) {
    char core_dir[MAX_BUFFER_SIZE];
    snprintf(core_dir, sizeof(core_dir), "%s/%s/",
             INFO_COR_PATH, get_last_subdir(rom_dir, '/', 4));

    create_directories(core_dir);

    switch (method) {
        case SINGLE:
            assign_core_single(core_dir, core, sys, rom, cache);
            break;
        case PARENT:
            assign_core_parent(core_dir, core, sys, cache);
            break;
        case DIRECTORY:
            assign_core_directory(core_dir, core, sys, cache, 1);
            break;
        case DIRECTORY_NO_WIPE:
        default:
            assign_core_directory(core_dir, core, sys, cache, 0);
            break;
    }

    char pico8_splore[MAX_BUFFER_SIZE];
    snprintf(pico8_splore, sizeof(pico8_splore), "%s/Splore.p8", rom_dir);
    if (strcasecmp(core, "ext-pico8") == 0 && !file_exist(pico8_splore)) {
        run_exec((const char *[]) {"touch", pico8_splore, NULL});
    }

    if (file_exist(MUOS_SAA_LOAD)) {
        remove(MUOS_SAA_LOAD);
    }
}

char **read_assign_ini(const char *filename, int *cores) {
    FILE *file = fopen(filename, "r");
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
    DIR *ad;
    struct dirent *af;

    char assign_dir[PATH_MAX];
    snprintf(assign_dir, sizeof(assign_dir), "%s/MUOS/info/assign",
             device.STORAGE.ROM.MOUNT);

    ad = opendir(assign_dir);
    if (ad == NULL) {
        lv_obj_clear_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    char **file_names = NULL;
    size_t file_count = 0;

    while ((af = readdir(ad))) {
        if (af->d_type == DT_REG) {
            char filename[FILENAME_MAX];
            snprintf(filename, sizeof(filename), "%s/%s", assign_dir, af->d_name);

            char *last_dot = strrchr(af->d_name, '.');
            if (last_dot != NULL) {
                *last_dot = '\0';
            }

            file_names = realloc(file_names, (file_count + 1) * sizeof(char *));
            file_names[file_count] = strdup(af->d_name);
            file_count++;
        }
    }

    closedir(ad);
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
            lv_obj_set_user_data(ui_pnlCore, strdup(base_filename));

            lv_obj_t * ui_lblCoreItem = lv_label_create(ui_pnlCore);
            apply_theme_list_item(&theme, ui_lblCoreItem, base_filename, true, false);
            lv_obj_set_user_data(ui_lblCoreItem, strdup(base_filename));

            lv_obj_t * ui_lblCoreItemGlyph = lv_img_create(ui_pnlCore);
            apply_theme_list_glyph(&theme, ui_lblCoreItemGlyph, mux_module, "system");

            lv_group_add_obj(ui_group, ui_lblCoreItem);
            lv_group_add_obj(ui_group_glyph, ui_lblCoreItemGlyph);
            lv_group_add_obj(ui_group_panel, ui_pnlCore);

            apply_size_to_content(&theme, ui_pnlContent, ui_lblCoreItem, ui_lblCoreItemGlyph, base_filename);
            apply_text_long_dot(&theme, ui_pnlContent, ui_lblCoreItem, base_filename);
        }
        if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
    }
}

char *get_raw_core(const char *group) {
    char chosen_core_ini[FILENAME_MAX];
    snprintf(chosen_core_ini, sizeof(chosen_core_ini),
             "%s/MUOS/info/assign/%s.ini",
             device.STORAGE.ROM.MOUNT, rom_system);

    mini_t *chosen_core = mini_load(chosen_core_ini);

    const char *raw_core = strdup(mini_get_string(
            chosen_core, group,
            "core", "none"));
    char *raw_core_copy = NULL;
    raw_core_copy = malloc(strlen(raw_core) + 1);
    if (raw_core_copy) strcpy(raw_core_copy, raw_core);

    mini_free(chosen_core);
    return raw_core_copy;
}

void create_core_items(const char *target) {
    char *directory_core = get_directory_core(rom_dir);
    char *file_core = get_file_core(rom_dir, rom_name);
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
    mini_t *assign_ini = mini_try_load(filename);

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
            LOG_INFO(mux_module, "Skipping Non-Assignable Core: %s", core_headers[i])
            continue;
        }

        LOG_SUCCESS(mux_module, "Generating Item For Core: %s", core_headers[i])

        ui_count++;

        char *rawcore = get_raw_core(core_headers[i]);
        char display_name[MAX_BUFFER_SIZE];
        if (strcasecmp(file_core, directory_core) != 0 && strcasecmp(file_core, rawcore) == 0) {
            snprintf(display_name, sizeof(display_name), "%s (%s)", core_headers[i], TS("Assigned to file"));
        } else if (strcasecmp(directory_core, rawcore) == 0) {
            snprintf(display_name, sizeof(display_name), "%s (%s)", core_headers[i], TS("Assigned to directory"));
        } else {
            snprintf(display_name, sizeof(display_name), "%s", core_headers[i]);
        }

        lv_obj_t * ui_pnlCore = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(&theme, &device, ui_pnlCore);
        lv_obj_set_user_data(ui_pnlCore, strdup(display_name));

        lv_obj_t * ui_lblCoreItem = lv_label_create(ui_pnlCore);
        apply_theme_list_item(&theme, ui_lblCoreItem, display_name, true, false);
        lv_obj_set_user_data(ui_lblCoreItem, strdup(core_headers[i]));

        lv_obj_t * ui_lblCoreItemGlyph = lv_img_create(ui_pnlCore);

        char *glyph = (strcasecmp(core_headers[i], assign_default) == 0) ? "default" : "core";
        apply_theme_list_glyph(&theme, ui_lblCoreItemGlyph, mux_module, glyph);

        lv_group_add_obj(ui_group, ui_lblCoreItem);
        lv_group_add_obj(ui_group_glyph, ui_lblCoreItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlCore);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblCoreItem, ui_lblCoreItemGlyph, display_name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblCoreItem, display_name);

        free(core_headers[i]);
    }

    free(assign_default);

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
    free(core_headers);
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    nav_moved = 1;
}

void list_nav_next(int steps) {
    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound, 0, 0);
    }
    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    nav_moved = 1;
}

void handle_confirm() {
    if (msgbox_active) return;

    const char *u_data = str_trim(lv_obj_get_user_data(lv_group_get_focused(ui_group)));
    if (strcasecmp(rom_system, "none") == 0) {
        load_assign(rom_name, rom_dir, u_data, 0);
    } else {
        LOG_INFO(mux_module, "Single Core Assignment Triggered")

        play_sound("confirm", nav_sound, 0, 1);

        char chosen_core_ini[FILENAME_MAX];
        snprintf(chosen_core_ini, sizeof(chosen_core_ini),
                 "%s/MUOS/info/assign/%s.ini",
                 device.STORAGE.ROM.MOUNT, rom_system);
        mini_t *chosen_core = mini_load(chosen_core_ini);

        const char *raw_core = mini_get_string(
                chosen_core, u_data,
                "core", "none");

        int name_lookup = mini_get_int(chosen_core, "global", "lookup", 0);

        static char core_catalogue[MAX_BUFFER_SIZE];
        strcpy(core_catalogue, get_ini_string(chosen_core, "global",
                                              "catalogue", rom_system));

        create_core_assignment(raw_core, core_catalogue, rom_name,
                               name_lookup, SINGLE);

        mini_free(chosen_core);
    }

    mux_input_stop();
}

void handle_x() {
    if (msgbox_active) return;

    if (strcasecmp(rom_system, "none") != 0) {
        LOG_INFO(mux_module, "Directory Core Assignment Triggered")

        play_sound("confirm", nav_sound, 0, 1);

        char chosen_core_ini[FILENAME_MAX];
        snprintf(chosen_core_ini, sizeof(chosen_core_ini),
                 "%s/MUOS/info/assign/%s.ini",
                 device.STORAGE.ROM.MOUNT, rom_system);

        mini_t *chosen_core = mini_load(chosen_core_ini);

        const char *u_data = str_trim(lv_obj_get_user_data(lv_group_get_focused(ui_group)));
        const char *raw_core = mini_get_string(
                chosen_core, u_data,
                "core", "none");

        int name_lookup = mini_get_int(chosen_core, "global", "lookup", 0);

        static char core_catalogue[MAX_BUFFER_SIZE];
        strcpy(core_catalogue, get_ini_string(chosen_core, "global",
                                              "catalogue", rom_system));

        create_core_assignment(raw_core, core_catalogue, rom_name,
                               name_lookup, DIRECTORY);

        mini_free(chosen_core);

        mux_input_stop();
    }
}

void handle_y() {
    if (msgbox_active) return;

    if (strcasecmp(rom_system, "none") != 0) {
        LOG_INFO(mux_module, "Parent Core Assignment Triggered")

        play_sound("confirm", nav_sound, 0, 1);

        char chosen_core_ini[FILENAME_MAX];
        snprintf(chosen_core_ini, sizeof(chosen_core_ini),
                 "%s/MUOS/info/assign/%s.ini",
                 device.STORAGE.ROM.MOUNT, rom_system);

        mini_t *chosen_core = mini_load(chosen_core_ini);

        const char *u_data = str_trim(lv_obj_get_user_data(lv_group_get_focused(ui_group)));
        const char *raw_core = mini_get_string(
                chosen_core, u_data,
                "core", "none");

        int name_lookup = mini_get_int(chosen_core, "global", "lookup", 0);

        static char core_catalogue[MAX_BUFFER_SIZE];
        strcpy(core_catalogue, get_ini_string(chosen_core, "global",
                                              "catalogue", rom_system));

        create_core_assignment(raw_core, core_catalogue, rom_name,
                               name_lookup, PARENT);

        mini_free(chosen_core);

        mux_input_stop();
    }
}

void handle_back() {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound("back", nav_sound, 0, 1);
    if (strcasecmp(rom_system, "none") == 0) {
        FILE *file = fopen(MUOS_SYS_LOAD, "w");
        fprintf(file, "%s", "");
        fclose(file);
    } else {
        load_assign(rom_name, rom_dir, "none", 0);
    }

    remove(MUOS_SAA_LOAD);
    mux_input_stop();
}

void handle_help() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 0, 0);
        show_help();
    }
}

void init_elements() {
    ui_mux_panels[0] = ui_pnlFooter;
    ui_mux_panels[1] = ui_pnlHeader;
    ui_mux_panels[2] = ui_pnlHelp;
    ui_mux_panels[3] = ui_pnlProgressBrightness;
    ui_mux_panels[4] = ui_pnlProgressVolume;

    adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

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

    lv_label_set_text(ui_lblNavA, TG("Select"));
    lv_label_set_text(ui_lblNavB, TG("Back"));

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
        lv_label_set_text(ui_lblNavA, TG("Individual"));
        lv_label_set_text(ui_lblNavX, TG("Directory"));
        lv_label_set_text(ui_lblNavY, TG("Recursive"));

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

    if (TEST_IMAGE) display_testing_message(ui_screen);

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image, theme.MISC.IMAGE_OVERLAY);
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
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    mux_module = basename(argv[0]);
    load_device(&device);


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

    LOG_INFO(mux_module, "Assign Core ROM_NAME: \"%s\"", rom_name)
    LOG_INFO(mux_module, "Assign Core ROM_DIR: \"%s\"", rom_dir)
    LOG_INFO(mux_module, "Assign Core ROM_SYS: \"%s\"", rom_system)

    if (safe_atoi(auto_assign) && !file_exist(MUOS_SAA_LOAD)) {
        LOG_INFO(mux_module, "Automatic Assign Core Initiated")

        char core_file[MAX_BUFFER_SIZE];
        snprintf(core_file, sizeof(core_file), "%s/%s/core.cfg",
                 INFO_COR_PATH, get_last_subdir(rom_dir, '/', 4));

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

                LOG_INFO(mux_module, "<Automatic Core Assign> Core Assigned: %s", ass_config)

                char assigned_core_ini[MAX_BUFFER_SIZE];
                snprintf(assigned_core_ini, sizeof(assigned_core_ini), "%s/MUOS/info/assign/%s",
                         device.STORAGE.ROM.MOUNT, ass_config);

                LOG_INFO(mux_module, "<Automatic Core Assign> Obtaining Core INI: %s", assigned_core_ini)

                mini_t *core_config_ini = mini_load(assigned_core_ini);

                static char def_core[MAX_BUFFER_SIZE];
                strcpy(def_core, get_ini_string(core_config_ini, "global", "default", "none"));

                LOG_INFO(mux_module, "<Automatic Core Assign> Default Core: %s", def_core)

                if (strcmp(def_core, "none") != 0) {
                    static char auto_core[MAX_BUFFER_SIZE];
                    strcpy(auto_core, get_ini_string(core_config_ini, def_core, "core", "invalid"));

                    LOG_INFO(mux_module, "<Automatic Core Assign> Assigned Core To: %s", auto_core)

                    if (strcmp(def_core, "invalid") != 0) {
                        static char core_catalogue[MAX_BUFFER_SIZE];
                        strcpy(core_catalogue, get_ini_string(core_config_ini, "global", "catalogue", "none"));

                        int name_lookup = mini_get_int(core_config_ini, "global", "lookup", 0);

                        LOG_INFO(mux_module, "<Automatic Core Assign> Core Cache: %d", name_lookup)
                        LOG_INFO(mux_module, "<Automatic Core Assign> Core Catalogue: %s", core_catalogue)

                        create_core_assignment(auto_core, core_catalogue, rom_name, name_lookup, DIRECTORY_NO_WIPE);

                        auto_assign_good = 1;
                        LOG_SUCCESS(mux_module, "<Automatic Core Assign> Successful")
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
    load_language(mux_module);

    ui_common_screen_init(&theme, &device, "");
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);

    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND,
                   theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND, GENERAL);

    load_font_text(basename(argv[0]), ui_screen);
    load_font_section(basename(argv[0]), FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(mux_module, FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(mux_module, FONT_FOOTER_FOLDER, ui_pnlFooter);

    nav_sound = init_nav_sound(mux_module);

    lv_label_set_text(ui_lblScreenMessage, TS("No Cores Found..."));

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
        LOG_ERROR(mux_module, "Failed to open joystick device!")
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

    if (ui_count > 0) {
        if (strcasecmp(rom_system, "none") == 0) {
            LOG_SUCCESS(mux_module, "%d System%s Detected", ui_count, ui_count == 1 ? "" : "s")
        } else {
            LOG_SUCCESS(mux_module, "%d Core%s Detected", ui_count, ui_count == 1 ? "" : "s")
        }
        char title[MAX_BUFFER_SIZE];
        snprintf(title, sizeof(title), "%s - %s", TS("ASSIGN"), get_last_dir(rom_dir));
        lv_label_set_text(ui_lblTitle, title);
        list_nav_next(0);
    } else {
        LOG_ERROR(mux_module, "No Cores Detected - Check Directory!")
        lv_obj_clear_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
    }

    refresh_screen(device.SCREEN.WAIT);
    load_kiosk(&kiosk);

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
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    // List navigation:
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
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
                            .hold_handler = ui_common_handle_bright,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_bright,
                            .hold_handler = ui_common_handle_bright,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_vol,
                            .hold_handler = ui_common_handle_vol,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_vol,
                            .hold_handler = ui_common_handle_vol,
                    },
            },
            .idle_handler = ui_common_handle_idle,
    };
    mux_input_task(&input_opts);

    close(js_fd);
    close(js_fd_sys);

    LOG_SUCCESS(mux_module, "Safe Quit!")

    return 0;
}
