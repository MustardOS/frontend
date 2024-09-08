#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "ui/ui.h"
#include <sys/types.h>
#include <linux/joystick.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "../common/common.h"
#include "../common/config.h"
#include "../common/device.h"

__thread uint64_t start_ms = 0;

int NAV_DPAD_HOR;
int NAV_ANLG_HOR;
int NAV_DPAD_VER;
int NAV_ANLG_VER;
int NAV_A;
int NAV_B;

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

int main() {
    load_device(&device);
    seed_random();

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

    ui_init();

    animFade_Animation(ui_conStart, -1000);
    animScroll_Animation(ui_conScroll, 10000);
    animFade_Animation(ui_conKofi, 50000);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "credit");

    int hold = 0;
    while (!safe_quit) {
        hold++;
        lv_task_handler();
        usleep(device.SCREEN.WAIT);
        if (hold > 3500) {
            safe_quit = 1;
        }
        //printf("%d\n", hold);
    }

    return 0;
}

uint32_t mux_tick(void) {
    struct timespec tv_now;
    clock_gettime(CLOCK_REALTIME, &tv_now);

    uint64_t now_ms = ((uint64_t) tv_now.tv_sec * 1000) + (tv_now.tv_nsec / 1000000);
    start_ms = start_ms || now_ms;

    return (uint32_t)(now_ms - start_ms);
}
