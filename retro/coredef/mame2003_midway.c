#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "mame2003-sample_rate", .value = "44100"},
};

COREDEF_CORE(mame2003_midway, "mame2003_midway", options);
