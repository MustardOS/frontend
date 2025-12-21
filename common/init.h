#pragma once

#include "../lvgl/lvgl.h"
#include "input.h"

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

void detach_parent_process(void);

void refresh_screen(lv_obj_t *screen);

void safe_quit(int exit_status);

void close_input(void);

void init_module(const char *module);

void init_display();

void init_input(mux_input_options *opts, int def_combo);

void dispose_input(void);

void init_timer(void (*ui_refresh_task)(lv_timer_t *), void (*update_system_info)(lv_timer_t *));

void timer_destroy_all(void);

void timer_suspend_all(void);

void init_fonts(void);

void init_theme(int panel_init, int long_mode);

void status_task(lv_timer_t *timer);

void bluetooth_task(lv_timer_t *timer);

void network_task(lv_timer_t *timer);

void battery_task();
