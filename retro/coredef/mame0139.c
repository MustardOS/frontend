#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "mame_current_sample_rate", .value = "44100Hz"},
};

COREDEF_CORE(mame0139, "mame0139", options);
