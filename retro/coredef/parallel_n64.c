#include "coredef.h"

// The cached interpreter is far too slow for N64 on this hardware, so the recompiler is
// pinned. mupen64plus_next remains the assigned default; this only matters when someone
// picks this core instead.
static const struct coredef_option options[] = {
    {.key = "parallel-n64-cpucore", .value = "dynamic_recompiler"},
    {.key = "parallel-n64-screensize", .value = "320x240"},
};

COREDEF_CORE(parallel_n64, "parallel_n64", options);
