#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "sgx_initial_scanline", .value = "0"},
    {.key = "sgx_last_scanline", .value = "239"},
};

COREDEF_CORE(mednafen_supergrafx, "mednafen_supergrafx", options);
