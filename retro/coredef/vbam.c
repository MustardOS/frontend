#include "coredef.h"

// The other GBA core, with the same alternate frame transparency problem as mgba. Its
// Smart mode is the equivalent of mix_smart: it only blends where frames alternate, so
// nothing else loses definition. The remaining choice, motion blur, blends everything.
static const struct coredef_option options[] = {
    {.key = "vbam_interframeblending", .value = "smart"},
};

COREDEF_CORE(vbam, "vbam", options);
