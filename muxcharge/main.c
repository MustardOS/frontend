#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/reboot.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <sys/stat.h>
#include <sys/epoll.h>
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

int blank = 0;

char capacity_info[MAX_BUFFER_SIZE];
char voltage_info[MAX_BUFFER_SIZE];
char health_info[MAX_BUFFER_SIZE];

void check_for_cable() {
    if (file_exist(BATT_CHARGER)) {
        if (atoi(read_line_from_file(BATT_CHARGER, 1)) == 0) {
            sync();
            sleep(1);
            reboot(RB_POWER_OFF);
        }
    }
}

void *joystick_task() {
    struct input_event ev;

    while (1) {
        read(js_fd, &ev, sizeof(struct input_event));
        switch (ev.type) {
            case EV_KEY:
                if (ev.value == 1) {
                    if (ev.code == JOY_POWER) {
                        if (blank < 5) {
                            safe_quit = 1;
                        }

                        blank = 0;
                        set_brightness(100);

                        usleep(500000);
                    }
                }
                break;
            default:
                break;
        }
    }
}

void battery_task() {
    snprintf(capacity_info, sizeof(capacity_info), "Capacity: %d%%", read_battery_capacity());
    snprintf(voltage_info, sizeof(voltage_info), "Voltage: %s", read_battery_voltage());
    snprintf(health_info, sizeof(health_info), "Health: %s", read_battery_health());

    lv_label_set_text(ui_lblCapacity, capacity_info);
    lv_label_set_text(ui_lblVoltage, voltage_info);
    lv_label_set_text(ui_lblHealth, health_info);

    if (blank == 5) {
        set_brightness(0);
    }

    check_for_cable();

    blank++;
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
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    lv_disp_drv_register(&disp_drv);

    load_config(&config);

    ui_init();
    set_brightness(100);

    load_theme(&theme, basename(argv[0]));
    apply_theme();

    lv_obj_set_user_data(ui_scrCharge, "muxcharge");

    char *current_wall = load_wallpaper(ui_scrCharge, NULL, theme.MISC.ANIMATED_BACKGROUND);
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

    load_font_text(basename(argv[0]), ui_scrCharge);

    if (config.SETTINGS.GENERAL.SOUND == 2) {
        nav_sound = 1;
    }

    lv_obj_set_y(ui_pnlCharge, theme.CHARGER.Y_POS);

    js_fd = open(SYS_DEVICE, O_RDONLY);
    if (js_fd < 0) {
        perror("Failed to open joystick device");
        return 1;
    }

    pthread_t joystick_thread;
    pthread_create(&joystick_thread, NULL, (void *(*)(void *)) joystick_task, NULL);

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);

    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = evdev_read;
    indev_drv.user_data = (void *) (intptr_t) js_fd;

    lv_indev_drv_register(&indev_drv);

    lv_timer_t *battery_timer = lv_timer_create(battery_task, UINT16_MAX / 32, NULL);
    lv_timer_ready(battery_timer);

    while (!safe_quit) {
        lv_task_handler();
        usleep(SCREEN_WAIT);
    }

    pthread_cancel(joystick_thread);

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
