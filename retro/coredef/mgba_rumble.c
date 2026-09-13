#include "coredef.h"

// Same core as mgba, so it carries the same alternate frame transparency problem.
// See mgba.c for why Smart is the value we want.
static const struct coredef_option options[] = {
    {.key = "mgba_interframe_blending", .value = "mix_smart"},
};

COREDEF_CORE(mgba_rumble, "mgba_rumble", options);
