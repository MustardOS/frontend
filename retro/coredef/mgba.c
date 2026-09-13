#include "coredef.h"

// Several GBA titles fake translucency by drawing a layer on alternate frames and letting
// the original LCD smear the two together. On a modern panel that arrives as raw flicker,
// so the core blends the frames itself. Smart only engages where alternation is detected,
// which leaves everything that is not flickering perfectly sharp.
static const struct coredef_option options[] = {
    {.key = "mgba_interframe_blending", .value = "mix_smart"},
};

COREDEF_CORE(mgba, "mgba", options);
