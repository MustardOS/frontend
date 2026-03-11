#pragma once

#include "../lvgl/lvgl.h"

void battery_set_daemon_mode(int enable);

void battery_init(void);

void battery_reset(void);

void battery_update(void);

const char *battery_get_voltage(void);

int battery_get_capacity(void);

int battery_get_low_threshold(void);

int battery_is_low(void);

int battery_is_charging(void);

char *battery_get_capacity_glyph(void);

void battery_capacity_task(lv_timer_t *timer);
