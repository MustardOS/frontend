#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "util.h"
#include "exec.h"
#include "log.h"
#include "language.h"
#include "options.h"
#include "../module/muxshare.h"

const char *get_random_hex(void) {
    static int seeded = 0;
    if (!seeded) {
        srandom((unsigned) time(NULL) ^ (uintptr_t) &seeded);
        seeded = 1;
    }

    unsigned char r = random() % 256;
    unsigned char g = random() % 256;
    unsigned char b = random() % 256;

    char *hex = malloc(8);
    if (!hex) return NULL;
    snprintf(hex, 8, "%02X%02X%02X", r, g, b);

    return hex;
}

static void fix_range(int *min, int *max) {
    if (*min > *max) {
        int tmp = *min;
        *min = *max;
        *max = tmp;
    }
}

int clamp_range(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

int pct_to_int(int pct, int min, int max) {
    fix_range(&min, &max);
    pct = clamp_range(pct, 0, 100);

    const int range = max - min;
    return min + (pct * range + 50) / 100;
}

int int_to_pct(int num, int min, int max) {
    fix_range(&min, &max);
    num = clamp_range(num, min, max);

    const int range = max - min;
    return ((num - min) * 100 + range / 2) / range;
}

void set_setting_value(const char *script_name, int value, int offset) {
    char script_path[MAX_BUFFER_SIZE];
    snprintf(script_path, sizeof(script_path), DEV_SCRIPT "%s.sh", script_name);

    char value_str[8];
    snprintf(value_str, sizeof(value_str), "%d", value + offset);

    if (!block_input) {
        block_input = 1;
        const char *args[] = {script_path, value_str, NULL};
        run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);

        block_input = 0;
    }

    refresh_config = 1;
}

int get_index_on_delete(int current_index, int post_delete_count) {
    if (post_delete_count <= 0) return 0;

    int max_index = post_delete_count - 1;
    return current_index > max_index ? max_index : current_index;
}

int16_t validate_int16(int value, const char *field) {
    (void) field;
    if (value < INT16_MIN || value > INT16_MAX) {
        LOG_ERROR(mux_module, "%s", lang.SYSTEM.FAIL_INT16_LENGTH);
        return (value < INT16_MIN) ? INT16_MIN : INT16_MAX;
    }
    return (int16_t) value;
}
