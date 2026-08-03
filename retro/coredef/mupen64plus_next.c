#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "mupen64plus-ThreadedRenderer", .value = "True"},
};

COREDEF_CORE(mupen64plus_next, "mupen64plus_next", options);
