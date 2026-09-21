#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "LudicrousN64-43screensize", .value = "320x240"},
    {.key = "LudicrousN64-cpucore", .value = "dynamic_recompiler"},
};

COREDEF_CORE(km_ludicrousn64_2k22_xtreme_amped, "km_ludicrousn64_2k22_xtreme_amped", options);
