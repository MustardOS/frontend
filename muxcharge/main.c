#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <sys/types.h>
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
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "../common/common.h"
#include "../common/help.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/mini/mini.h"

__thread uint64_t start_ms = 0;

char *mux_prog;
static int js_fd;

int turbo_mode = 0;
int msgbox_active = 0;
int input_disable = 0;
int SD2_found = 0;
int nav_sound = 0;
int exit_status = -1;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;

struct mux_config config;
struct mux_device device;

lv_obj_t *msgbox_element = NULL;

int blank = 0;

char capacity_info[MAX_BUFFER_SIZE];
char voltage_info[MAX_BUFFER_SIZE];
char health_info[MAX_BUFFER_SIZE];

lv_timer_t *battery_timer;

void check_for_cable() {
    if (file_exist(device.BATTERY.CHARGER)) {
        if (atoi(read_line_from_file(device.BATTERY.CHARGER, 1)) == 0) {
            exit_status = 1;
        }
    }
}

void set_brightness(int brightness) {
    char command[MAX_BUFFER_SIZE];
    snprintf(command, sizeof(command), "%s/device/%s/input/combo/bright.sh %d",
             INTERNAL_PATH, str_tolower(device.DEVICE.NAME), brightness);
    system(command);
}

void *joystick_task() {
    struct input_event ev;
    int epoll_fd;
    struct epoll_event event, events[device.DEVICE.EVENT];

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Error creating EPOLL instance");
        return NULL;
    }

    event.events = EPOLLIN;
    event.data.fd = js_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, js_fd, &event) == -1) {
        perror("Error with EPOLL controller");
        return NULL;
    }

    while (1) {
        int num_events = epoll_wait(epoll_fd, events, device.DEVICE.EVENT, 64);
        if (num_events == -1) {
            perror("Error with EPOLL wait event timer");
            continue;
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == js_fd) {
                int ret = read(js_fd, &ev, sizeof(struct input_event));
                if (ret == -1) {
                    perror("Error reading input");
                    continue;
                }

                switch (ev.type) {
                    case EV_KEY:
                        if (ev.value == 1) {
                            if (ev.code == device.RAW_INPUT.BUTTON.POWER_SHORT) {
                                if (blank < 5) {
                                    lv_timer_pause(battery_timer);

                                    lv_obj_add_flag(ui_lblCapacity, LV_OBJ_FLAG_HIDDEN);
                                    lv_obj_add_flag(ui_lblVoltage, LV_OBJ_FLAG_HIDDEN);
                                    lv_obj_add_flag(ui_lblHealth, LV_OBJ_FLAG_HIDDEN);

                                    lv_obj_add_flag(ui_lblCapacity, LV_OBJ_FLAG_FLOATING);
                                    lv_obj_add_flag(ui_lblVoltage, LV_OBJ_FLAG_FLOATING);
                                    lv_obj_add_flag(ui_lblHealth, LV_OBJ_FLAG_FLOATING);

                                    lv_label_set_text(ui_lblBoot, _("Booting System - Please Wait..."));

                                    lv_task_handler();
                                    usleep(device.SCREEN.WAIT);

                                    exit_status = 0;
                                }

                                blank = 0;
                                set_brightness(atoi(read_text_from_file("/opt/muos/config/brightness.txt")));
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
        }

        lv_task_handler();
        usleep(device.SCREEN.WAIT);
    }
}

void battery_task() {
    snprintf(capacity_info, sizeof(capacity_info), "%s: %d%%", _("Capacity"), read_battery_capacity());
    snprintf(voltage_info, sizeof(voltage_info), "%s: %s", _("Voltage"), read_battery_voltage());
    snprintf(health_info, sizeof(health_info), "%s: %s", _("Health"), read_battery_health());

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
    mux_prog = basename(argv[0]);
    load_device(&device);
    srand(time(NULL));

    lv_init();
    fbdev_init(device.SCREEN.DEVICE);

    static lv_disp_draw_buf_t disp_buf;
    uint32_t disp_buf_size = device.SCREEN.BUFFER;

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
    lv_disp_drv_register(&disp_drv);

    load_config(&config);
    load_language(mux_prog);

    ui_init();
    set_brightness(atoi(read_text_from_file("/opt/muos/config/brightness.txt")));

    load_theme(&theme, &config, &device, basename(argv[0]));
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

    if (TEST_IMAGE) display_testing_message(ui_scrCharge);

    if (config.SETTINGS.GENERAL.SOUND) {
        if (SDL_Init(SDL_INIT_AUDIO) >= 0) {
            Mix_Init(0);
            Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
            printf("SDL init success!\n");
            nav_sound = 1;
        } else {
            fprintf(stderr, "Failed to init SDL\n");
        }
    }

    lv_obj_set_y(ui_pnlCharge, theme.CHARGER.Y_POS);

    js_fd = open(device.INPUT.EV0, O_RDONLY);
    if (js_fd < 0) {
        perror("Failed to open joystick device");
        return 1;
    }

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);

    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = evdev_read;
    indev_drv.user_data = (void *) (intptr_t) js_fd;

    lv_indev_drv_register(&indev_drv);

    battery_timer = lv_timer_create(battery_task, UINT16_MAX / 32, NULL);
    lv_timer_ready(battery_timer);

    pthread_t joystick_thread;
    pthread_create(&joystick_thread, NULL, (void *(*)(void *)) joystick_task, NULL);

    while (exit_status < 0) {
        usleep(device.SCREEN.WAIT);
    }

    pthread_cancel(joystick_thread);

    close(js_fd);

    return exit_status;
}

uint32_t mux_tick(void) {
    struct timespec tv_now;
    clock_gettime(CLOCK_REALTIME, &tv_now);

    uint64_t now_ms = ((uint64_t) tv_now.tv_sec * 1000) + (tv_now.tv_nsec / 1000000);
    start_ms = start_ms || now_ms;

    return (uint32_t)(now_ms - start_ms);
}
