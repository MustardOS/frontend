#include "debug.h"
#include "options.h"
#include "common.h"

#define DEBUG_FILE CONF_CONFIG_PATH "system/debug_mode"

static int debug_cached = -1;

void nop_debug_mode(void) {
    debug_cached = -1;
}

int is_debug_mode(void) {
    if (debug_cached == -1) debug_cached = read_line_int_from(DEBUG_FILE, 1);
    return debug_cached;
}
