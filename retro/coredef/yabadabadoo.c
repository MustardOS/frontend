#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "yabadabadoo_numthreads", .value = "auto"},
};

COREDEF_CORE_THREADED(yabadabadoo, "yabadabadoo", options);
