#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "np2kai_FastMC", .value = "ON"},
};

COREDEF_CORE(np2kai, "np2kai", options);
