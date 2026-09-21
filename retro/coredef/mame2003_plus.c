#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "mame2003-plus_sample_rate", .value = "44100"},
};

COREDEF_CORE(mame2003_plus, "mame2003_plus", options);
