#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "beetle_psx_frame_duping", .value = "enabled"},
};

COREDEF_CORE(mednafen_psx, "mednafen_psx", options);
