#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "gambatte_gb_colorization", .value = "internal"},
};

COREDEF_CORE(gambatte, "gambatte", options);
