#include "muxshare.h"
#include "muxapp.h"
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/input/list_nav.h"

#define UI_PANEL 5
static lv_obj_t *ui_mux_panels[UI_PANEL];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

static void create_app_items();

static void update_footer_nav_elements();

static void show_help() {
    char *title = items[current_item_index].name;

    char help_info[MAX_BUFFER_SIZE];
    snprintf(help_info, sizeof(help_info), "%s/%s/%s.sh",
             device.STORAGE.ROM.MOUNT, MUOS_APPS_PATH, title);

    char *message = get_script_value(help_info, "HELP", lang.GENERIC.NO_HELP);
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent, TS(title), TS(message));
}

static void init_navigation_group_grid(const char *app_path) {
    grid_mode_enabled = 1;

    init_grid_info((int) item_count, theme.GRID.COLUMN_COUNT);
    create_grid_panel(&theme, (int) item_count);

    load_font_section(FONT_PANEL_FOLDER, ui_pnlGrid);
    load_font_section(FONT_PANEL_FOLDER, ui_lblGridCurrentItem);

    for (size_t i = 0; i < item_count; i++) {
        uint8_t col = i % theme.GRID.COLUMN_COUNT;
        uint8_t row = i / theme.GRID.COLUMN_COUNT;

        lv_obj_t *cell_panel = lv_obj_create(ui_pnlGrid);
        lv_obj_t *cell_image = lv_img_create(cell_panel);
        lv_obj_t *cell_label = lv_label_create(cell_panel);

        char app_launcher[MAX_BUFFER_SIZE];
        snprintf(app_launcher, sizeof(app_launcher), "%s/%s/" APP_LAUNCHER, app_path, items[i].extra_data);

        char *glyph_name = get_script_value(app_launcher, "ICON", "app");

        char mux_dimension[15];
        get_mux_dimension(mux_dimension, sizeof(mux_dimension));

        char grid_image[MAX_BUFFER_SIZE];
        load_image_catalogue("Application", glyph_name, "default", mux_dimension, "grid",
                             grid_image, sizeof(grid_image));

        char glyph_name_focused[MAX_BUFFER_SIZE];
        snprintf(glyph_name_focused, sizeof(glyph_name_focused), "%s_focused", glyph_name);

        char grid_image_focused[MAX_BUFFER_SIZE];
        load_image_catalogue("Application", glyph_name_focused, "default_focused", mux_dimension, "grid",
                             grid_image_focused, sizeof(grid_image_focused));

        create_grid_item(&theme, cell_panel, cell_label, cell_image, col, row,
                         grid_image, grid_image_focused, items[i].display_name);

        lv_group_add_obj(ui_group, cell_label);
        lv_group_add_obj(ui_group_glyph, cell_image);
        lv_group_add_obj(ui_group_panel, cell_panel);
    }
}

static void create_app_items() {
    char app_path[MAX_BUFFER_SIZE];
    snprintf(app_path, sizeof(app_path), "%s/%s", device.STORAGE.ROM.MOUNT, MUOS_APPS_PATH);

    DIR *app_dir = opendir(app_path);
    if (!app_dir) return;

    struct dirent *entry;
    char **dir_names = NULL;
    size_t dir_count = 0;

    while ((entry = readdir(app_dir))) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char launch_script[MAX_BUFFER_SIZE];
            snprintf(launch_script, sizeof(launch_script), "%s/%s/" APP_LAUNCHER, app_path, entry->d_name);

            if (access(launch_script, F_OK) == 0) {
                char **temp = realloc(dir_names, (dir_count + 1) * sizeof(char *));
                if (!temp) {
                    perror(lang.SYSTEM.FAIL_ALLOCATE_MEM);
                    free(dir_names);
                    closedir(app_dir);
                    return;
                }
                dir_names = temp;
                dir_names[dir_count] = strdup(entry->d_name);
                if (!dir_names[dir_count]) {
                    perror(lang.SYSTEM.FAIL_DUP_STRING);
                    free(dir_names);
                    closedir(app_dir);
                    return;
                }
                dir_count++;
            }
        }
    }
    closedir(app_dir);

    if (!dir_names) return;
    qsort(dir_names, dir_count, sizeof(char *), str_compare);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; i < dir_count; i++) {
        if (!dir_names[i]) continue;

        char app_launcher[MAX_BUFFER_SIZE];
        snprintf(app_launcher, sizeof(app_launcher), "%s/%s/" APP_LAUNCHER, app_path, dir_names[i]);
        char *app_name_for_grid = get_script_value(app_launcher, "GRID", dir_names[i]);

        char app_store[MAX_BUFFER_SIZE];
        snprintf(app_store, sizeof(app_store), "%s", theme.GRID.ENABLED ? app_name_for_grid : dir_names[i]);

        add_item(&items, &item_count, app_store, TS(app_store), dir_names[i], ITEM);

        free(dir_names[i]);
    }

    free(dir_names);

    if (theme.GRID.ENABLED && item_count > 0) {
        init_navigation_group_grid(app_path);
        ui_count += (int) item_count;
    } else {
        for (size_t i = 0; i < item_count; i++) {
            lv_obj_t *ui_pnlApp = lv_obj_create(ui_pnlContent);
            if (ui_pnlApp) {
                apply_theme_list_panel(ui_pnlApp);

                lv_obj_t *ui_lblAppItem = lv_label_create(ui_pnlApp);
                if (ui_lblAppItem) apply_theme_list_item(&theme, ui_lblAppItem, TS(items[i].name));

                lv_obj_t *ui_lblAppItemGlyph = lv_img_create(ui_pnlApp);
                if (ui_lblAppItemGlyph) {
                    char app_launcher[MAX_BUFFER_SIZE];
                    snprintf(app_launcher, sizeof(app_launcher), "%s/%s/" APP_LAUNCHER, app_path, items[i].name);

                    apply_theme_list_glyph(&theme, ui_lblAppItemGlyph, mux_module,
                                           get_script_value(app_launcher, "ICON", "app"));
                }

                lv_group_add_obj(ui_group, ui_lblAppItem);
                lv_group_add_obj(ui_group_glyph, ui_lblAppItemGlyph);
                lv_group_add_obj(ui_group_panel, ui_pnlApp);

                apply_size_to_content(&theme, ui_pnlContent, ui_lblAppItem, ui_lblAppItemGlyph, TS(items[i].name));
                apply_text_long_dot(&theme, ui_pnlContent, ui_lblAppItem, TS(items[i].name));
                ui_count++;
            }
        }
    }

    if (ui_count > 0) {
        theme.GRID.ENABLED ? lv_obj_update_layout(ui_pnlGrid) : lv_obj_update_layout(ui_pnlContent);
    }
}

static void list_nav_move(int steps, int direction) {
    if (ui_count <= 0) return;
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE, 0);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            items[current_item_index].display_name);

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    if (grid_mode_enabled) {
        update_grid_scroll_position(theme.GRID.COLUMN_COUNT, theme.GRID.ROW_COUNT, theme.GRID.ROW_HEIGHT,
                                    current_item_index, ui_pnlGrid);
    } else {
        update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL,
                               ui_count, current_item_index, ui_pnlContent);
    }

    set_label_long_mode(&theme, lv_group_get_focused(ui_group), items[current_item_index].display_name);
    lv_label_set_text(ui_lblGridCurrentItem, items[current_item_index].display_name);

    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a() {
    if (msgbox_active) return;

    if (ui_count > 0) {
        struct {
            const char *app_name;
            int16_t *kiosk_flag;
        } elements[] = {
                {lang.MUXAPP.ARCHIVE, &kiosk.APPLICATION.ARCHIVE},
                {lang.MUXAPP.TASK,    &kiosk.APPLICATION.TASK}
        };

        int skip_toast = 0;

        for (size_t i = 0; i < sizeof(elements) / sizeof(elements[0]); i++) {
            if (strcasecmp(items[current_item_index].name, elements[i].app_name) == 0) {
                if (*(elements[i].kiosk_flag)) {
                    play_sound(SND_ERROR, 0);
                    toast_message(kiosk_nope(), 1000, 1000);
                    refresh_screen(ui_screen);

                    return;
                }

                skip_toast = 1;
            }
        }

        play_sound(SND_CONFIRM, 0);

        if (!skip_toast) {
            toast_message(lang.MUXAPP.LOAD_APP, 0, 0);
            refresh_screen(ui_screen);
        }

        write_text_to_file(MUOS_APP_LOAD, "w", CHAR, items[current_item_index].extra_data);
        write_text_to_file(MUOS_AIN_LOAD, "w", INT, current_item_index);

        close_input();
        mux_input_stop();
    }
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
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "apps");

    close_input();
    mux_input_stop();
}

static void handle_menu() {
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

    lv_label_set_text(ui_lblNavA, lang.GENERIC.LAUNCH);
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

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

static void update_footer_nav_elements() {
    if (!ui_count) {
        lv_obj_add_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    } else {
        lv_obj_clear_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    }
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
            lv_obj_set_user_data(element_focused, items[current_item_index].name);

            adjust_wallpaper_element(ui_group, 0, APPLICATION);
        }
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxapp_main() {
    init_module("muxapp");

    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXAPP.TITLE);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    init_fonts();
    create_app_items();
    update_footer_nav_elements();

    int ain_index = 0;
    if (file_exist(MUOS_AIN_LOAD)) {
        ain_index = read_line_int_from(MUOS_AIN_LOAD, 1);
        remove(MUOS_AIN_LOAD);
    }

    lv_obj_set_user_data(lv_group_get_focused(ui_group), items[current_item_index].name);
    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, APPLICATION);

    if (ui_count > 0) {
        if (ain_index > -1 && ain_index <= ui_count && current_item_index < ui_count) list_nav_move(ain_index, +1);
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXAPP.NO_APP);
    }

    load_kiosk(&kiosk);

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1 ||
                          (grid_mode_enabled && theme.GRID.NAVIGATION_TYPE >= 1 && theme.GRID.NAVIGATION_TYPE <= 5)),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_list_nav_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_list_nav_right,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_DPAD_LEFT] = handle_list_nav_left_hold,
                    [MUX_INPUT_DPAD_RIGHT] = handle_list_nav_right_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    free_items(&items, &item_count);

    return 0;
}
