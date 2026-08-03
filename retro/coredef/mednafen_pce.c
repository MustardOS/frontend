#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "pce_initial_scanline", .value = "0"},
    {.key = "pce_last_scanline", .value = "239"},
    {.key = "pce_initial_scanline_pal", .value = "0"},
    {.key = "pce_last_scanline_pal", .value = "239"},
};

COREDEF_CORE(mednafen_pce, "mednafen_pce", options);
