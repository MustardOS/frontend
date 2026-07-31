#include "coredef.h"

static const struct coredef_option options[] = {
    {"pce_initial_scanline", "0"},
    {"pce_last_scanline", "239"},
    {"pce_initial_scanline_pal", "0"},
    {"pce_last_scanline_pal", "239"},
};

COREDEF_CORE(mednafen_pce, "mednafen_pce", options);
