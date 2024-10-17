#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/img/nothing.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/config.h"
#include "../common/device.h"
#include "ui/theme.h"

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
        fprintf(stderr, "Usage: %s <message>\n", argv[0]);
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

    load_config(&config);

    ui_init();

    load_theme(&theme, &config, &device, basename(argv[0]));
    apply_theme();

    lv_obj_set_user_data(ui_scrStart, "muxstart");

    if (config.BOOT.FACTORY_RESET) {
        char init_wall[MAX_BUFFER_SIZE];
        snprintf(init_wall, sizeof(init_wall), "M:%s/theme/image/wall/muxstart.png", INTERNAL_PATH);
        lv_img_set_src(ui_imgWall, strdup(init_wall));
    } else {
        load_wallpaper(ui_scrStart, NULL, ui_pnlWall, ui_imgWall, 0, 0, 0);
    }

    load_font_text(basename(argv[0]), ui_scrStart);

    if (TEST_IMAGE) display_testing_message(ui_scrStart);

    lv_obj_set_y(ui_pnlMessage, theme.VERBOSE_BOOT.Y_POS);
    lv_label_set_text(ui_lblMessage, argv[1]);
    refresh_screen();

    return 0;
}
