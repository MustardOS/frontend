#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <libgen.h>
#include "../common/common.h"
#include "../common/help.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/config.h"
#include "../common/glyph.h"
#include "../common/mini/mini.h"

static int js_fd;
static int js_fd_sys;

int turbo_mode = 0;
int msgbox_active = 0;
int input_disable = 0;
int SD2_found = 0;
int nav_sound = 0;
int safe_quit = 0;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;
struct mux_config config;

// Place as many NULL as there are options!
lv_obj_t *labels[] = {};
unsigned int label_count = sizeof(labels) / sizeof(labels[0]);

lv_obj_t *msgbox_element = NULL;

void *joystick_task() {
    struct input_event ev;

    while (1) {
        if (input_disable) {
            continue;
        }
        read(js_fd, &ev, sizeof(struct input_event));
        switch (ev.type) {
            case EV_KEY:
                if (ev.value == 1) {
                    lv_obj_add_flag(ui_lblFirst, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lblButton, LV_OBJ_FLAG_HIDDEN);
                    switch (ev.code) {
                        case JOY_A:
                            lv_label_set_text(ui_lblButton, "↦⇓");
                            break;
                        case JOY_B:
                            lv_label_set_text(ui_lblButton, "↧⇒");
                            break;
                        case JOY_X:
                            lv_label_set_text(ui_lblButton, "↥⇐");
                            break;
                        case JOY_Y:
                            lv_label_set_text(ui_lblButton, "↤⇑");
                            break;
                        case JOY_L1:
                            lv_label_set_text(ui_lblButton, "↖");
                            break;
                        case JOY_R1:
                            lv_label_set_text(ui_lblButton, "↗");
                            break;
                        case JOY_SELECT:
                            lv_label_set_text(ui_lblButton, "⇷");
                            break;
                        case JOY_START:
                            lv_label_set_text(ui_lblButton, "⇸");
                            break;
                        case JOY_MENU:
                            lv_label_set_text(ui_lblButton, "⇹");
                            break;
                        case JOY_PLUS:
                            lv_label_set_text(ui_lblButton, "⇾");
                            break;
                        case JOY_MINUS:
                            lv_label_set_text(ui_lblButton, "⇽");
                            break;
                        case JOY_POWER:
                            safe_quit = 1;
                            break;
                        case JOY_L2:
                            lv_label_set_text(ui_lblButton, "↲");
                            break;
                        case JOY_R2:
                            lv_label_set_text(ui_lblButton, "↳");
                            break;
                        case BTN_TR2:
                            lv_label_set_text(ui_lblButton, "↺");
                            break;
                        case BTN_MODE:
                            lv_label_set_text(ui_lblButton, "↻");
                            break;
                    }
                } else {
                    lv_label_set_text(ui_lblButton, " ");
                }
                break;
            case EV_ABS:
                if (ev.code == ABS_HAT0Y || ev.code == ABS_RX) {
                    lv_obj_add_flag(ui_lblFirst, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lblButton, LV_OBJ_FLAG_HIDDEN);
                    switch (ev.value) {
                        case -4096 ... -2048:
                            lv_label_set_text(ui_lblButton, "↾");
                            break;
                        case -1:
                            lv_label_set_text(ui_lblButton, "↟");
                            break;
                        case 1:
                            lv_label_set_text(ui_lblButton, "↡");
                            break;
                        case 2048 ... 4096:
                            lv_label_set_text(ui_lblButton, "⇂");
                            break;
                        default:
                            lv_label_set_text(ui_lblButton, " ");
                            break;
                    }
                } else if (ev.code == ABS_HAT0X || ev.code == ABS_Z) {
                    lv_obj_add_flag(ui_lblFirst, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lblButton, LV_OBJ_FLAG_HIDDEN);
                    switch (ev.value) {
                        case -4096 ... -2048:
                            lv_label_set_text(ui_lblButton, "↼");
                            break;
                        case -1:
                            lv_label_set_text(ui_lblButton, "↞");
                            break;
                        case 1:
                            lv_label_set_text(ui_lblButton, "↠");
                            break;
                        case 2048 ... 4096:
                            lv_label_set_text(ui_lblButton, "⇀");
                            break;
                        default:
                            lv_label_set_text(ui_lblButton, " ");
                            break;
                    }
                } else if (ev.code == ABS_RZ) {
                    lv_obj_add_flag(ui_lblFirst, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lblButton, LV_OBJ_FLAG_HIDDEN);
                    switch (ev.value) {
                        case -4096 ... -2048:
                            lv_label_set_text(ui_lblButton, "↿");
                            break;
                        case 2048 ... 4096:
                            lv_label_set_text(ui_lblButton, "⇃");
                            break;
                        default:
                            lv_label_set_text(ui_lblButton, " ");
                            break;
                    }
                } else if (ev.code == ABS_RY) {
                    lv_obj_add_flag(ui_lblFirst, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lblButton, LV_OBJ_FLAG_HIDDEN);
                    switch (ev.value) {
                        case -4096 ... -2048:
                            lv_label_set_text(ui_lblButton, "↽");
                            break;
                        case 2048 ... 4096:
                            lv_label_set_text(ui_lblButton, "⇁");
                            break;
                        default:
                            lv_label_set_text(ui_lblButton, " ");
                            break;
                    }
                }
            default:
                break;
        }
    }
}

void *joystick_system_task() {
    struct input_event ev;

    while (1) {
        read(js_fd_sys, &ev, sizeof(struct input_event));
        switch (ev.type) {
            case EV_KEY:
                if (ev.value == 1) {
                    switch (ev.code) {
                        case JOY_POWER:
                            safe_quit = 1;
                            break;
                    }
                }
                break;
            default:
                break;
        }
    }
}

void glyph_task() {
    // TODO: Bluetooth connectivity!

/*
    if (is_network_connected() > 0) {
        lv_obj_set_style_text_color(ui_staNetwork, lv_color_hex(theme.STATUS.NETWORK.ACTIVE), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staNetwork, theme.STATUS.NETWORK.ACTIVE_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    else {
        lv_obj_set_style_text_color(ui_staNetwork, lv_color_hex(theme.STATUS.NETWORK.NORMAL), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staNetwork, theme.STATUS.NETWORK.NORMAL_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
*/

    if (atoi(read_text_from_file(BATT_CHARGER))) {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.STATUS.BATTERY.ACTIVE),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.STATUS.BATTERY.ACTIVE_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (read_battery_capacity() <= 15) {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.STATUS.BATTERY.LOW), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.STATUS.BATTERY.LOW_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.STATUS.BATTERY.NORMAL),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.STATUS.BATTERY.NORMAL_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void init_elements() {
    lv_obj_move_foreground(ui_pnlFooter);
    lv_obj_move_foreground(ui_pnlHeader);

    if (bar_footer) {
        lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (bar_header) {
        lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/system/bin", 1);
    setenv("NO_COLOR", "1", 1);

    lv_init();
    fbdev_init();

    static lv_color_t buf1[DISP_BUF_SIZE];
    static lv_color_t buf2[DISP_BUF_SIZE];
    static lv_disp_draw_buf_t disp_buf;

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    if (strcasecmp(HARDWARE, "RG28XX") == 0) {
        disp_drv.hor_res = SCREEN_HEIGHT;
        disp_drv.ver_res = SCREEN_WIDTH;
        disp_drv.sw_rotate = 1;
        disp_drv.rotated = LV_DISP_ROT_90;
    } else {
        disp_drv.hor_res = SCREEN_WIDTH;
        disp_drv.ver_res = SCREEN_HEIGHT;
    }
    lv_disp_drv_register(&disp_drv);

    load_config(&config);

    ui_init();
    init_elements();

    lv_obj_set_user_data(ui_scrTester, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());
    lv_label_set_text(ui_staCapacity, get_capacity());

    load_theme(&theme, basename(argv[0]));
    apply_theme();

    char *current_wall = load_wallpaper(ui_scrTester, NULL, theme.MISC.ANIMATED_BACKGROUND);
    if (strlen(current_wall) > 3) {
        if (theme.MISC.ANIMATED_BACKGROUND) {
            lv_obj_t * img = lv_gif_create(ui_pnlWall);
            lv_gif_set_src(img, current_wall);
        } else {
            lv_img_set_src(ui_imgWall, current_wall);
        }
    } else {
        lv_img_set_src(ui_imgWall, &ui_img_nothing_png);
    }

    load_font_text(basename(argv[0]), ui_scrTester);

    if (config.SETTINGS.GENERAL.SOUND == 2) {
        nav_sound = 1;
    }

    struct dt_task_param dt_par;
    struct bat_task_param bat_par;

    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;

    js_fd = open(JOY_DEVICE, O_RDONLY);
    if (js_fd < 0) {
        perror("Failed to open joystick device");
        return 1;
    }

    js_fd_sys = open(SYS_DEVICE, O_RDONLY);
    if (js_fd_sys < 0) {
        perror("Failed to open joystick device");
        return 1;
    }

    pthread_t joystick_thread;
    pthread_t joystick_system_thread;

    pthread_create(&joystick_thread, NULL, (void *(*)(void *)) joystick_task, NULL);
    pthread_create(&joystick_system_thread, NULL, (void *(*)(void *)) joystick_system_task, NULL);

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

    init_elements();
    while (!safe_quit) {
        lv_task_handler();
        usleep(SCREEN_WAIT);
    }

    pthread_cancel(joystick_thread);
    pthread_cancel(joystick_system_thread);

    close(js_fd);

    return 0;
}

uint32_t mux_tick(void) {
    static uint64_t start_ms = 0;

    if (start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}
