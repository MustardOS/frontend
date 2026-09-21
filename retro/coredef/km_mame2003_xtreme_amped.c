#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "mame2003-xtreme-amped-sample_rate", .value = "44100"},
};

COREDEF_CORE(km_mame2003_xtreme_amped, "km_mame2003_xtreme_amped", options);
