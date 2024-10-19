#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/common.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/input.h"

char *mux_module;
static int js_fd;
static int js_fd_sys;

int turbo_mode = 0;
int msgbox_active = 0;
int input_disable = 0;
int SD2_found = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;

struct mux_config config;
struct mux_device device;
struct theme_config theme;

int nav_moved = 1;
int current_item_index = 0;
int first_open = 1;

lv_obj_t *msgbox_element = NULL;

int progress_onscreen = -1;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

#define UI_COUNT 8
lv_obj_t *ui_objects[UI_COUNT];
lv_obj_t *ui_icons[UI_COUNT];

lv_obj_t *ui_mux_panels[5];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblContent,    TS("Content on storage devices (SD1/SD2/USB) can be found and launched here")},
            {ui_lblFavourites, TS("Content marked as favourite can be found and launched here")},
            {ui_lblHistory,    TS("Content previously launched can be found and launched here")},
            {ui_lblApps,       TS("Various applications can be found and launched here")},
            {ui_lblInfo,       TS("Various information can be found and launched here")},
            {ui_lblConfig,     TS("Various configurations can be changed here")},
            {ui_lblReboot,     TS("Reboot your device safely")},
            {ui_lblShutdown,   TS("Shut down your device safely")},
    };

    char *message = TG("No Help Information Found");
    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);

    for (int i = 0; i < num_messages; i++) {
        if (element_focused == help_messages[i].element) {
            message = help_messages[i].message;
            break;
        }
    }

    if (strlen(message) <= 1) message = TG("No Help Information Found");

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
}

void init_navigation_groups() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlExplore,
            ui_pnlFavourites,
            ui_pnlHistory,
            ui_pnlApps,
            ui_pnlInfo,
            ui_pnlConfig,
            ui_pnlReboot,
            ui_pnlShutdown,
    };

    ui_objects[0] = ui_lblContent;
    ui_objects[1] = ui_lblFavourites;
    ui_objects[2] = ui_lblHistory;
    ui_objects[3] = ui_lblApps;
    ui_objects[4] = ui_lblInfo;
    ui_objects[5] = ui_lblConfig;
    ui_objects[6] = ui_lblReboot;
    ui_objects[7] = ui_lblShutdown;

    ui_icons[0] = ui_icoContent;
    ui_icons[1] = ui_icoFavourites;
    ui_icons[2] = ui_icoHistory;
    ui_icons[3] = ui_icoApps;
    ui_icons[4] = ui_icoInfo;
    ui_icons[5] = ui_icoConfig;
    ui_icons[6] = ui_icoReboot;
    ui_icons[7] = ui_icoShutdown;

    apply_theme_list_panel(&theme, &device, ui_pnlExplore);
    apply_theme_list_panel(&theme, &device, ui_pnlFavourites);
    apply_theme_list_panel(&theme, &device, ui_pnlHistory);
    apply_theme_list_panel(&theme, &device, ui_pnlApps);
    apply_theme_list_panel(&theme, &device, ui_pnlInfo);
    apply_theme_list_panel(&theme, &device, ui_pnlConfig);
    apply_theme_list_panel(&theme, &device, ui_pnlReboot);
    apply_theme_list_panel(&theme, &device, ui_pnlShutdown);

    apply_theme_list_item(&theme, ui_lblContent, TS("Explore Content"), false, false);
    apply_theme_list_item(&theme, ui_lblFavourites, TS("Favourites"), false, false);
    apply_theme_list_item(&theme, ui_lblHistory, TS("History"), false, false);
    apply_theme_list_item(&theme, ui_lblApps, TS("Applications"), false, false);
    apply_theme_list_item(&theme, ui_lblInfo, TS("Information"), false, false);
    apply_theme_list_item(&theme, ui_lblConfig, TS("Configuration"), false, false);
    apply_theme_list_item(&theme, ui_lblReboot, TS("Reboot"), false, false);
    apply_theme_list_item(&theme, ui_lblShutdown, TS("Shutdown"), false, false);

    apply_theme_list_glyph(&theme, ui_icoContent, mux_module, "explore");
    apply_theme_list_glyph(&theme, ui_icoFavourites, mux_module, "favourite");
    apply_theme_list_glyph(&theme, ui_icoHistory, mux_module, "history");
    apply_theme_list_glyph(&theme, ui_icoApps, mux_module, "apps");
    apply_theme_list_glyph(&theme, ui_icoInfo, mux_module, "info");
    apply_theme_list_glyph(&theme, ui_icoConfig, mux_module, "config");
    apply_theme_list_glyph(&theme, ui_icoReboot, mux_module, "reboot");
    apply_theme_list_glyph(&theme, ui_icoShutdown, mux_module, "shutdown");

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (unsigned int i = 0; i < sizeof(ui_objects) / sizeof(ui_objects[0]); i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_glyph, ui_icons[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);

        apply_size_to_content(&theme, ui_pnlContent, ui_objects[i], ui_icons[i], lv_label_get_text(ui_objects[i]));
    }
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == 0) ? UI_COUNT - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, UI_COUNT, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound, 0);
    }
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == UI_COUNT - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, UI_COUNT, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void handle_a() {
    if (msgbox_active) {
        return;
    }

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

    if (element_focused == ui_lblContent) {
        play_sound("confirm", nav_sound, 1);
        load_mux("explore_alt");
    } else if (element_focused == ui_lblFavourites) {
        play_sound("confirm", nav_sound, 1);
        load_mux("favourite");
    } else if (element_focused == ui_lblHistory) {
        play_sound("confirm", nav_sound, 1);
        load_mux("history");
    } else if (element_focused == ui_lblApps) {
        play_sound("confirm", nav_sound, 1);
        load_mux("app");
    } else if (element_focused == ui_lblInfo) {
        play_sound("confirm", nav_sound, 1);
        load_mux("info");
    } else if (element_focused == ui_lblConfig) {
        play_sound("confirm", nav_sound, 1);
        load_mux("config");
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "config");
    } else if (element_focused == ui_lblReboot) {
        play_sound("reboot", nav_sound, 1);
        load_mux("reboot");
    } else if (element_focused == ui_lblShutdown) {
        play_sound("shutdown", nav_sound, 1);
        load_mux("shutdown");
    }
    mux_input_stop();
}

void handle_b() {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    load_mux("launcher");
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "");
    mux_input_stop();
}

void handle_menu() {
    if (msgbox_active) {
        return;
    }

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 0);
        show_help(lv_group_get_focused(ui_group));
    }
}

void handle_up() {
    if (msgbox_active) {
        return;
    }

    // Horizontal Navigation with 2 rows of 4 items.  Wrap on Row.
    if (theme.MISC.NAVIGATION_TYPE == 4 &&
        (current_item_index == 0 || current_item_index == 4)) {
        list_nav_next(3);
        // Horizontal Navigation with 3 item first row, 5 item second row.  Wrap on Row.
    } else if (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index == 0) {
        list_nav_next(2);
    } else if (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index == 3) {
        list_nav_next(4);
        // Regular Navigation
    } else {
        list_nav_prev(1);
    }
}

void handle_down() {
    if (msgbox_active) {
        return;
    }

    // Horizontal Navigation with 2 rows of 4 items.  Wrap on Row.
    if (theme.MISC.NAVIGATION_TYPE == 4 &&
        (current_item_index == 3 || current_item_index == 7)) {
        list_nav_prev(3);
        // Horizontal Navigation with 3 item first row, 5 item second row.  Wrap on Row.
    } else if (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index == 2) {
        list_nav_prev(2);
    } else if (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index == 7) {
        list_nav_prev(4);
        // Regular Navigation
    } else {
        list_nav_next(1);
    }
}

void handle_up_hold(void) {//prev
    if (msgbox_active) {
        return;
    }

    // Don't wrap around when scrolling on hold.
    if ((theme.MISC.NAVIGATION_TYPE == 4 && current_item_index > 0 && current_item_index <= 3) ||
        (theme.MISC.NAVIGATION_TYPE == 4 && current_item_index > 4) ||
        (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index > 0 && current_item_index <= 2) ||
        (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index > 3) ||
        (theme.MISC.NAVIGATION_TYPE < 4 && current_item_index > 0)) {
        handle_up();
    }
}

void handle_down_hold(void) {//next
    if (msgbox_active) {
        return;
    }

    // Don't wrap around when scrolling on hold.
    if ((theme.MISC.NAVIGATION_TYPE == 4 && current_item_index < UI_COUNT - 1 && current_item_index > 3) ||
        (theme.MISC.NAVIGATION_TYPE == 4 && current_item_index < 3) ||
        (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index < UI_COUNT - 1 && current_item_index > 2) ||
        (theme.MISC.NAVIGATION_TYPE == 5 && current_item_index < 2) ||
        (theme.MISC.NAVIGATION_TYPE < 4 && current_item_index < UI_COUNT - 1)) {
        handle_down();
    }
}

void handle_left() {
    if (msgbox_active) {
        return;
    }

    // Horizontal Navigation with 2 rows of 4 items
    if (theme.MISC.NAVIGATION_TYPE == 2 || theme.MISC.NAVIGATION_TYPE == 4) {
        list_nav_prev(4);
    }
    // Horizontal Navigation with 3 item first row, 5 item second row
    if (theme.MISC.NAVIGATION_TYPE == 3 || theme.MISC.NAVIGATION_TYPE == 5) {
        switch (current_item_index) {
            case 3:
            case 4:
                list_nav_prev(3);
                break;
            case 5:
                list_nav_prev(4);
                break;
            case 6:
            case 7:
                list_nav_prev(5);
                break;
        }
    }
}

void handle_right() {
    if (msgbox_active) {
        return;
    }

    // Horizontal Navigation with 2 rows of 4 items
    if (theme.MISC.NAVIGATION_TYPE == 2 || theme.MISC.NAVIGATION_TYPE == 4) {
        list_nav_next(4);
    }
    // Horizontal Navigation with 3 item first row, 5 item second row
    if (theme.MISC.NAVIGATION_TYPE == 3 || theme.MISC.NAVIGATION_TYPE == 5) {
        switch (current_item_index) {
            case 0:
                list_nav_next(3);
                break;
            case 1:
                list_nav_next(4);
                break;
            case 2:
                list_nav_next(5);
                break;
        }
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

    lv_obj_t *nav_hide[] = {
            ui_lblNavBGlyph,
            ui_lblNavB,
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

    lv_obj_set_user_data(ui_lblContent, "explore");
    lv_obj_set_user_data(ui_lblFavourites, "favourite");
    lv_obj_set_user_data(ui_lblHistory, "history");
    lv_obj_set_user_data(ui_lblApps, "apps");
    lv_obj_set_user_data(ui_lblInfo, "info");
    lv_obj_set_user_data(ui_lblConfig, "config");
    lv_obj_set_user_data(ui_lblReboot, "reboot");
    lv_obj_set_user_data(ui_lblShutdown, "shutdown");

    if (TEST_IMAGE) display_testing_message(ui_screen);
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
    update_bars(ui_barProgressBrightness, ui_barProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

void direct_to_previous() {
    if (file_exist(MUOS_PDI_LOAD)) {
        char *prev = read_text_from_file(MUOS_PDI_LOAD);
        int text_hit = 0;

        for (unsigned int i = 0; i < sizeof(ui_objects) / sizeof(ui_objects[0]); i++) {
            const char *u_data = lv_obj_get_user_data(ui_objects[i]);

            if (strcasecmp(u_data, prev) == 0) {
                text_hit = i;
                break;
            }
        }

        if (text_hit != 0) {
            list_nav_next(text_hit);
            nav_moved = 1;
        }
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

    ui_common_screen_init(&theme, &device, TS("MAIN MENU"));
    ui_init(ui_pnlContent);
    init_elements();
    load_overlay_image(ui_screen, theme.MISC.IMAGE_OVERLAY);

    lv_obj_set_user_data(ui_screen, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND,
                   theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND);

    load_font_text(basename(argv[0]), ui_screen);
    load_font_section(basename(argv[0]), FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(mux_module, FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(mux_module, FONT_FOOTER_FOLDER, ui_pnlFooter);

    nav_sound = init_nav_sound(mux_module);
    init_navigation_groups();

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

    lv_timer_t *osd_timer = lv_timer_create(osd_task, UINT16_MAX / 32, &osd_par);
    lv_timer_ready(osd_timer);

    lv_timer_t *glyph_timer = lv_timer_create(glyph_task, UINT16_MAX / 64, NULL);
    lv_timer_ready(glyph_timer);

    lv_timer_t *ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
    lv_timer_ready(ui_refresh_timer);

    direct_to_previous();

    refresh_screen();

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = 16 /* ~60 FPS */,
            .swap_btn = config.SETTINGS.ADVANCED.SWAP,
            .swap_axis = (theme.MISC.NAVIGATION_TYPE >= 1 && theme.MISC.NAVIGATION_TYPE <= 5),
            .stick_nav = true,
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
            },
            .hold_handler = {
                    // List navigation:
                    [MUX_INPUT_DPAD_UP] = handle_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_down_hold,
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

    return 0;
}
