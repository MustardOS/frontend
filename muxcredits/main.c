#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "ui/ui.h"
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../common/common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/theme.h"
#include "../common/input.h"

char *mux_module;

int turbo_mode = 0;
int msgbox_active = 0;
int SD2_found = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;

struct mux_config config;
struct mux_device device;
struct theme_config theme;

int progress_onscreen = -1;
int ui_count = 0;
int current_item_index = 0;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;

// Stubs to appease the compiler!
void list_nav_prev(void) {}

void list_nav_next(void) {}

void timeout_task() {
    mux_input_stop();
}

void handle_quit(void) {
    mux_input_stop();
}

void refresh(void) {
    refresh_screen(device.SCREEN.WAIT);
}

int main() {
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

    ui_init();

    animFade_Animation(ui_conStart, -1000);
    animScroll_Animation(ui_conScroll, 12000);
    animFade_Animation(ui_conSpecial, 65000);
    animFade_Animation(ui_conKofi, 75000);

    if (!config.BOOT.FACTORY_RESET) write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "credit");

    lv_timer_create(timeout_task, 90000, NULL);

    int js_fd = open(device.INPUT.EV1, O_RDONLY);
    if (js_fd < 0) {
        perror("Failed to open joystick device");
        return 1;
    }

    int js_fd_sys = open(device.INPUT.EV0, O_RDONLY);
    if (js_fd_sys < 0) {
        perror("Failed to open joystick device");
        return 1;
    }

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = 16 /* ~60 FPS */,
            .combo = {
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_START),
                            .press_handler = handle_quit,
                    },
            },
            .idle_handler = refresh,
    };
    mux_input_task(&input_opts);

    close(js_fd);
    close(js_fd_sys);

    return 0;
}
