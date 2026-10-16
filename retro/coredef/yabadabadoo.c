#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "yabadabadoo_numthreads", .value = "4"},
};

COREDEF_CORE_THREADED(yabadabadoo, "yabadabadoo", options);
