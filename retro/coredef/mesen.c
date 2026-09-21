#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "mesen_audio_sample_rate", .value = "44100"},
};

COREDEF_CORE(mesen, "mesen", options);
