#include "../lvgl/lvgl.h"
#include "../lvgl/src/drivers/fbdev.h"
#include "../lvgl/src/drivers/evdev.h"
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/common.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/collection.h"
#include "../common/config.h"
#include "../common/device.h"
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
struct theme_config theme;

int nav_moved = 1;
lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;

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

void glyph_task();

void ui_refresh_task();

void show_help() {
    char *title = items[current_item_index].name;

    char help_info[MAX_BUFFER_SIZE];
    snprintf(help_info, sizeof(help_info),
             "%s/MUOS/application/%s.sh", device.STORAGE.ROM.MOUNT, title);
    char *message = get_script_value(help_info, "HELP");

    if (strlen(message) <= 1) message = TG("No Help Information Found");
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent, TS(title), TS(message));
}

void init_navigation_groups_grid(const char *app_path) {
    grid_mode_enabled = 1;
    init_grid_info((int) item_count, theme.GRID.COLUMN_COUNT);
    create_grid_panel(&theme, (int) item_count);
    load_font_section(mux_module, FONT_PANEL_FOLDER, ui_pnlGrid);
    for (size_t i = 0; i < item_count; i++) {
        uint8_t col = i % theme.GRID.COLUMN_COUNT;
        uint8_t row = i / theme.GRID.COLUMN_COUNT;

        lv_obj_t *cell_panel = lv_obj_create(ui_pnlGrid);
        lv_obj_t *cell_image = lv_img_create(cell_panel);
        lv_obj_t *cell_label = lv_label_create(cell_panel);

        char *glyph_name = get_glyph_from_file(app_path, items[i].name, "app");
        char device_dimension[15];
        get_device_dimension(device_dimension, sizeof(device_dimension));
        char grid_image[MAX_BUFFER_SIZE];
        if (!load_element_image_specifics(STORAGE_THEME, device_dimension, mux_module, "grid",
                                          glyph_name, "png", grid_image, sizeof(grid_image)) &&
            !load_element_image_specifics(STORAGE_THEME, device_dimension, mux_module, "grid",
                                          "app", "png", grid_image, sizeof(grid_image)) &&
            !load_element_image_specifics(STORAGE_THEME, device_dimension, mux_module, "grid",
                                          "default", "png", grid_image, sizeof(grid_image)) &&
            !load_element_image_specifics(STORAGE_THEME, "", mux_module, "grid",
                                          glyph_name, "png", grid_image, sizeof(grid_image)) &&
            !load_element_image_specifics(STORAGE_THEME, "", mux_module, "grid",
                                         "app", "png", grid_image, sizeof(grid_image))) {
            load_element_image_specifics(STORAGE_THEME, "", mux_module, "grid",
                                         "default", "png", grid_image, sizeof(grid_image));
        }

        char glyph_name_focused[MAX_BUFFER_SIZE];
        snprintf(glyph_name_focused, sizeof(glyph_name_focused), "%s_focused", glyph_name);                
        char grid_image_focused[MAX_BUFFER_SIZE];
        if (!load_element_image_specifics(STORAGE_THEME, device_dimension, mux_module, "grid",
                                          glyph_name_focused, "png", grid_image_focused, sizeof(grid_image_focused)) &&
            !load_element_image_specifics(STORAGE_THEME, device_dimension, mux_module, "grid",
                                          "app_focused", "png", grid_image_focused, sizeof(grid_image_focused)) &&
            !load_element_image_specifics(STORAGE_THEME, device_dimension, mux_module, "grid",
                                          "default_focused", "png", grid_image_focused, sizeof(grid_image_focused)) &&
            !load_element_image_specifics(STORAGE_THEME, "", mux_module, "grid",
                                          glyph_name_focused, "png", grid_image_focused, sizeof(grid_image_focused)) &&
            !load_element_image_specifics(STORAGE_THEME, "", mux_module, "grid",
                                         "app_focused", "png", grid_image_focused, sizeof(grid_image_focused))) {
            load_element_image_specifics(STORAGE_THEME, "", mux_module, "grid",
                                         "default_focused", "png", grid_image_focused, sizeof(grid_image_focused));
        }

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
             "%s/MUOS/application", device.STORAGE.ROM.MOUNT);

    const char *app_directories[] = {
            app_path
    };
    char app_dir[MAX_BUFFER_SIZE];

    char **file_names = NULL;
    size_t file_count = 0;

    for (size_t dir_index = 0; dir_index < sizeof(app_directories) / sizeof(app_directories[0]); ++dir_index) {
        snprintf(app_dir, sizeof(app_dir), "%s/", app_directories[dir_index]);

        DIR *ad = opendir(app_dir);
        if (ad == NULL) continue;

        struct dirent *af;
        while ((af = readdir(ad))) {
            if (af->d_type == DT_REG) {
                char *last_dot = strrchr(af->d_name, '.');
                if (last_dot != NULL && strcasecmp(last_dot, ".sh") == 0) {
                    char **temp = realloc(file_names, (file_count + 1) * sizeof(char *));
                    if (temp == NULL) {
                        perror("Failed to allocate memory");
                        free(file_names);
                        closedir(ad);
                        return;
                    }
                    file_names = temp;

                    char full_app_name[MAX_BUFFER_SIZE];
                    snprintf(full_app_name, sizeof(full_app_name), "%s%s", app_dir, af->d_name);
                    file_names[file_count] = strdup(full_app_name);
                    if (file_names[file_count] == NULL) {
                        perror("Failed to duplicate string");
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

    qsort(file_names, file_count, sizeof(char *), str_compare);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; i < file_count; i++) {
        char *base_filename = file_names[i];

        char app_name[MAX_BUFFER_SIZE];
        snprintf(app_name, sizeof(app_name), "%s",
                 str_remchar(str_replace(base_filename, strip_dir(base_filename), ""), '/'));

        char app_store[MAX_BUFFER_SIZE];
        snprintf(app_store, sizeof(app_store), "%s", strip_ext(app_name));

        add_item(&items, &item_count, app_store, TS(app_store), file_names[i], ROM);

        free(base_filename);
    }

    if (theme.GRID.ENABLED && item_count > 0) {
        init_navigation_groups_grid(app_path);
        ui_count += (int) item_count;
    } else {
        for (size_t i = 0; i < item_count; i++) {
            lv_obj_t *ui_pnlApp = lv_obj_create(ui_pnlContent);
            if (ui_pnlApp) {
                apply_theme_list_panel(&theme, &device, ui_pnlApp);

                lv_obj_t *ui_lblAppItem = lv_label_create(ui_pnlApp);
                if (ui_lblAppItem) apply_theme_list_item(&theme, ui_lblAppItem, TS(items[i].name), true, false);

                lv_obj_t *ui_lblAppItemGlyph = lv_img_create(ui_pnlApp);
                if (ui_lblAppItemGlyph) {
                    apply_theme_list_glyph(&theme, ui_lblAppItemGlyph, mux_module,
                                           get_glyph_from_file(app_path, items[i].name, "app"));
                }

                lv_group_add_obj(ui_group, ui_lblAppItem);
                lv_group_add_obj(ui_group_glyph, ui_lblAppItemGlyph);
                lv_group_add_obj(ui_group_panel, ui_pnlApp);

                apply_size_to_content(&theme, ui_pnlContent, ui_lblAppItem, ui_lblAppItemGlyph, items[i].name);
                apply_text_long_dot(&theme, ui_pnlContent, ui_lblAppItem, items[i].name);;
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
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group), items[current_item_index].display_name);
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
    nav_moved = 1;
}

void list_nav_next(int steps) {
    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound, 0, 0);
    }
    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group), items[current_item_index].display_name);
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
    nav_moved = 1;
}

void handle_a() {
    if (msgbox_active) return;

    if (ui_count > 0) {
        play_sound("confirm", nav_sound, 0, 1);

        lv_label_set_text(ui_lblMessage, TS("Loading Application"));
        lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);

        static char command[MAX_BUFFER_SIZE];
        snprintf(command, sizeof(command), "%s/MUOS/application/%s.sh",
                 device.STORAGE.ROM.MOUNT, items[current_item_index].name);
        write_text_to_file(MUOS_APP_LOAD, "w", CHAR, command);

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

    lv_label_set_text(ui_lblNavA, TG("Launch"));
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

    if (TEST_IMAGE) display_testing_message(ui_screen);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image, theme.MISC.IMAGE_OVERLAY);
}

void update_footer_nav_elements() {
    if (ui_count == 0) {
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
    load_device(&device);

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

    ui_common_screen_init(&theme, &device, TS("APPLICATIONS"));
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);

    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_font_text(basename(argv[0]), ui_screen);
    load_font_section(basename(argv[0]), FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(mux_module, FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(mux_module, FONT_FOOTER_FOLDER, ui_pnlFooter);

    create_app_items();
    update_footer_nav_elements();

    int ain_index = 0;
    if (file_exist(MUOS_AIN_LOAD)) {
        ain_index = read_int_from_file(MUOS_AIN_LOAD, 1);
        printf("loading AIN at: %d\n", ain_index);
        remove(MUOS_AIN_LOAD);
    }

    if (ui_count > 0) {
        if (ain_index > -1 && ain_index <= ui_count && current_item_index < ui_count) {
            list_nav_next(ain_index);
        }
    } else {
        lv_label_set_text(ui_lblScreenMessage, TS("No Applications Found"));
        lv_obj_clear_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_set_user_data(lv_group_get_focused(ui_group), items[current_item_index].name);

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND,
                   theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND, APPLICATION);

    nav_sound = init_nav_sound(mux_module);
    struct dt_task_param dt_par;
    struct bat_task_param bat_par;

    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;

    js_fd = open(device.INPUT.EV1, O_RDONLY);
    if (js_fd < 0) {
        perror("Failed to open joystick device");
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

    lv_timer_t *glyph_timer = lv_timer_create(glyph_task, UINT16_MAX / 64, NULL);
    lv_timer_ready(glyph_timer);

    lv_timer_t *ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
    lv_timer_ready(ui_refresh_timer);

    refresh_screen(device.SCREEN.WAIT);

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = 16 /* ~60 FPS */,
            .swap_btn = config.SETTINGS.ADVANCED.SWAP,
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1 ||
                          (grid_mode_enabled && theme.GRID.NAVIGATION_TYPE >= 1 && theme.GRID.NAVIGATION_TYPE <= 5)),
            .stick_nav = true,
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    // List navigation:
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_list_nav_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_list_nav_right,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    // List navigation:
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_DPAD_LEFT] = handle_list_nav_left_hold,
                    [MUX_INPUT_DPAD_RIGHT] = handle_list_nav_right_hold,
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

    free_items(items, item_count);
    close(js_fd);
    close(js_fd_sys);

    return 0;
}
