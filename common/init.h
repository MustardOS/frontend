#pragma once

#include "../lvgl/lvgl.h"

extern lv_obj_t *ui_lblDatetime;
extern lv_obj_t *ui_staCapacity;
extern lv_obj_t *ui_lblMessage;
extern lv_obj_t *ui_pnlMessage;

struct screen_dimension {
    int WIDTH;
    int HEIGHT;
};

struct dt_task_param {
    lv_obj_t *lblDatetime;
};

struct bat_task_param {
    lv_obj_t *staCapacity;
};

void setup_background_process();

void refresh_screen(lv_obj_t *screen);

void safe_quit(int exit_status);

void init_display();

void init_input(int *joy_general, int *joy_power, int *joy_volume, int *joy_extra);

void init_timer(void (*ui_refresh_task)(lv_timer_t *), void (*update_system_info)(lv_timer_t *));

void init_fonts();

void init_theme(int panel_init, int long_mode);

void status_task();

void bluetooth_task();

void network_task();

void battery_task();
