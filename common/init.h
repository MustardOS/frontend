#pragma once

#include "../lvgl/lvgl.h"
#include "input.h"
#include "options.h"

extern lv_obj_t *ui_lbl_datetime;
extern lv_obj_t *ui_sta_capacity;
extern lv_obj_t *ui_lbl_message;
extern lv_obj_t *ui_pnl_message;

extern char mux_module[MAX_BUFFER_SIZE];
extern char mux_dim[15];
extern int msgbox_active;
extern lv_obj_t *msgbox_element;
extern int progress_onscreen;
extern int block_input;
extern int page_nav_blocked;
extern int last_idle;
extern int current_brightness;
extern int current_volume;
extern int is_blank;
extern int config_auth;
extern int idle_state_exists;
extern int safe_quit_exists;
extern int hdmi_refresh_exists;
extern int blank_exists;
extern unsigned idle_state_changes;
extern unsigned saver_type_changes;
extern unsigned charging_changes;
extern int hdmi_mode;
extern int g350_menu_pressed;

struct screen_dimension {
    int width;
    int height;
};

void detach_parent_process(void);

void refresh_screen(const lv_obj_t *screen, int flush);

void safe_quit(int exit_status);

void init_module(const char *module);

void init_display();

void init_input(mux_input_options *opts, int def_combo);

void init_timer(void (*ui_refresh_task)(lv_timer_t *), void (*update_system_info)(const lv_timer_t *));

void timer_suspend_all(void);

void timer_resume_all(void);

void timer_destroy_all(void);

void init_fonts(void);

void init_theme(int panel_init, int long_mode);

void status_task(const lv_timer_t *timer);

void status_poll(void);

void bluetooth_task(const lv_timer_t *timer);

void network_task(const lv_timer_t *timer);

void battery_task();
