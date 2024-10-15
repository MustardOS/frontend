#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/img/nothing.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/input.h"
#include "../common/input/list_nav.h"

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

pthread_t scan_networks_thread;

int nav_moved = 1;
char *current_wall = "";

lv_obj_t *msgbox_element = NULL;

int progress_onscreen = -1;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

int ui_count = 0;
int current_item_index = 0;
int first_open = 1;

void show_help() {
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(ui_lblTitle)), TS("Detect, display and connect to "
                                                            "available Wi-Fi networks"));
}

void *scan_networks() {
    system("/opt/muos/script/web/ssid.sh");
    return NULL;
}

void create_network_items() {
    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    char *scan_file = "/tmp/net_scan";
    FILE *file = fopen(scan_file, "r");
    if (file == NULL) {
        fprintf(stderr, "Error opening file %s\n", scan_file);
        lv_label_set_text(ui_lblScreenMessage, TS("No Wi-Fi Networks Found"));
        return;
    }

    if (strlen(read_line_from_file(scan_file, 1)) > 0) {
        lv_obj_add_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(ui_lblScreenMessage, TS("No Wi-Fi Networks Found"));
        return;
    }

    char ssid[40];
    while (fgets(ssid, sizeof(ssid), file)) {
        ui_count++;

        lv_obj_t *ui_pnlNetScan = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(&theme, &device, ui_pnlNetScan);

        lv_obj_t *ui_lblNetScanItem = lv_label_create(ui_pnlNetScan);
        apply_theme_list_item(&theme, ui_lblNetScanItem, str_nonew(ssid), false, false);

        lv_obj_t *ui_lblNetScanGlyph = lv_img_create(ui_pnlNetScan);
        apply_theme_list_glyph(&theme, ui_lblNetScanGlyph, mux_module, "netscan");

        lv_group_add_obj(ui_group, ui_lblNetScanItem);
        lv_group_add_obj(ui_group_glyph, ui_lblNetScanGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlNetScan);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblNetScanItem, ui_lblNetScanGlyph, str_nonew(ssid));
    }
    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
    fclose(file);
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound, 0);
    }
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void handle_confirm(void) {
    if (msgbox_active) {
        return;
    }

    play_sound("confirm", nav_sound, 1);
    write_text_to_file("/run/muos/global/network/ssid", "w", CHAR,
                       lv_label_get_text(lv_group_get_focused(ui_group)));
    write_text_to_file("/run/muos/global/network/pass", "w", CHAR, "");
    write_text_to_file("/run/muos/global/network/address", "w", CHAR, "");
    write_text_to_file("/run/muos/global/network/subnet", "w", CHAR, "");
    write_text_to_file("/run/muos/global/network/gateway", "w", CHAR, "");
    write_text_to_file("/run/muos/global/network/dns", "w", CHAR, "");
    mux_input_stop();
}

void handle_back(void) {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 1);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound("back", nav_sound, 1);
    mux_input_stop();
}

void handle_rescan(void) {
    if (msgbox_active) {
        return;
    }

    play_sound("confirm", nav_sound, 1);
    load_mux("net_scan");
    mux_input_stop();
}

void handle_help(void) {
    if (msgbox_active) {
        return;
    }

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 1);
        show_help();
    }
}

void init_elements() {
    lv_obj_move_foreground(ui_pnlFooter);
    lv_obj_move_foreground(ui_pnlHeader);
    lv_obj_move_foreground(ui_pnlHelp);
    lv_obj_move_foreground(ui_pnlProgressBrightness);
    lv_obj_move_foreground(ui_pnlProgressVolume);

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

    lv_label_set_text(ui_lblNavA, TG("Use"));
    lv_label_set_text(ui_lblNavB, TG("Back"));
    lv_label_set_text(ui_lblNavX, TG("Rescan"));

    lv_obj_t *nav_hide[] = {
            ui_lblNavCGlyph,
            ui_lblNavC,
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
        if (lv_group_get_obj_count(ui_group) > 0) {
            static char old_wall[MAX_BUFFER_SIZE];
            static char new_wall[MAX_BUFFER_SIZE];

            snprintf(old_wall, sizeof(old_wall), "%s", current_wall);
            snprintf(new_wall, sizeof(new_wall), "%s", load_wallpaper(
                    ui_screen, ui_group, theme.MISC.ANIMATED_BACKGROUND, theme.MISC.RANDOM_BACKGROUND));

            if (strcasecmp(new_wall, old_wall) != 0) {
                strcpy(current_wall, new_wall);
                if (strlen(new_wall) > 3) {
                    if (theme.MISC.RANDOM_BACKGROUND) {
                        load_image_random(ui_imgWall, new_wall);
                    } else {
                        switch (theme.MISC.ANIMATED_BACKGROUND) {
                            case 1:
                                lv_gif_set_src(lv_gif_create(ui_pnlWall), new_wall);
                                break;
                            case 2:
                                load_image_animation(ui_imgWall, theme.ANIMATION.ANIMATION_DELAY, new_wall);
                                break;
                            default:
                                lv_img_set_src(ui_imgWall, new_wall);
                                break;
                        }
                    }
                } else {
                    lv_img_set_src(ui_imgWall, &ui_image_Nothing);
                }
            }

            static char static_image[MAX_BUFFER_SIZE];
            snprintf(static_image, sizeof(static_image), "%s",
                     load_static_image(ui_screen, ui_group));

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
                lv_img_set_src(ui_imgBox, &ui_image_Nothing);
            }
        }
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

    ui_common_screen_init(&theme, &device, TS("NETWORK SCAN"));
    init_elements();
    load_overlay_image(ui_screen, theme.MISC.IMAGE_OVERLAY);
    lv_label_set_text(ui_lblScreenMessage, TS("Scanning for Wi-Fi Networks..."));
    lv_obj_clear_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_user_data(ui_screen, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());

    current_wall = load_wallpaper(ui_screen, NULL, theme.MISC.ANIMATED_BACKGROUND, theme.MISC.RANDOM_BACKGROUND);
    if (strlen(current_wall) > 3) {
        if (theme.MISC.RANDOM_BACKGROUND) {
            load_image_random(ui_imgWall, current_wall);
        } else {
            switch (theme.MISC.ANIMATED_BACKGROUND) {
                case 1:
                    lv_gif_set_src(lv_gif_create(ui_pnlWall), current_wall);
                    break;
                case 2:
                    load_image_animation(ui_imgWall, theme.ANIMATION.ANIMATION_DELAY, current_wall);
                    break;
                default:
                    lv_img_set_src(ui_imgWall, current_wall);
                    break;
            }
        }
    } else {
        lv_img_set_src(ui_imgWall, &ui_image_Nothing);
    }

    load_font_text(basename(argv[0]), ui_screen);
    load_font_section(basename(argv[0]), FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(mux_module, FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(mux_module, FONT_FOOTER_FOLDER, ui_pnlFooter);

    nav_sound = init_nav_sound(mux_module);
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

    pthread_create(&scan_networks_thread, NULL, scan_networks, NULL);
    while (pthread_tryjoin_np(scan_networks_thread, NULL) != 0) {
        refresh_screen();
    }

    create_network_items();

    refresh_screen();

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
                    [MUX_INPUT_X] = handle_rescan,
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

    return 0;
}
