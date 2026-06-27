#include <time.h>
#include "datetime.h"
#include "init.h"
#include "config.h"
#include "options.h"

char *get_datetime(void) {
    const time_t now = time(NULL);
    const struct tm *time_info = localtime(&now);
    static char datetime_str[MAX_BUFFER_SIZE];

    const char *fmt;
    switch (config.clock.notation) {
        case 0:
            fmt = TIME_STRING_12;
            break;
        case 1:
            fmt = TIME_STRING_24;
            break;
        case 2:
            fmt = TIME_STRING_DD_MM_12;
            break;
        case 3:
            fmt = TIME_STRING_DD_MM_24;
            break;
        case 4:
            fmt = TIME_STRING_MM_DD_12;
            break;
        case 5:
            fmt = TIME_STRING_MM_DD_24;
            break;
        default:
            fmt = *config.clock.custom ? config.clock.custom : TIME_STRING_12;
            break;
    }

    strftime(datetime_str, sizeof(datetime_str), fmt, time_info);
    return datetime_str;
}

void datetime_task(const lv_timer_t *timer) {
    LV_UNUSED(timer);
    if (ui_lbl_datetime) lv_label_set_text(ui_lbl_datetime, get_datetime());
}
