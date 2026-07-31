#include "coredef.h"

static const struct coredef_option options[] = {
    {"sgx_initial_scanline", "0"},
    {"sgx_last_scanline", "239"},
};

COREDEF_CORE(mednafen_supergrafx, "mednafen_supergrafx", options);
