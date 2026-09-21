#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "fbneo-samplerate", .value = "44100"},
};

COREDEF_CORE(fbneo, "fbneo", options);
