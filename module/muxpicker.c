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
#include "../common/img/nothing.h"
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
int progress_onscreen = -1;
int ui_count = 0;
int current_item_index = 0;
int first_open = 1;

char *picker_type;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;

size_t item_count = 0;
content_item *items = NULL;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

lv_obj_t *ui_mux_panels[5];

void show_help() {
    char *picker_name = lv_label_get_text(lv_group_get_focused(ui_group));
    char picker_archive[MAX_BUFFER_SIZE];

    snprintf(picker_archive, sizeof(picker_archive), "%s/%s/%s.zip", STORAGE_PATH, picker_type, picker_name);

    char credits[MAX_BUFFER_SIZE];
    if (extract_file_from_zip(picker_archive, "credits.txt", "/tmp/credits.txt")) {
        strcpy(credits, TS("There are no attributed credits!"));
    } else {
        strcpy(credits, read_text_from_file("/tmp/credits.txt"));
    }

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(lv_group_get_focused(ui_group))), TS(credits));
}

void reset_label_long_mode() {
    apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group), items[current_item_index].display_name);
}

void set_label_long_mode() {
    char *content_label = lv_label_get_text(lv_group_get_focused(ui_group));

    size_t len = strlen(content_label);
    bool ends_with_ellipse = len > 3 && strcmp(&content_label[len - 3], "…") == 0;

    if (strcasecmp(items[current_item_index].display_name, content_label) != 0 && ends_with_ellipse) {
        lv_label_set_long_mode(lv_group_get_focused(ui_group), LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_label_set_text(lv_group_get_focused(ui_group), items[current_item_index].display_name);
    }
}

void image_refresh() {
    // Invalidate the cache for this image path
    lv_img_cache_invalidate_src(lv_img_get_src(ui_imgBox));

    char *picker_name = lv_label_get_text(lv_group_get_focused(ui_group));
    char picker_archive[MAX_BUFFER_SIZE];

    snprintf(picker_archive, sizeof(picker_archive), "%s/%s/%s.zip", STORAGE_PATH, picker_type, picker_name);

    char device_dimension[15];
    get_device_dimension(device_dimension, sizeof(device_dimension));
    char device_preview[PATH_MAX];
    snprintf(device_preview, sizeof(device_preview), "%spreview.png", device_dimension);

    if (extract_file_from_zip(picker_archive, device_preview, "/tmp/preview.png") &&
        extract_file_from_zip(picker_archive, "preview.png", "/tmp/preview.png")) {
        lv_img_set_src(ui_imgBox, &ui_image_Nothing);
        return;
    }

    lv_img_set_src(ui_imgBox, "M:/tmp/preview.png");
}

void create_picker_items() {
    DIR *td;
    struct dirent *tf;

    char picker_dir[PATH_MAX];
    snprintf(picker_dir, sizeof(picker_dir), "%s/%s", STORAGE_PATH, picker_type);

    td = opendir(picker_dir);
    if (td == NULL) {
        lv_obj_clear_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    while ((tf = readdir(td))) {
        if (tf->d_type == DT_REG) {
            char filename[FILENAME_MAX];
            snprintf(filename, sizeof(filename), "%s/%s", picker_dir, tf->d_name);

            char *last_dot = strrchr(tf->d_name, '.');
            if (last_dot != NULL && strcasecmp(last_dot, ".zip") == 0) {
                *last_dot = '\0';

                add_item(&items, &item_count, tf->d_name, tf->d_name, "", ROM);
            }
        }
    }

    closedir(td);
    sort_items(items, item_count);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; i < item_count; i++) {
        ui_count++;

        lv_obj_t *ui_pnlPicker = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(&theme, &device, ui_pnlPicker);

        lv_obj_t *ui_lblPickerItem = lv_label_create(ui_pnlPicker);
        apply_theme_list_item(&theme, ui_lblPickerItem, items[i].display_name, true, false);

        lv_obj_t *ui_lblPickerItemGlyph = lv_img_create(ui_pnlPicker);
        apply_theme_list_glyph(&theme, ui_lblPickerItemGlyph, mux_module, get_last_subdir(picker_type, '/', 1));

        lv_group_add_obj(ui_group, ui_lblPickerItem);
        lv_group_add_obj(ui_group_glyph, ui_lblPickerItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlPicker);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblPickerItem, ui_lblPickerItemGlyph, items[i].display_name);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblPickerItem, items[i].display_name);
    }
    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
}

void list_nav_prev(int steps) {
    if (ui_count <= 0) return;

    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        reset_label_long_mode();
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    image_refresh();
    set_label_long_mode();
    nav_moved = 1;
}

void list_nav_next(int steps) {
    if (ui_count <= 0) return;

    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound, 0, 0);
    }
    for (int step = 0; step < steps; ++step) {
        reset_label_long_mode();
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    image_refresh();
    set_label_long_mode();
    nav_moved = 1;
}

void handle_confirm() {
    if (msgbox_active || ui_count <= 0) return;

    play_sound("confirm", nav_sound, 0, 1);
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);

    static char picker_script[MAX_BUFFER_SIZE];
    snprintf(picker_script, sizeof(picker_script),
             "%s/script/package/%s.sh", INTERNAL_PATH, get_last_subdir(picker_type, '/', 1));

    static char command[MAX_BUFFER_SIZE];
    snprintf(command, sizeof(command), "/opt/muos/bin/fbpad %s \"%s\"",
             picker_script, lv_label_get_text(lv_group_get_focused(ui_group)));
    setenv("TERM", "xterm-256color", 1);
    printf("RUNNING: %s\n", command);

    if (config.VISUAL.BLACKFADE) {
        fade_to_black(ui_screen);
    } else {
        unload_image_animation();
    }

    system(command);

    write_text_to_file(MUOS_PIN_LOAD, "w", INT, current_item_index);

    load_mux("picker");
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
    load_mux("custom");
    mux_input_stop();
}

void handle_help() {
    if (msgbox_active) return;

    if (progress_onscreen == -1 && ui_count > 0) {
        play_sound("confirm", nav_sound, 0, 0);
        show_help();
    }
}

void init_elements() {
    lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);

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

    if (TEST_IMAGE) display_testing_message(ui_screen);

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

    if (ui_count > 0 && nav_moved) {
        image_refresh();

        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_invalidate(ui_pnlBox);
        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    mux_module = basename(argv[0]);
    load_device(&device);

    char *cmd_help = "\nmuOS Extras - Custom Picker\nUsage: %s <-m>\n\nOptions:\n"
                     "\t-m Picker module from:\n"
                     "\t\ttheme\n" // This might need to be changed later?
                     "\t\tpackage/catalogue\n"
                     "\t\tpackage/config\n\n";

    int opt;
    while ((opt = getopt(argc, argv, "m:")) != -1) {
        if (opt == 'm') {
            picker_type = optarg;
        } else {
            fprintf(stderr, cmd_help, argv[0]);
            return 1;

        }
    }

    if (picker_type == NULL) {
        fprintf(stderr, cmd_help, argv[0]);
        return 1;
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

    config.VISUAL.BOX_ART = 1;  //Force correct panel size for displaying preview in bottom right

    if (strcasecmp(picker_type, "theme") == 0) {
        ui_common_screen_init(&theme, &device, TS("THEME PICKER"));
        lv_label_set_text(ui_lblScreenMessage, TS("No Theme Packages Found"));
    } else if (strcasecmp(picker_type, "package/catalogue") == 0) {
        ui_common_screen_init(&theme, &device, TS("CATALOGUE PICKER"));
        lv_label_set_text(ui_lblScreenMessage, TS("No Catalogue Packages Found"));
    } else if (strcasecmp(picker_type, "package/config") == 0) {
        ui_common_screen_init(&theme, &device, TS("CONFIG PICKER"));
        lv_label_set_text(ui_lblScreenMessage, TS("No Configuration Packages Found"));
    } else {
        ui_common_screen_init(&theme, &device, TS("CUSTOM PICKER"));
        lv_label_set_text(ui_lblScreenMessage, TS("No Custom Packages Found"));
    }

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

    create_picker_items();

    int sys_index = 0;
    if (file_exist(MUOS_PIN_LOAD)) {
        sys_index = read_int_from_file(MUOS_PIN_LOAD, 1);
        remove(MUOS_PIN_LOAD);
    }


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

    if (ui_count > 0) {
        if (sys_index > -1 && sys_index <= ui_count && current_item_index < ui_count) {
            list_nav_next(sys_index);
        }
    } else {
        lv_obj_add_flag(ui_lblNavA, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblNavA, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblNavAGlyph, LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
    }

    refresh_screen(device.SCREEN.WAIT);

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

    free_items(items, item_count);
    close(js_fd);
    close(js_fd_sys);

    return 0;
}
