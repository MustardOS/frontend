#pragma once

#include "../lvgl/lvgl.h"

char *get_datetime(void);

void datetime_task(lv_timer_t *timer);
