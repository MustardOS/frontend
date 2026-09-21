#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "fbneo-samplerate", .value = "44100"},
};

COREDEF_CORE(km_fbneo_xtreme_amped, "km_fbneo_xtreme_amped", options);
