#include "muxshare.h"
#include "muxassign.h"
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <linux/limits.h>
#include "../common/init.h"
#include "../common/log.h"
#include "../common/common.h"
#include "../common/common_core.h"
#include "../common/ui_common.h"
#include "../common/json/json.h"
#include "../common/input/list_nav.h"

#define UI_PANEL 5
static lv_obj_t *ui_mux_panels[UI_PANEL];

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_system[PATH_MAX];

static void show_help() {
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     lang.MUXASSIGN.TITLE, lang.MUXASSIGN.HELP);
}

static void create_system_items() {
    DIR *ad;
    struct dirent *af;

    char assign_dir[PATH_MAX];
    snprintf(assign_dir, sizeof(assign_dir), "%s/%s",
             device.STORAGE.ROM.MOUNT, STORE_LOC_ASIN);

    ad = opendir(assign_dir);
    if (!ad) return;

    while ((af = readdir(ad))) {
        if (af->d_type == DT_DIR) add_item(&items, &item_count, af->d_name, af->d_name, "", FOLDER);
    }

    closedir(ad);
    sort_items(items, item_count);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; i < item_count; i++) {
        ui_count++;

        lv_obj_t *ui_pnlSystem = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlSystem);

        lv_obj_t *ui_lblSystemItem = lv_label_create(ui_pnlSystem);
        apply_theme_list_item(&theme, ui_lblSystemItem, items[i].name);
        lv_obj_set_user_data(ui_lblSystemItem, items[i].name);

        lv_obj_t *ui_lblSystemItemGlyph = lv_img_create(ui_pnlSystem);
        apply_theme_list_glyph(&theme, ui_lblSystemItemGlyph, mux_module, "system");

        lv_group_add_obj(ui_group, ui_lblSystemItem);
        lv_group_add_obj(ui_group_glyph, ui_lblSystemItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlSystem);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblSystemItem, ui_lblSystemItemGlyph, items[i].name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblSystemItem, items[i].name);
    }

    if (ui_count > 0) {
        lv_obj_update_layout(ui_pnlContent);
        free_items(&items, &item_count);
    }
}

static void create_core_items(const char *target) {
    char assign_dir[PATH_MAX];
    snprintf(assign_dir, sizeof(assign_dir), "%s/%s/%s",
             device.STORAGE.ROM.MOUNT, STORE_LOC_ASIN, target);

    char global_assign[FILENAME_MAX];
    snprintf(global_assign, sizeof(global_assign), "%s/global.ini", assign_dir);

    char default_assign[FILENAME_MAX];
    mini_t *global_config = mini_load(global_assign);
    strncpy(default_assign, get_ini_string(global_config, "global", "default", "none"), sizeof(default_assign));
    default_assign[sizeof(default_assign) - 1] = '\0';
    mini_free(global_config);

    if (!strcmp(default_assign, "none")) return;

    DIR *ad;
    struct dirent *af;

    ad = opendir(assign_dir);
    if (!ad) return;

    while ((af = readdir(ad))) {
        if (af->d_type == DT_REG) {
            if (strcasecmp(af->d_name, "global.ini") == 0) continue;

            char core_file[FILENAME_MAX];
            snprintf(core_file, sizeof(core_file), "%s/%s", assign_dir, af->d_name);

            char *last_dot = strrchr(af->d_name, '.');
            if (last_dot && !strcasecmp(last_dot, ".ini")) {
                *last_dot = '\0';

                mini_t *core_config = mini_load(core_file);

                char assign_name[FILENAME_MAX];
                strncpy(assign_name, get_ini_string(core_config, af->d_name, "name", "none"), sizeof(assign_name));
                assign_name[sizeof(assign_name) - 1] = '\0';

                char assign_core[FILENAME_MAX];
                strncpy(assign_core, get_ini_string(core_config, af->d_name, "core", "none"), sizeof(assign_core));
                assign_core[sizeof(assign_core) - 1] = '\0';

                mini_free(core_config);

                if (strcmp(assign_core, "none") != 0) {
                    add_item(&items, &item_count, assign_name, af->d_name, assign_core, ITEM);
                }
            }
        }
    }

    closedir(ad);
    sort_items(items, item_count);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; i < item_count; i++) {
        ui_count++;

        char *directory_core = get_directory_core(rom_dir, 1);
        char *file_core = get_file_core(rom_dir, rom_name);

        char display_name[MAX_BUFFER_SIZE];
        if (strcasecmp(file_core, directory_core) != 0 && !strcasecmp(file_core, items[i].extra_data)) {
            snprintf(display_name, sizeof(display_name), "%s (%s)", items[i].name, lang.MUXASSIGN.FILE);
        } else if (!strcasecmp(directory_core, items[i].extra_data)) {
            snprintf(display_name, sizeof(display_name), "%s (%s)", items[i].name, lang.MUXASSIGN.DIR);
        } else {
            snprintf(display_name, sizeof(display_name), "%s", items[i].name);
        }

        lv_obj_t *ui_pnlCore = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlCore);

        lv_obj_t *ui_lblCoreItem = lv_label_create(ui_pnlCore);
        apply_theme_list_item(&theme, ui_lblCoreItem, items[i].name);

        lv_obj_t *ui_lblCoreItemGlyph = lv_img_create(ui_pnlCore);
        char *glyph = !strcasecmp(items[i].name, default_assign) ? "default" : "core";
        apply_theme_list_glyph(&theme, ui_lblCoreItemGlyph, mux_module, glyph);

        lv_group_add_obj(ui_group, ui_lblCoreItem);
        lv_group_add_obj(ui_group_glyph, ui_lblCoreItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlCore);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblCoreItem, ui_lblCoreItemGlyph, items[i].name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblCoreItem, items[i].name);
    }

    if (ui_count > 0) {
        lv_obj_update_layout(ui_pnlContent);
        free_items(&items, &item_count);
    }
}

static void list_nav_move(int steps, int direction) {
    if (ui_count <= 0) return;
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE, 0);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group),
                        lv_obj_get_user_data(lv_group_get_focused(ui_group_panel)));
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_b() {
    if (msgbox_active) {
        play_sound(SND_CONFIRM, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK, 0);
    if (!strcasecmp(rom_system, "none")) {
        FILE *file = fopen(MUOS_SYS_LOAD, "w");
        fprintf(file, "%s", "");
        fclose(file);
    } else {
        load_assign(rom_name, rom_dir, "none", 0);
    }

    remove(MUOS_SAA_LOAD);

    close_input();
    mux_input_stop();
}

static void handle_core_assignment(const char *log_msg, int assignment_mode) {
    LOG_INFO(mux_module, "%s", log_msg)
    play_sound(SND_CONFIRM, 0);

    char *selected_item = str_tolower(lv_label_get_text(lv_group_get_focused(ui_group)));
    LOG_INFO(mux_module, "Selected Core: %s (%s)", selected_item, lv_label_get_text(lv_group_get_focused(ui_group)))

    char assign_dir[PATH_MAX];
    snprintf(assign_dir, sizeof(assign_dir), "%s/%s/%s",
             device.STORAGE.ROM.MOUNT, STORE_LOC_ASIN, rom_system);

    char global_core[FILENAME_MAX];
    snprintf(global_core, sizeof(global_core), "%s/global.ini",
             assign_dir);
    mini_t *global_ini = mini_load(global_core);
    LOG_INFO(mux_module, "Global Core Path: %s", global_core)

    char local_core[FILENAME_MAX];
    snprintf(local_core, sizeof(local_core), "%s/%s.ini",
             assign_dir, selected_item);
    mini_t *local_ini = mini_load(local_core);
    LOG_INFO(mux_module, "Global Core Path: %s", local_core)

    static char core_catalogue[MAX_BUFFER_SIZE];
    char *use_local_catalogue = get_ini_string(local_ini, selected_item, "catalogue", "none");
    if (strcmp(use_local_catalogue, "none") != 0) {
        strcpy(core_catalogue, use_local_catalogue);
    } else {
        strcpy(core_catalogue, get_ini_string(global_ini, "global", "catalogue", "none"));
    }
    LOG_INFO(mux_module, "Content Core Catalogue: %s", core_catalogue)

    static int core_lookup;
    int use_local_lookup = get_ini_int(local_ini, selected_item, "lookup", 0);
    core_lookup = use_local_lookup ? use_local_lookup : get_ini_int(global_ini, "global", "lookup", 0);
    LOG_INFO(mux_module, "Content Core Lookup: %d", core_lookup)

    static char core_launch[MAX_BUFFER_SIZE];
    strcpy(core_launch, get_ini_string(local_ini, selected_item, "core", "none"));
    LOG_INFO(mux_module, "Content Core Launcher: %s", core_launch)

    create_core_assignment(rom_dir, core_launch, core_catalogue, rom_name, core_lookup, assignment_mode);

    mini_free(global_ini);
    mini_free(local_ini);
}

static void handle_a() {
    if (msgbox_active) return;

    if (!strcasecmp(rom_system, "none")) {
        play_sound(SND_CONFIRM, 0);
        load_assign(rom_name, rom_dir, lv_label_get_text(lv_group_get_focused(ui_group)), 0);
    } else {
        handle_core_assignment("Single Core Assignment Triggered", SINGLE);
    }

    close_input();
    mux_input_stop();
}

static void handle_x() {
    if (msgbox_active || !strcasecmp(rom_system, "none")) return;

    handle_core_assignment("Directory Core Assignment Triggered", DIRECTORY);

    close_input();
    mux_input_stop();
}

static void handle_y() {
    if (msgbox_active || !strcasecmp(rom_system, "none")) return;

    handle_core_assignment("Parent Core Assignment Triggered", PARENT);

    close_input();
    mux_input_stop();
}

static void handle_help() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM, 0);
        show_help();
    }
}

static void init_elements() {
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
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    }

    if (strcasecmp(rom_system, "none") != 0) {
        lv_label_set_text(ui_lblNavA, lang.GENERIC.INDIVIDUAL);
        lv_label_set_text(ui_lblNavX, lang.GENERIC.DIRECTORY);
        lv_label_set_text(ui_lblNavY, lang.GENERIC.RECURSIVE);

        lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);

        lv_obj_clear_flag(ui_lblNavY, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_lblNavYGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);

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

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxassign_main(int auto_assign, char *name, char *dir, char *sys) {
    snprintf(rom_name, sizeof(rom_name), "%s", name);
    snprintf(rom_dir, sizeof(rom_name), "%s", dir);
    snprintf(rom_system, sizeof(rom_name), "%s", sys);

    init_module("muxassign");

    LOG_INFO(mux_module, "Assign Core ROM_NAME: \"%s\"", rom_name)
    LOG_INFO(mux_module, "Assign Core ROM_DIR: \"%s\"", rom_dir)
    LOG_INFO(mux_module, "Assign Core ROM_SYS: \"%s\"", rom_system)

    if (auto_assign && !file_exist(MUOS_SAA_LOAD)) {
        if (automatic_assign_core(rom_dir) || !strcmp(rom_system, "none")) {
            close_input();
            return 0;
        }
    }

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, "");
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);
    init_fonts();

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

    init_timer(ui_refresh_task, NULL);

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
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
