#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "scummvm_samplerate", .value = "44100 Hz"},
};

COREDEF_CORE(scummvm, "scummvm", options);
