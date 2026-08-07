#pragma once

#include <stdint.h>
#include "../../lvgl/lvgl.h"
#include "../theme.h"

typedef enum {
    notify_info = 0, // Ordinary progress and whatever message
    notify_success,  // Something finished as intended
    notify_warning,  // Worth noticing, but not worth interrupting for
    notify_error     // Fullscreen message, must be acknowledged before carrying on
} notify_level;

void notify_send(notify_level level, const char *msg);

void notify_send_for(notify_level level, const char *msg, uint32_t hold_ms);

void notify_tick(void);

void notify_reset(void);

void notify_screen_reset(void);

uint32_t notify_hold_ms(void);
