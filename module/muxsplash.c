#include "../lvgl/lvgl.h"
#include "../lvgl/src/drivers/fbdev.h"
#include "ui/ui_muxsplash.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "../common/common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/theme.h"

char *mux_module;
char *osd_message;
int msgbox_active = 0;
int nav_sound = 0;

struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int progress_onscreen = -1;
int ui_count = 0;
int current_item_index = 0;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;
lv_obj_t *kiosk_image = NULL;

// Stubs to appease the compiler!
void list_nav_prev(void) {}

void list_nav_next(void) {}

void setup_background_process() {
    pid_t pid = fork();

    if (pid == -1) {
        perror("Failed to fork");
        exit(1);
    } else if (pid > 0) {
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    load_device(&device);

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <full path to PNG image>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    setup_background_process();

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

    ui_init();

    char init_wall[MAX_BUFFER_SIZE];
    snprintf(init_wall, sizeof(init_wall), "M:%s", argv[1]);

    lv_obj_t *img = lv_img_create(ui_scrSplash);
    lv_img_set_src(img, init_wall);

    if (TEST_IMAGE) display_testing_message(ui_scrSplash);

    overlay_image = lv_img_create(ui_scrSplash);
    load_overlay_image(ui_scrSplash, overlay_image, theme.MISC.IMAGE_OVERLAY);

    kiosk_image = lv_img_create(ui_scrSplash);
    load_kiosk_image(ui_scrSplash, kiosk_image);

    refresh_screen(device.SCREEN.WAIT);

    return 0;
}
