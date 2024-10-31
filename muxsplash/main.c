#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "../common/common.h"
#include "../common/device.h"

struct mux_device device;

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
    char image_path[MAX_BUFFER_SIZE];
    char device_dimension[15];
    get_device_dimension(device_dimension, sizeof(device_dimension));

    snprintf(image_path, sizeof(image_path), "%s/%simage/%s", 
                STORAGE_THEME, device_dimension, argv[1]);

    if (!file_exist(image_path)) {
        snprintf(image_path, sizeof(image_path), "%s/image/%s", 
                STORAGE_THEME, argv[1]);
    }

    snprintf(init_wall, sizeof(init_wall), "M:%s", image_path);

    lv_obj_t *img = lv_img_create(ui_scrSplash);
    lv_img_set_src(img, init_wall);

    refresh_screen();

    return 0;
}
