#include "../lvgl/lvgl.h"
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/collection.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/input/list_nav.h"

char *mux_module;

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

size_t item_count = 0;
content_item *items = NULL;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

int ui_count = 0;
int current_item_index = 0;
int first_open = 1;

lv_obj_t *ui_mux_panels[5];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help();

void create_task_items();

void list_nav_prev(int steps);

void list_nav_next(int steps);

void init_elements();

void update_footer_nav_elements();

void ui_refresh_task();

void show_help() {
    char *title = items[current_item_index].name;

    char help_info[MAX_BUFFER_SIZE];
    snprintf(help_info, sizeof(help_info),
             "%s/%s/%s.sh", device.STORAGE.ROM.MOUNT, MUOS_TASK_PATH, title);

    char *message = get_script_value(help_info, "HELP", lang.GENERIC.NO_HELP);
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent, TS(title), TS(message));
}

void create_task_items() {
    char task_path[MAX_BUFFER_SIZE];
    snprintf(task_path, sizeof(task_path),
             "%s/%s", device.STORAGE.ROM.MOUNT, MUOS_TASK_PATH);

    const char *task_directories[] = {
            task_path
    };
    char task_dir[MAX_BUFFER_SIZE];

    char **file_names = NULL;
    size_t file_count = 0;

    for (size_t dir_index = 0; dir_index < sizeof(task_directories) / sizeof(task_directories[0]); ++dir_index) {
        snprintf(task_dir, sizeof(task_dir), "%s/", task_directories[dir_index]);

        DIR *ad = opendir(task_dir);
        if (!ad) continue;

        struct dirent *tf;
        while ((tf = readdir(ad))) {
            if (tf->d_type == DT_REG) {
                char *last_dot = strrchr(tf->d_name, '.');
                if (last_dot && strcasecmp(last_dot, ".sh") == 0) {
                    char **temp = realloc(file_names, (file_count + 1) * sizeof(char *));
                    if (!temp) {
                        perror(lang.SYSTEM.FAIL_ALLOCATE_MEM);
                        free(file_names);
                        closedir(ad);
                        return;
                    }
                    file_names = temp;

                    char full_task_name[MAX_BUFFER_SIZE];
                    snprintf(full_task_name, sizeof(full_task_name), "%s%s", task_dir, tf->d_name);
                    file_names[file_count] = strdup(full_task_name);
                    if (!file_names[file_count]) {
                        perror(lang.SYSTEM.FAIL_DUP_STRING);
                        free(file_names);
                        closedir(ad);
                        return;
                    }
                    file_count++;
                }
            }
        }
        closedir(ad);
    }

    if (!file_names) return;
    qsort(file_names, file_count, sizeof(char *), str_compare);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; i < file_count; i++) {
        if (!file_names[i]) continue;
        char *base_filename = file_names[i];

        char task_name[MAX_BUFFER_SIZE];
        snprintf(task_name, sizeof(task_name), "%s",
                 str_remchar(str_replace(base_filename, strip_dir(base_filename), ""), '/'));

        char task_store[MAX_BUFFER_SIZE];
        snprintf(task_store, sizeof(task_store), "%s", strip_ext(task_name));

        ui_count++;

        add_item(&items, &item_count, task_store, TS(task_store), file_names[i], ROM);

        lv_obj_t *ui_pnlTask = lv_obj_create(ui_pnlContent);
        if (ui_pnlTask) {
            apply_theme_list_panel(ui_pnlTask);
            lv_obj_set_user_data(ui_pnlTask, strdup(TS(task_store)));

            lv_obj_t *ui_lblTaskItem = lv_label_create(ui_pnlTask);
            if (ui_lblTaskItem) apply_theme_list_item(&theme, ui_lblTaskItem, TS(task_store));

            lv_obj_t *ui_lblTaskItemGlyph = lv_img_create(ui_pnlTask);
            if (ui_lblTaskItemGlyph) {
                apply_theme_list_glyph(&theme, ui_lblTaskItemGlyph, mux_module,
                                       get_script_value(items[i].extra_data, "ICON", "task"));
            }

            lv_group_add_obj(ui_group, ui_lblTaskItem);
            lv_group_add_obj(ui_group_glyph, ui_lblTaskItemGlyph);
            lv_group_add_obj(ui_group_panel, ui_pnlTask);

            apply_size_to_content(&theme, ui_pnlContent, ui_lblTaskItem, ui_lblTaskItemGlyph, task_store);
            apply_text_long_dot(&theme, ui_pnlContent, ui_lblTaskItem, TS(task_store));
        }

        free(base_filename);
    }

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);

    free(file_names);
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

    play_sound("confirm", nav_sound, 0, 1);

    static char task_script[MAX_BUFFER_SIZE];
    snprintf(task_script, sizeof(task_script), "%s/%s/%s.sh",
             device.STORAGE.ROM.MOUNT, MUOS_TASK_PATH, items[current_item_index].name);

    const char *args[] = {
            (INTERNAL_PATH "bin/fbpad"),
            "-bg", theme.TERMINAL.BACKGROUND,
            "-fg", theme.TERMINAL.FOREGROUND,
            task_script,
            NULL
    };

    setenv("TERM", "xterm-256color", 1);

    if (config.VISUAL.BLACKFADE) {
        fade_to_black(ui_screen);
    } else {
        unload_image_animation();
    }

    run_exec(args);

    write_text_to_file(MUOS_TIN_LOAD, "w", INT, current_item_index);

    load_mux("task");

    safe_quit(0);
    mux_input_stop();
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

    safe_quit(0);
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

    if (bar_footer) lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, "");

    lv_label_set_text(ui_lblNavA, lang.GENERIC.LAUNCH);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);

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

    if (!ui_count) {
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

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

void update_footer_nav_elements() {
    if (!ui_count) {
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
}

void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
            lv_obj_set_user_data(element_focused, items[current_item_index].name);

            adjust_wallpaper_element(ui_group, 0, TASK);
        }
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    (void) argc;

    mux_module = basename(argv[0]);
    setup_background_process();

    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    init_theme(1, 1);
    init_display();

    init_ui_common_screen(&theme, &device, &lang, lang.MUXTASK.TITLE);
    init_timer(ui_refresh_task, NULL);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());
    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, TASK);

    init_fonts();
    create_task_items();
    init_navigation_sound(&nav_sound, mux_module);

    int tin_index = 0;
    if (file_exist(MUOS_TIN_LOAD)) {
        tin_index = read_int_from_file(MUOS_TIN_LOAD, 1);
        remove(MUOS_TIN_LOAD);
    }

    lv_obj_set_user_data(lv_group_get_focused(ui_group), items[current_item_index].name);

    int nav_hidden = 0;
    if (ui_count > 0) {
        nav_hidden = 1;
        if (tin_index > -1 && tin_index <= ui_count && current_item_index < ui_count) list_nav_next(tin_index);
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXTASK.NONE);
    }

    struct nav_flag nav_e[] = {
            {ui_lblNavA,      nav_hidden},
            {ui_lblNavAGlyph, nav_hidden}
    };
    set_nav_flags(nav_e, sizeof(nav_e) / sizeof(nav_e[0]));

    load_kiosk(&kiosk);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
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

    free_items(items, item_count);

    return 0;
}
