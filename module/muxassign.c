#include "../lvgl/lvgl.h"
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <libgen.h>
#include <linux/limits.h>
#include "../common/init.h"
#include "../common/log.h"
#include "../common/common.h"
#include "../common/common_core.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/json/json.h"
#include "../common/input/list_nav.h"

char *mux_module;

static int joy_general;
static int joy_power;
static int joy_volume;
static int joy_extra;

int msgbox_active = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;

struct mux_lang lang;
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

void show_help() {
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     lang.MUXASSIGN.TITLE, lang.MUXASSIGN.HELP);
}

char **read_assign_ini(const char *filename, int *cores) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror(lang.SYSTEM.FAIL_FILE_OPEN);
        return NULL;
    }

    char **headers = (char **) malloc(MAX_BUFFER_SIZE * sizeof(char *));
    if (!headers) {
        perror(lang.SYSTEM.FAIL_ALLOCATE_MEM);
        fclose(file);
        return NULL;
    }

    *cores = 0;
    char line[MAX_BUFFER_SIZE];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '[') {
            char *start = strstr(line, "[");
            char *end = strstr(line, "]");
            if (start && end && start < end) {
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
    snprintf(assign_dir, sizeof(assign_dir), "%s/%s",
             device.STORAGE.ROM.MOUNT, STORE_LOC_ASIN);

    ad = opendir(assign_dir);
    if (!ad) return;

    char **file_names = NULL;
    size_t file_count = 0;

    while ((af = readdir(ad))) {
        if (af->d_type == DT_REG) {
            char filename[FILENAME_MAX];
            snprintf(filename, sizeof(filename), "%s/%s", assign_dir, af->d_name);

            char *last_dot = strrchr(af->d_name, '.');
            if (last_dot) *last_dot = '\0';

            char **temp = realloc(file_names, (file_count + 1) * sizeof(char *));
            if (!temp) {
                if (file_names) {
                    for (size_t i = 0; i < file_count; i++) {
                        if (!file_names[i]) continue;
                        free(file_names[i]);
                    }
                    free(file_names);
                    file_names = NULL;
                }
                break;
            }

            file_names = temp;
            file_names[file_count] = strdup(af->d_name);
            if (!file_names[file_count]) {
                for (size_t i = 0; i < file_count; i++) {
                    if (!file_names[i]) continue;
                    free(file_names[i]);
                }
                free(file_names);
                file_names = NULL;
                break;
            }

            file_count++;
        }
    }

    closedir(ad);

    if (!file_names) return;
    qsort(file_names, file_count, sizeof(char *), str_compare);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    if (file_count > 0) {
        for (size_t i = 0; i < file_count; i++) {
            if (!file_names[i]) continue;
            char *base_filename = file_names[i];

            ui_count++;

            lv_obj_t *ui_pnlCore = lv_obj_create(ui_pnlContent);
            apply_theme_list_panel(ui_pnlCore);
            lv_obj_set_user_data(ui_pnlCore, strdup(base_filename));

            lv_obj_t *ui_lblCoreItem = lv_label_create(ui_pnlCore);
            apply_theme_list_item(&theme, ui_lblCoreItem, base_filename);
            lv_obj_set_user_data(ui_lblCoreItem, strdup(base_filename));

            lv_obj_t *ui_lblCoreItemGlyph = lv_img_create(ui_pnlCore);
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
             "%s/%s/%s.ini",
             device.STORAGE.ROM.MOUNT, STORE_LOC_ASIN, rom_system);

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
    char *directory_core = get_directory_core(rom_dir, 1);
    char *file_core = get_file_core(rom_dir, rom_name);
    char filename[FILENAME_MAX];
    snprintf(filename, sizeof(filename), "%s/%s/%s.ini",
             device.STORAGE.ROM.MOUNT, STORE_LOC_ASIN, target);

    int cores;
    char **core_headers = read_assign_ini(filename, &cores);
    if (!core_headers) return;

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
            if (!strcasecmp(core_headers[i], skip_entries[k])) {
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
        if (strcasecmp(file_core, directory_core) != 0 && !strcasecmp(file_core, rawcore)) {
            snprintf(display_name, sizeof(display_name), "%s (%s)", core_headers[i], lang.MUXASSIGN.FILE);
        } else if (!strcasecmp(directory_core, rawcore)) {
            snprintf(display_name, sizeof(display_name), "%s (%s)", core_headers[i], lang.MUXASSIGN.DIR);
        } else {
            snprintf(display_name, sizeof(display_name), "%s", core_headers[i]);
        }

        lv_obj_t *ui_pnlCore = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlCore);
        lv_obj_set_user_data(ui_pnlCore, strdup(display_name));

        lv_obj_t *ui_lblCoreItem = lv_label_create(ui_pnlCore);
        apply_theme_list_item(&theme, ui_lblCoreItem, display_name);
        lv_obj_set_user_data(ui_lblCoreItem, strdup(core_headers[i]));

        lv_obj_t *ui_lblCoreItemGlyph = lv_img_create(ui_pnlCore);

        char *glyph = !strcasecmp(core_headers[i], assign_default) ? "default" : "core";
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
        current_item_index = !current_item_index ? ui_count - 1 : current_item_index - 1;
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

void handle_a() {
    if (msgbox_active) return;

    const char *u_data = str_trim(lv_obj_get_user_data(lv_group_get_focused(ui_group)));
    if (!strcasecmp(rom_system, "none")) {
        load_assign(rom_name, rom_dir, u_data, 0);
    } else {
        LOG_INFO(mux_module, "Single Core Assignment Triggered")

        play_sound("confirm", nav_sound, 0, 1);

        char chosen_core_ini[FILENAME_MAX];
        snprintf(chosen_core_ini, sizeof(chosen_core_ini),
                 "%s/%s/%s.ini",
                 device.STORAGE.ROM.MOUNT, STORE_LOC_ASIN, rom_system);
        mini_t *chosen_core = mini_load(chosen_core_ini);

        const char *raw_core = mini_get_string(
                chosen_core, u_data,
                "core", "none");

        int name_lookup = get_ini_int(chosen_core, "global", "lookup", 0);

        static char core_catalogue[MAX_BUFFER_SIZE];
        strcpy(core_catalogue, get_ini_string(chosen_core, "global",
                                              "catalogue", rom_system));

        create_core_assignment(rom_dir, raw_core, core_catalogue, rom_name,
                               name_lookup, SINGLE);

        mini_free(chosen_core);
    }

    safe_quit(0);
    mux_input_stop();
}

void handle_b() {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound("back", nav_sound, 0, 1);
    if (!strcasecmp(rom_system, "none")) {
        FILE *file = fopen(MUOS_SYS_LOAD, "w");
        fprintf(file, "%s", "");
        fclose(file);
    } else {
        load_assign(rom_name, rom_dir, "none", 0);
    }

    remove(MUOS_SAA_LOAD);

    safe_quit(0);
    mux_input_stop();
}

void handle_x() {
    if (msgbox_active) return;

    if (strcasecmp(rom_system, "none") != 0) {
        LOG_INFO(mux_module, "Directory Core Assignment Triggered")

        play_sound("confirm", nav_sound, 0, 1);

        char chosen_core_ini[FILENAME_MAX];
        snprintf(chosen_core_ini, sizeof(chosen_core_ini),
                 "%s/%s/%s.ini",
                 device.STORAGE.ROM.MOUNT, STORE_LOC_ASIN, rom_system);

        mini_t *chosen_core = mini_load(chosen_core_ini);

        const char *u_data = str_trim(lv_obj_get_user_data(lv_group_get_focused(ui_group)));
        const char *raw_core = mini_get_string(
                chosen_core, u_data,
                "core", "none");

        int name_lookup = get_ini_int(chosen_core, "global", "lookup", 0);

        static char core_catalogue[MAX_BUFFER_SIZE];
        strcpy(core_catalogue, get_ini_string(chosen_core, "global",
                                              "catalogue", rom_system));

        create_core_assignment(rom_dir, raw_core, core_catalogue, rom_name,
                               name_lookup, DIRECTORY);

        mini_free(chosen_core);

        safe_quit(0);
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
                 "%s/%s/%s.ini",
                 device.STORAGE.ROM.MOUNT, STORE_LOC_ASIN, rom_system);

        mini_t *chosen_core = mini_load(chosen_core_ini);

        const char *u_data = str_trim(lv_obj_get_user_data(lv_group_get_focused(ui_group)));
        const char *raw_core = mini_get_string(
                chosen_core, u_data,
                "core", "none");

        int name_lookup = get_ini_int(chosen_core, "global", "lookup", 0);

        static char core_catalogue[MAX_BUFFER_SIZE];
        strcpy(core_catalogue, get_ini_string(chosen_core, "global",
                                              "catalogue", rom_system));

        create_core_assignment(rom_dir, raw_core, core_catalogue, rom_name,
                               name_lookup, PARENT);

        mini_free(chosen_core);

        safe_quit(0);
        mux_input_stop();
    }
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

    if (bar_footer) lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, "");

    lv_label_set_text(ui_lblNavA, lang.GENERIC.SELECT);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    if (strcasecmp(rom_system, "none") != 0) {
        lv_label_set_text(ui_lblNavA, lang.GENERIC.INDIVIDUAL);
        lv_label_set_text(ui_lblNavX, lang.GENERIC.DIRECTORY);
        lv_label_set_text(ui_lblNavY, lang.GENERIC.RECURSIVE);

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

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
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

    if (!auto_assign || !rom_name || !rom_dir || !rom_system) {
        fprintf(stderr, cmd_help, argv[0]);
        return 1;
    }

    mux_module = basename(argv[0]);
    setup_background_process();

    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    LOG_INFO(mux_module, "Assign Core ROM_NAME: \"%s\"", rom_name)
    LOG_INFO(mux_module, "Assign Core ROM_DIR: \"%s\"", rom_dir)
    LOG_INFO(mux_module, "Assign Core ROM_SYS: \"%s\"", rom_system)

    if (safe_atoi(auto_assign) && !file_exist(MUOS_SAA_LOAD)) {
        if (automatic_assign_core(rom_dir) || !strcmp(rom_system, "none")) {
            safe_quit(0);
            return 0;
        }
    }

    init_theme(1, 0);
    init_display();

    init_ui_common_screen(&theme, &device, &lang, "");
    init_timer(ui_refresh_task, NULL);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_sound(&nav_sound, mux_module);

    if (!strcasecmp(rom_system, "none")) {
        create_system_items();
    } else {
        create_core_items(rom_system);
    }

    if (ui_count > 0) {
        if (!strcasecmp(rom_system, "none")) {
            LOG_SUCCESS(mux_module, "%d System%s Detected", ui_count, ui_count == 1 ? "" : "s")
        } else {
            LOG_SUCCESS(mux_module, "%d Core%s Detected", ui_count, ui_count == 1 ? "" : "s")
        }
        char title[MAX_BUFFER_SIZE];
        snprintf(title, sizeof(title), "%s - %s", lang.MUXASSIGN.TITLE, get_last_dir(rom_dir));
        lv_label_set_text(ui_lblTitle, title);
        list_nav_next(0);
    } else {
        LOG_ERROR(mux_module, "No Cores Detected - Check Directory!")
        lv_label_set_text(ui_lblScreenMessage, lang.MUXASSIGN.NONE);
    }

    load_kiosk(&kiosk);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
