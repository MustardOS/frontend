#include <common/runtime/debug.h>
#include <common/base/options.h>
#include <common/storage/fileio.h>

static int debug_cached = -1;

void nop_debug_mode(void) {
    debug_cached = -1;
}

int is_debug_mode(void) {
    if (debug_cached == -1) debug_cached = read_line_int_from(DEBUG_FILE, 1);
    return debug_cached;
}
