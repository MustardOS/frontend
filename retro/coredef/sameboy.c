#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "sameboy_screen_layout", .value = "left-right"},
};

COREDEF_CORE(sameboy, "sameboy", options);
