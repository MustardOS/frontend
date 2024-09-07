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
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "../common/common.h"
#include "../common/help.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/mini/mini.h"

__thread uint64_t start_ms = 0;

char *mux_prog;
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
struct mux_device device;

lv_obj_t *msgbox_element = NULL;

void *joystick_task() {
    struct input_event ev;

    while (1) {
        read(js_fd, &ev, sizeof(struct input_event));
        switch (ev.type) {
            case EV_KEY:
                if (ev.value == 1) {
                    lv_obj_add_flag(ui_lblFirst, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lblButton, LV_OBJ_FLAG_HIDDEN);
                    if (ev.code == device.RAW_INPUT.BUTTON.A) {
                        lv_label_set_text(ui_lblButton, "↦⇓");
                    } else if (ev.code == device.RAW_INPUT.BUTTON.B) {
                        lv_label_set_text(ui_lblButton, "↧⇒");
                    } else if (ev.code == device.RAW_INPUT.BUTTON.C) {
                        // TODO: Add glyph icon for this!
                    } else if (ev.code == device.RAW_INPUT.BUTTON.X) {
                        lv_label_set_text(ui_lblButton, "↥⇐");
                    } else if (ev.code == device.RAW_INPUT.BUTTON.Y) {
                        lv_label_set_text(ui_lblButton, "↤⇑");
                    } else if (ev.code == device.RAW_INPUT.BUTTON.Z) {
                        // TODO: Add glyph icon for this!
                    } else if (ev.code == device.RAW_INPUT.BUTTON.L1) {
                        lv_label_set_text(ui_lblButton, "↖");
                    } else if (ev.code == device.RAW_INPUT.BUTTON.R1) {
                        lv_label_set_text(ui_lblButton, "↗");
                    } else if (ev.code == device.RAW_INPUT.BUTTON.L2) {
                        lv_label_set_text(ui_lblButton, "↲");
                    } else if (ev.code == device.RAW_INPUT.BUTTON.R2) {
                        lv_label_set_text(ui_lblButton, "↳");
                    } else if (ev.code == device.RAW_INPUT.BUTTON.MENU_SHORT ||
                               ev.code == device.RAW_INPUT.BUTTON.MENU_LONG) {
                        lv_label_set_text(ui_lblButton, "⇹");
                    } else if (ev.code == device.RAW_INPUT.BUTTON.POWER_SHORT ||
                               ev.code == device.RAW_INPUT.BUTTON.POWER_LONG) {
                        lv_label_set_text(ui_lblButton, "↧⇒");
                    } else if (ev.code == device.RAW_INPUT.BUTTON.VOLUME_UP) {
                        lv_label_set_text(ui_lblButton, "⇾");
                    } else if (ev.code == device.RAW_INPUT.BUTTON.VOLUME_DOWN) {
                        lv_label_set_text(ui_lblButton, "⇽");
                    } else if (ev.code == device.RAW_INPUT.BUTTON.START) {
                        lv_label_set_text(ui_lblButton, "⇸");
                    } else if (ev.code == device.RAW_INPUT.BUTTON.SELECT) {
                        lv_label_set_text(ui_lblButton, "⇷");
                    } else if (ev.code == device.RAW_INPUT.ANALOG.LEFT.CLICK) {
                        lv_label_set_text(ui_lblButton, "↺");
                    } else if (ev.code == device.RAW_INPUT.ANALOG.RIGHT.CLICK) {
                        lv_label_set_text(ui_lblButton, "↻");
                    }
                } else {
                    lv_label_set_text(ui_lblButton, " ");
                }
                break;
            case EV_ABS:
                if (ev.code == device.RAW_INPUT.DPAD.UP ||
                    ev.code == device.RAW_INPUT.DPAD.DOWN) {
                    lv_obj_add_flag(ui_lblFirst, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lblButton, LV_OBJ_FLAG_HIDDEN);
                    if (ev.value == -1) {
                        lv_label_set_text(ui_lblButton, "↟");
                    } else if (ev.value == 1) {
                        lv_label_set_text(ui_lblButton, "↡");
                    } else {
                        lv_label_set_text(ui_lblButton, " ");
                    }
                } else if (ev.code == device.RAW_INPUT.DPAD.LEFT ||
                           ev.code == device.RAW_INPUT.DPAD.RIGHT) {
                    lv_obj_add_flag(ui_lblFirst, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lblButton, LV_OBJ_FLAG_HIDDEN);
                    if (ev.value == -1) {
                        lv_label_set_text(ui_lblButton, "↞");
                    } else if (ev.value == 1) {
                        lv_label_set_text(ui_lblButton, "↠");
                    } else {
                        lv_label_set_text(ui_lblButton, " ");
                    }
                } else if (ev.code == device.RAW_INPUT.ANALOG.LEFT.UP ||
                           ev.code == device.RAW_INPUT.ANALOG.LEFT.DOWN) {
                    lv_obj_add_flag(ui_lblFirst, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lblButton, LV_OBJ_FLAG_HIDDEN);
                    if (ev.value >= -device.INPUT.AXIS_MAX && ev.value <= -device.INPUT.AXIS_MIN) {
                        lv_label_set_text(ui_lblButton, "↾");
                    } else if (ev.value >= device.INPUT.AXIS_MIN && ev.value <= device.INPUT.AXIS_MAX) {
                        lv_label_set_text(ui_lblButton, "⇂");
                    } else {
                        lv_label_set_text(ui_lblButton, " ");
                    }
                } else if (ev.code == device.RAW_INPUT.ANALOG.LEFT.LEFT ||
                           ev.code == device.RAW_INPUT.ANALOG.LEFT.RIGHT) {
                    lv_obj_add_flag(ui_lblFirst, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lblButton, LV_OBJ_FLAG_HIDDEN);
                    if (ev.value >= -device.INPUT.AXIS_MAX && ev.value <= -device.INPUT.AXIS_MIN) {
                        lv_label_set_text(ui_lblButton, "↼");
                    } else if (ev.value >= device.INPUT.AXIS_MIN && ev.value <= device.INPUT.AXIS_MAX) {
                        lv_label_set_text(ui_lblButton, "⇀");
                    } else {
                        lv_label_set_text(ui_lblButton, " ");
                    }
                } else if (ev.code == ABS_RZ) {
                    lv_obj_add_flag(ui_lblFirst, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lblButton, LV_OBJ_FLAG_HIDDEN);
                    if (ev.value >= -device.INPUT.AXIS_MAX && ev.value <= -device.INPUT.AXIS_MIN) {
                        lv_label_set_text(ui_lblButton, "↿");
                    } else if (ev.value >= device.INPUT.AXIS_MIN && ev.value <= device.INPUT.AXIS_MAX) {
                        lv_label_set_text(ui_lblButton, "⇃");
                    } else {
                        lv_label_set_text(ui_lblButton, " ");
                    }
                } else if (ev.code == ABS_RY) {
                    lv_obj_add_flag(ui_lblFirst, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lblButton, LV_OBJ_FLAG_HIDDEN);
                    if (ev.value >= -device.INPUT.AXIS_MAX && ev.value <= -device.INPUT.AXIS_MIN) {
                        lv_label_set_text(ui_lblButton, "↽");
                    } else if (ev.value >= device.INPUT.AXIS_MIN && ev.value <= device.INPUT.AXIS_MAX) {
                        lv_label_set_text(ui_lblButton, "⇁");
                    } else {
                        lv_label_set_text(ui_lblButton, " ");
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
                    if (ev.code == device.RAW_INPUT.BUTTON.POWER_SHORT ||
                        ev.code == device.RAW_INPUT.BUTTON.POWER_LONG) {
                        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "tester");
                        safe_quit = 1;
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
    //update_bluetooth_status(ui_staBluetooth, &theme);

    update_network_status(ui_staNetwork, &theme);

    update_battery_capacity(ui_staCapacity, &theme);
}

void init_elements() {
    lv_label_set_text(ui_lblMessage, _("Press POWER to finish testing"));
    lv_obj_set_y(ui_pnlMessage, -5);
    lv_obj_set_height(ui_pnlFooter, 0);
    lv_obj_move_foreground(ui_pnlHeader);
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_border_width(ui_pnlMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (bar_header) {
        lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    char *overlay = load_overlay_image();
    if (strlen(overlay) > 0 && theme.MISC.IMAGE_OVERLAY) {
        lv_obj_t * overlay_img = lv_img_create(ui_screen);
        lv_img_set_src(overlay_img, overlay);
        lv_obj_move_foreground(overlay_img);
    }

    if (TEST_IMAGE) display_testing_message(ui_screen);
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
    load_theme(&theme, &config, &device, basename(argv[0]));
    load_language(mux_prog);

    ui_common_screen_init(&theme, &device, _("INPUT TESTER"));
    ui_init(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());

    apply_theme();

    char *current_wall = load_wallpaper(ui_screen, NULL, theme.MISC.ANIMATED_BACKGROUND);
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

    load_font_text(basename(argv[0]), ui_screen);

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

    while (!safe_quit) {
        lv_task_handler();
        usleep(device.SCREEN.WAIT);
    }

    pthread_cancel(joystick_thread);
    pthread_cancel(joystick_system_thread);

    close(js_fd);

    return 0;
}

uint32_t mux_tick(void) {
    struct timespec tv_now;
    clock_gettime(CLOCK_REALTIME, &tv_now);

    uint64_t now_ms = ((uint64_t) tv_now.tv_sec * 1000) + (tv_now.tv_nsec / 1000000);
    start_ms = start_ms || now_ms;

    return (uint32_t)(now_ms - start_ms);
}
