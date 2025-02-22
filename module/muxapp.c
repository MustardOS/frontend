#include "../lvgl/lvgl.h"
#include <unistd.h>
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
#include "../common/input.h"
#include "../common/input/list_nav.h"

char *mux_module;

static int joy_general;
static int joy_power;
static int joy_volume;
static int joy_extra;

int turbo_mode = 0;
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

void create_app_items();

void list_nav_prev(int steps);

void list_nav_next(int steps);

void init_elements();

void update_footer_nav_elements();

void ui_refresh_task();

void show_help() {
    char *title = items[current_item_index].name;

    char help_info[MAX_BUFFER_SIZE];
    snprintf(help_info, sizeof(help_info), "%s/%s/%s.sh",
             device.STORAGE.ROM.MOUNT, MUOS_APPS_PATH, title);
    char *message = get_script_value(help_info, "HELP");

    if (strlen(message) <= 1) message = lang.GENERIC.NO_HELP;
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent, TS(title), TS(message));
}

void init_navigation_group_grid(const char *app_path) {
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

        char *glyph_name = get_var_from_file(app_path, items[i].extra_data, "ICON", "app");
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

void create_app_items() {
    char app_path[MAX_BUFFER_SIZE];
    snprintf(app_path, sizeof(app_path),
             "%s/%s", device.STORAGE.ROM.MOUNT, MUOS_APPS_PATH);

    const char *app_directories[] = {
            app_path
    };
    char app_dir[MAX_BUFFER_SIZE];

    char **file_names = NULL;
    size_t file_count = 0;

    for (size_t dir_index = 0; dir_index < sizeof(app_directories) / sizeof(app_directories[0]); ++dir_index) {
        snprintf(app_dir, sizeof(app_dir), "%s/", app_directories[dir_index]);

        DIR *ad = opendir(app_dir);
        if (!ad) continue;

        struct dirent *af;
        while ((af = readdir(ad))) {
            if (af->d_type == DT_REG) {
                char *last_dot = strrchr(af->d_name, '.');
                if (last_dot && strcasecmp(last_dot, ".sh") == 0) {
                    char **temp = realloc(file_names, (file_count + 1) * sizeof(char *));
                    if (!temp) {
                        perror(lang.SYSTEM.FAIL_ALLOCATE_MEM);
                        free(file_names);
                        closedir(ad);
                        return;
                    }
                    file_names = temp;

                    char full_app_name[MAX_BUFFER_SIZE];
                    snprintf(full_app_name, sizeof(full_app_name), "%s%s", app_dir, af->d_name);
                    file_names[file_count] = strdup(full_app_name);
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

        char *standard_app_name = strip_ext(str_remchar(str_replace(base_filename, strip_dir(base_filename), ""), '/'));
        char *app_name_for_grid = get_var_from_file(app_path, base_filename, "GRID", standard_app_name);

        char app_store[MAX_BUFFER_SIZE];
        snprintf(app_store, sizeof(app_store), "%s", theme.GRID.ENABLED ? app_name_for_grid : standard_app_name);

        add_item(&items, &item_count, app_store, TS(app_store), file_names[i], ROM);

        free(base_filename);
    }

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
                    apply_theme_list_glyph(&theme, ui_lblAppItemGlyph, mux_module,
                                           get_var_from_file(app_path, items[i].extra_data, "ICON", "app"));
                }

                lv_group_add_obj(ui_group, ui_lblAppItem);
                lv_group_add_obj(ui_group_glyph, ui_lblAppItemGlyph);
                lv_group_add_obj(ui_group_panel, ui_pnlApp);

                apply_size_to_content(&theme, ui_pnlContent, ui_lblAppItem, ui_lblAppItemGlyph, items[i].name);
                apply_text_long_dot(&theme, ui_pnlContent, ui_lblAppItem, items[i].name);
                ui_count++;
            }
        }
    }

    if (ui_count > 0) {
        theme.GRID.ENABLED ? lv_obj_update_layout(ui_pnlGrid) : lv_obj_update_layout(ui_pnlContent);
    }

    free(file_names);
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group),
                            items[current_item_index].display_name);
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    if (grid_mode_enabled) {
        update_grid_scroll_position(theme.GRID.COLUMN_COUNT, theme.GRID.ROW_COUNT, theme.GRID.ROW_HEIGHT,
                                    current_item_index, ui_pnlGrid);
    } else {
        update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    }
    set_label_long_mode(&theme, lv_group_get_focused(ui_group), items[current_item_index].display_name);
    lv_label_set_text(ui_lblGridCurrentItem, items[current_item_index].display_name);
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
                            items[current_item_index].display_name);
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    if (grid_mode_enabled) {
        update_grid_scroll_position(theme.GRID.COLUMN_COUNT, theme.GRID.ROW_COUNT, theme.GRID.ROW_HEIGHT,
                                    current_item_index, ui_pnlGrid);
    } else {
        update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    }
    set_label_long_mode(&theme, lv_group_get_focused(ui_group), items[current_item_index].display_name);
    lv_label_set_text(ui_lblGridCurrentItem, items[current_item_index].display_name);
    nav_moved = 1;
}

void handle_a() {
    if (msgbox_active) return;

    if (ui_count > 0) {
        struct {
            const char *app_name;
            int16_t *kiosk_flag;
        } elements[] = {
                {lang.MUXAPP.ARCHIVE, &kiosk.APPLICATION.ARCHIVE},
                {lang.MUXAPP.TASK,    &kiosk.APPLICATION.TASK}
        };

        for (size_t i = 0; i < sizeof(elements) / sizeof(elements[0]); i++) {
            if (strcasecmp(items[current_item_index].name, elements[i].app_name) == 0) {
                if (*(elements[i].kiosk_flag)) {
                    toast_message(kiosk_nope(), 1000, 1000);
                    return;
                }
            }
        }

        play_sound("confirm", nav_sound, 0, 1);
        toast_message(lang.MUXAPP.LOAD_APP, 0, 0);

        write_text_to_file(MUOS_APP_LOAD, "w", CHAR, items[current_item_index].extra_data);

        write_text_to_file(MUOS_AIN_LOAD, "w", INT, current_item_index);

        mux_input_stop();
    }
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
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "apps");
    mux_input_stop();
}

void handle_menu() {
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

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image, theme.MISC.IMAGE_OVERLAY);
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

            adjust_wallpaper_element(ui_group, 0, APPLICATION);
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

    init_ui_common_screen(&theme, &device, &lang, lang.MUXAPP.TITLE);
    init_timer(ui_refresh_task, NULL);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    init_fonts();
    create_app_items();
    update_footer_nav_elements();
    init_navigation_sound(&nav_sound, mux_module);

    int ain_index = 0;
    if (file_exist(MUOS_AIN_LOAD)) {
        ain_index = read_int_from_file(MUOS_AIN_LOAD, 1);
        remove(MUOS_AIN_LOAD);
    }

    lv_obj_set_user_data(lv_group_get_focused(ui_group), items[current_item_index].name);
    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, APPLICATION);

    init_input(&joy_general, &joy_power, &joy_volume, &joy_extra);

    if (ui_count > 0) {
        if (ain_index > -1 && ain_index <= ui_count && current_item_index < ui_count) list_nav_next(ain_index);
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXAPP.NO_APP);
    }

    load_kiosk(&kiosk);

    mux_input_options input_opts = {
            .general_fd = joy_general,
            .power_fd = joy_power,
            .volume_fd = joy_volume,
            .extra_fd = joy_extra,
            .max_idle_ms = IDLE_MS,
            .swap_btn = config.SETTINGS.ADVANCED.SWAP,
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1 ||
                          (grid_mode_enabled && theme.GRID.NAVIGATION_TYPE >= 1 && theme.GRID.NAVIGATION_TYPE <= 5)),
            .stick_nav = true,
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
            },
            .combo = {
                    COMBO_BRIGHT(BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_UP)),
                    COMBO_BRIGHT(BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_DOWN)),
                    COMBO_VOLUME(BIT(MUX_INPUT_VOL_UP)),
                    COMBO_VOLUME(BIT(MUX_INPUT_VOL_DOWN)),
            },
            .idle_handler = ui_common_handle_idle,
    };
    mux_input_task(&input_opts);
    safe_quit(0);

    free_items(items, item_count);

    close(joy_general);
    close(joy_power);
    close(joy_volume);
    close(joy_extra);

    return 0;
}
