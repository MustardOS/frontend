#include "coredef.h"

// The dynamic recompiler is the difference between full speed and unplayable on an A53,
// so it is pinned rather than left to the core's own judgement. Moving the sound unit to
// its own thread costs nothing on a quad core and takes the audio mixing off the same
// core as emulation, which is where the underruns came from.
static const struct coredef_option options[] = {
    {.key = "pcsx_rearmed_drc", .value = "enabled"},
    {.key = "pcsx_rearmed_spu_thread", .value = "enabled"},
};

COREDEF_CORE(pcsx_rearmed, "pcsx_rearmed", options);
