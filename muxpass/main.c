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
#include "../common/img/nothing.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/passcode.h"
#include "../common/input.h"
#include "ui/theme.h"

char *mux_module;
static int js_fd;
static int js_fd_sys;

int turbo_mode = 0;
int msgbox_active = 0;
int input_disable = 0;
int SD2_found = 0;
int nav_sound = 0;
int exit_status = 2;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;

struct mux_config config;
struct mux_device device;
struct mux_passcode passcode;

char *p_type;
char *p_code;
char *p_msg;

lv_obj_t *msgbox_element = NULL;

lv_group_t *ui_group;

#define UI_COUNT 6
lv_obj_t *ui_objects[UI_COUNT];

lv_obj_t *ui_mux_panels[2];

void init_navigation_groups() {
    ui_objects[0] = ui_rolComboOne;
    ui_objects[1] = ui_rolComboTwo;
    ui_objects[2] = ui_rolComboThree;
    ui_objects[3] = ui_rolComboFour;
    ui_objects[4] = ui_rolComboFive;
    ui_objects[5] = ui_rolComboSix;

    ui_group = lv_group_create();

    for (unsigned int i = 0; i < sizeof(ui_objects) / sizeof(ui_objects[0]); i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
    }
}

void handle_confirm(void) {
    char b1[2], b2[2], b3[2], b4[2], b5[2], b6[2];
    uint32_t bs = sizeof(b1);

    lv_roller_get_selected_str(ui_rolComboOne, b1, bs);
    lv_roller_get_selected_str(ui_rolComboTwo, b2, bs);
    lv_roller_get_selected_str(ui_rolComboThree, b3, bs);
    lv_roller_get_selected_str(ui_rolComboFour, b4, bs);
    lv_roller_get_selected_str(ui_rolComboFive, b5, bs);
    lv_roller_get_selected_str(ui_rolComboSix, b6, bs);

    char try_code[13];
    sprintf(try_code, "%s%s%s%s%s%s", b1, b2, b3, b4, b5, b6);

    if (strcasecmp(try_code, p_code) == 0) {
        exit_status = 1;
        mux_input_stop();
    }
}

void handle_back(void) {
    exit_status = 2;
    mux_input_stop();
}

void handle_up(void) {
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    play_sound("navigate", nav_sound, 0);
    lv_roller_set_selected(element_focused,
                           lv_roller_get_selected(element_focused) - 1,
                           LV_ANIM_ON);
}

void handle_down(void) {
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    play_sound("navigate", nav_sound, 0);
    lv_roller_set_selected(element_focused,
                           lv_roller_get_selected(element_focused) + 1,
                           LV_ANIM_ON);
}

void handle_left(void) {
    play_sound("navigate", nav_sound, 0);
    nav_prev(ui_group, 1);
}

void handle_right(void) {
    play_sound("navigate", nav_sound, 0);
    nav_next(ui_group, 1);
}

void init_elements() {
    ui_mux_panels[0] = ui_pnlFooter;
    ui_mux_panels[1] = ui_pnlHeader;

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
            ui_lblNavMenu,
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
}

int main(int argc, char *argv[]) {
    mux_module = basename(argv[0]);
    load_device(&device);

    char *cmd_help = "\nmuOS Extras - Passcode\nUsage: %s <-t>\n\nOptions:\n"
                     "\t-t Type of passcode lock <boot|launch|setting>\n\n";

    int opt;
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
            case 't':
                p_type = optarg;
                break;
            default:
                fprintf(stderr, cmd_help, argv[0]);
                return 2;
        }
    }

    if (p_type == NULL) {
        fprintf(stderr, cmd_help, argv[0]);
        return 2;
    }

    load_passcode(&passcode, &device);

    if (strcasecmp(p_type, "boot") == 0) {
        p_code = passcode.CODE.BOOT;
        p_msg = passcode.MESSAGE.BOOT;
    } else if (strcasecmp(p_type, "launch") == 0) {
        p_code = passcode.CODE.LAUNCH;
        p_msg = passcode.MESSAGE.LAUNCH;
    } else if (strcasecmp(p_type, "setting") == 0) {
        p_code = passcode.CODE.SETTING;
        p_msg = passcode.MESSAGE.SETTING;
    } else {
        fprintf(stderr, cmd_help, argv[0]);
        return 2;
    }

    if (strcasecmp(p_code, "000000") == 0) return 1;

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

    ui_common_screen_init(&theme, &device, TS("PASSCODE"));
    ui_init(ui_pnlContent);
    init_elements();
    load_overlay_image(ui_screen, theme.MISC.IMAGE_OVERLAY);

    if (strlen(p_msg) > 1) {
        lv_label_set_text(ui_lblMessage, p_msg);
        lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_set_user_data(ui_screen, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());

    apply_theme();

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND,
                   theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND);

    load_font_text(basename(argv[0]), ui_screen);

    nav_sound = init_nav_sound(mux_module);
    init_navigation_groups();

    struct dt_task_param dt_par;
    struct bat_task_param bat_par;

    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;

    js_fd = open(device.INPUT.EV1, O_RDONLY);
    if (js_fd < 0) {
        perror("Failed to open joystick device");
        return 2;
    }

    js_fd_sys = open(device.INPUT.EV0, O_RDONLY);
    if (js_fd_sys < 0) {
        perror("Failed to open joystick device");
        return 2;
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
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
            },
            .idle_handler = ui_common_handle_idle,
    };
    mux_input_task(&input_opts);

    close(js_fd);
    close(js_fd_sys);

    return exit_status;
}
