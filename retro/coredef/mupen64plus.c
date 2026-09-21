#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "mupen64plus-43screensize", .value = "320x240"},
    {.key = "mupen64plus-cpucore", .value = "dynamic_recompiler"},
};

COREDEF_CORE(mupen64plus, "mupen64plus", options);
