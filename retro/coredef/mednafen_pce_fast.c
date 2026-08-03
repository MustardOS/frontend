#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "pce_fast_initial_scanline", .value = "0"},
    {.key = "pce_fast_last_scanline", .value = "239"},
};

COREDEF_CORE(mednafen_pce_fast, "mednafen_pce_fast", options);
