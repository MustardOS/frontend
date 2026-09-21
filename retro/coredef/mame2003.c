#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "mame2003_sample_rate", .value = "44100"},
};

COREDEF_CORE(mame2003, "mame2003", options);
