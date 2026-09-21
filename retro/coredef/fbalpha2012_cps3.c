#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "fbalpha2012_cps3_samplerate", .value = "44100"},
};

COREDEF_CORE(fbalpha2012_cps3, "fbalpha2012_cps3", options);
