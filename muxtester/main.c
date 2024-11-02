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

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;

void handle_input(mux_input_type type, mux_input_action action) {
    const char *glyph[MUX_INPUT_COUNT] = {
            // Gamepad buttons:
            [MUX_INPUT_A] = "↦⇓",
            [MUX_INPUT_B] = "↧⇒",
            [MUX_INPUT_C] = "⇫",
            [MUX_INPUT_X] = "↥⇐",
            [MUX_INPUT_Y] = "↤⇑",
            [MUX_INPUT_Z] = "⇬",
            [MUX_INPUT_L1] = "↰",
            [MUX_INPUT_L2] = "↲",
            [MUX_INPUT_L3] = "↺",
            [MUX_INPUT_R1] = "↱",
            [MUX_INPUT_R2] = "↳",
            [MUX_INPUT_R3] = "↻",
            [MUX_INPUT_SELECT] = "⇷",
            [MUX_INPUT_START] = "⇸",

            // D-pad:
            [MUX_INPUT_DPAD_UP] = "↟",
            [MUX_INPUT_DPAD_DOWN] = "↡",
            [MUX_INPUT_DPAD_LEFT] = "↞",
            [MUX_INPUT_DPAD_RIGHT] = "↠",

            // Left stick:
            [MUX_INPUT_LS_UP] = "↾",
            [MUX_INPUT_LS_DOWN] = "⇂",
            [MUX_INPUT_LS_LEFT] = "↼",
            [MUX_INPUT_LS_RIGHT] = "⇀",

            // Right stick:
            [MUX_INPUT_RS_UP] = "↿",
            [MUX_INPUT_RS_DOWN] = "⇃",
            [MUX_INPUT_RS_LEFT] = "↽",
            [MUX_INPUT_RS_RIGHT] = "⇁",

            // Volume buttons:
            [MUX_INPUT_VOL_UP] = "⇾",
            [MUX_INPUT_VOL_DOWN] = "⇽",

            // Function buttons:
            [MUX_INPUT_MENU_LONG] = "⇹",
    };

    switch (action) {
        case MUX_INPUT_PRESS:
            if (glyph[type]) {
                lv_obj_add_flag(ui_lblFirst, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_lblButton, LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text(ui_lblButton, glyph[type]);
            }
            break;
        case MUX_INPUT_RELEASE:
            lv_label_set_text(ui_lblButton, " ");
            break;
        case MUX_INPUT_HOLD:
            break;
    }
}

void handle_power() {
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "tester");
    mux_input_stop();
}

void glyph_task() {
    // TODO: Bluetooth connectivity!
    //update_bluetooth_status(ui_staBluetooth, &theme);

    update_network_status(ui_staNetwork, &theme);
    update_battery_capacity(ui_staCapacity, &theme);
}

void init_elements() {
    lv_label_set_text(ui_lblMessage, TS("Press POWER to finish testing"));
    lv_obj_set_y(ui_pnlMessage, -5);
    lv_obj_set_height(ui_pnlFooter, 0);
    lv_obj_move_foreground(ui_pnlHeader);
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_border_width(ui_pnlMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (bar_header) {
        lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    if (TEST_IMAGE) display_testing_message(ui_screen);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image, theme.MISC.IMAGE_OVERLAY);
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

    ui_common_screen_init(&theme, &device, TS("INPUT TESTER"));
    ui_init(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, theme.MISC.ANIMATED_BACKGROUND,
                   theme.ANIMATION.ANIMATION_DELAY, theme.MISC.RANDOM_BACKGROUND);

    load_font_text(basename(argv[0]), ui_screen);

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

    refresh_screen(device.SCREEN.WAIT);

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = 16 /* ~60 FPS */,
            .press_handler = {
                    [MUX_INPUT_POWER_SHORT] = handle_power,
            },
            .input_handler = handle_input,
            .idle_handler = ui_common_handle_idle,
    };
    mux_input_task(&input_opts);

    close(js_fd);
    close(js_fd_sys);

    return 0;
}
