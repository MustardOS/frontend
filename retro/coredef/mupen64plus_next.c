#include "coredef.h"

// GLideN64 at 640x480 is more than the Mali-G31 on an RK3326 can finish inside one refresh,
// so frames double and the audio queue starves. 320x240 is what most N64 games render at and
// an exact 2x scale to a 640x480 panel. On a GKD Pixel 2 it took Super Mario 64 from 38% of
// refreshes missed to under 1%.
static const struct coredef_option options[] = {
    {.key = "mupen64plus-ThreadedRenderer", .value = "True"},
    {.key = "mupen64plus-43screensize", .value = "320x240"},
};

COREDEF_CORE(mupen64plus_next, "mupen64plus_next", options);
