#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../lvgl/lvgl.h"
#include "../lvgl/src/drivers/fbdev.h"
#include "../lvgl/src/drivers/evdev.h"
#include "init.h"
#include "ui_common.h"
#include "json/json.h"
#include "common.h"
#include "language.h"
#include "options.h"
#include "config.h"
#include "device.h"
#include "theme.h"

__thread uint64_t start_ms = 0;
static struct dt_task_param dt_par;
static struct bat_task_param bat_par;

uint32_t mux_tick(void) {
    struct timespec tv_now;
    clock_gettime(CLOCK_MONOTONIC, &tv_now);

    uint64_t now_ms = ((uint64_t) tv_now.tv_sec * 1000) + (tv_now.tv_nsec / 1000000);
    if (!start_ms) {
        start_ms = now_ms;
    }

    return (uint32_t) (now_ms - start_ms);
}

void refresh_screen(int wait) {
    lv_task_handler();
    usleep(wait);
}

void init_display() {
    struct screen_dimension dims = get_device_dimensions();

    lv_init();
    fbdev_init(device.SCREEN.DEVICE);

    static lv_disp_draw_buf_t disp_buf;
    uint32_t disp_buf_size = dims.WIDTH * dims.HEIGHT;

    lv_color_t *buf1 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));
    lv_color_t *buf2 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));
    if (!buf1 || !buf2) {
        perror("Failed to allocate display buffers");
        exit(EXIT_FAILURE);
    }

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, disp_buf_size);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res = dims.WIDTH;
    disp_drv.ver_res = dims.HEIGHT;
    disp_drv.sw_rotate = device.SCREEN.ROTATE;
    disp_drv.rotated = device.SCREEN.ROTATE;
    disp_drv.full_refresh = 0;
    disp_drv.direct_mode = 0;
    disp_drv.antialiasing = 1;
    disp_drv.color_chroma_key = lv_color_hex(0xFF00FF);

    lv_disp_drv_register(&disp_drv);
    lv_disp_flush_ready(&disp_drv);

    fbdev_force_refresh(true);
}

void init_input(int *js_fd, int *js_fd_sys) {
    *js_fd = open(device.INPUT.EV1, O_RDONLY);
    if (*js_fd < 0) {
        perror(lang.SYSTEM.NO_JOY);
        exit(EXIT_FAILURE);
    }

    *js_fd_sys = open(device.INPUT.EV0, O_RDONLY);
    if (*js_fd_sys < 0) {
        perror(lang.SYSTEM.NO_JOY);
        exit(EXIT_FAILURE);
    }

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);

    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = evdev_read;
    indev_drv.user_data = (void *) (intptr_t) (*js_fd);

    lv_indev_drv_register(&indev_drv);
}

void init_timer(void (*glyph_task_func)(lv_timer_t *), void (*ui_refresh_task)(lv_timer_t *),
                void (*update_system_info)(lv_timer_t *)) {
    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;

    lv_timer_t *datetime_timer = lv_timer_create(datetime_task, UINT16_MAX / 2, &dt_par);
    lv_timer_ready(datetime_timer);

    lv_timer_t *capacity_timer = lv_timer_create(capacity_task, UINT16_MAX / 2, &bat_par);
    lv_timer_ready(capacity_timer);

    if (glyph_task_func) {
        lv_timer_t *glyph_timer = lv_timer_create(glyph_task_func, UINT16_MAX / 64, NULL);
        lv_timer_ready(glyph_timer);
    }

    if (ui_refresh_task) {
        lv_timer_t *ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
        lv_timer_ready(ui_refresh_timer);
    }

    if (update_system_info) {
        lv_timer_t *sysinfo_timer = lv_timer_create(update_system_info, UINT16_MAX / 32, NULL);
        lv_timer_ready(sysinfo_timer);
    }
}

void init_fonts() {
    load_font_text(mux_module, ui_screen);
    load_font_section(mux_module, FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(mux_module, FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(mux_module, FONT_FOOTER_FOLDER, ui_pnlFooter);
}

void init_theme(int panel_init, int long_mode) {
    load_theme(&theme, &config, &device, mux_module);

    if (panel_init) {
        init_panel_style(&theme);
        init_item_style(&theme);
        init_glyph_style(&theme);
    }

    if (long_mode && theme.LIST_DEFAULT.LABEL_LONG_MODE != LV_LABEL_LONG_WRAP) init_item_animation();
}