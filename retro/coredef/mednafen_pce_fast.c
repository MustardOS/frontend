#include "coredef.h"

static const struct coredef_option options[] = {
    {"pce_fast_initial_scanline", "0"},
    {"pce_fast_last_scanline", "239"},
};

COREDEF_CORE(mednafen_pce_fast, "mednafen_pce_fast", options);
