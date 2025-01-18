#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../lvgl/lvgl.h"
#include "../lvgl/src/drivers/display/sdl.h"
#include "../lvgl/src/drivers/input/evdev.h"
#include "init.h"
#include "ui_common.h"
#include "common.h"
#include "language.h"
#include "options.h"
#include "config.h"
#include "device.h"
#include "theme.h"

__thread uint64_t start_ms = 0;
static struct dt_task_param dt_par;
static struct bat_task_param bat_par;
int current_capacity = -1;

lv_timer_t *bluetooth_timer;
lv_timer_t *network_timer;
lv_timer_t *battery_timer;

uint32_t mux_tick(void) {
    struct timespec tv_now;
    clock_gettime(CLOCK_MONOTONIC, &tv_now);

    uint64_t now_ms = ((uint64_t) tv_now.tv_sec * 1000) + (tv_now.tv_nsec / 1000000);
    if (!start_ms) start_ms = now_ms;

    return (uint32_t) (now_ms - start_ms);
}

void init_display() {
    lv_init();
    sdl_init();

    static lv_disp_drv_t disp_drv;
    static lv_disp_draw_buf_t disp_buf;
    struct screen_dimension dims = get_device_dimensions();

    uint32_t disp_buf_size = dims.WIDTH * dims.HEIGHT;
    lv_disp_draw_buf_init(&disp_buf, (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t)), NULL, disp_buf_size);
    lv_disp_drv_init(&disp_drv);

    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = dims.WIDTH;
    disp_drv.ver_res = dims.HEIGHT;
    disp_drv.sw_rotate = device.SCREEN.ROTATE;
    disp_drv.rotated = device.SCREEN.ROTATE;
    disp_drv.full_refresh = 1;
    disp_drv.direct_mode = 1;
    disp_drv.antialiasing = 1;
    disp_drv.color_chroma_key = lv_color_hex(0xFF00FF);

    lv_disp_drv_register(&disp_drv);
    lv_disp_flush_ready(&disp_drv);
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

void init_timer(void (*ui_refresh_task)(lv_timer_t *), void (*update_system_info)(lv_timer_t *)) {
    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;

    if (ui_refresh_task) {
        lv_timer_t *ui_refresh_timer = lv_timer_create(ui_refresh_task, 0, NULL);
        lv_timer_ready(ui_refresh_timer);
        lv_timer_set_period(ui_refresh_timer, TIMER_REFRESH);

        lv_refr_now(NULL);
    }

    lv_timer_t *datetime_timer = lv_timer_create(datetime_task, TIMER_DATETIME, &dt_par);
    lv_timer_ready(datetime_timer);

    lv_timer_t *capacity_timer = lv_timer_create(capacity_task, TIMER_CAPACITY, &bat_par);
    lv_timer_ready(capacity_timer);

    lv_timer_t *status_timer = lv_timer_create(status_task, TIMER_STATUS, NULL);
    lv_timer_ready(status_timer);

    if (device.DEVICE.HAS_BLUETOOTH && config.VISUAL.BLUETOOTH) {
        bluetooth_timer = lv_timer_create(bluetooth_task, 0, NULL);
        lv_timer_ready(bluetooth_timer);
    }

    if (device.DEVICE.HAS_NETWORK && config.VISUAL.NETWORK) {
        network_timer = lv_timer_create(network_task, 0, NULL);
        lv_timer_ready(network_timer);
    }

    if (config.VISUAL.BATTERY) {
        battery_timer = lv_timer_create(battery_task, 0, NULL);
        lv_timer_ready(battery_timer);
    }

    if (update_system_info) {
        lv_timer_t *sysinfo_timer = lv_timer_create(update_system_info, TIMER_SYSINFO, NULL);
        lv_timer_ready(sysinfo_timer);
    }

    lv_refr_now(NULL);
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

void status_task() {
    if (progress_onscreen > 0) {
        progress_onscreen -= 1;
    } else {
        if (!lv_obj_has_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN);
        }

        if (!lv_obj_has_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN);
        }

        if (!msgbox_active) {
            progress_onscreen = -1;
        }
    }
}

void bluetooth_task() {
    //update_bluetooth_status(ui_staBluetooth, &theme);
    //lv_timer_set_period(bluetooth_timer, TIMER_BLUETOOTH);
}

void network_task() {
    update_network_status(ui_staNetwork, &theme);
    lv_timer_set_period(network_timer, TIMER_NETWORK);
}

void battery_task() {
    int bat_cap = read_int_from_file(device.BATTERY.CHARGER, 1);
    if (bat_cap != current_capacity) {
        current_capacity = bat_cap;
        update_battery_capacity(ui_staCapacity, &theme);
        lv_timer_set_period(battery_timer, TIMER_BATTERY);
    }
}
