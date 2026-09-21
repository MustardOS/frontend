#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "vice_sound_sample_rate", .value = "44100"},
};

COREDEF_CORE(vice_xcbm2, "vice_xcbm2", options);
