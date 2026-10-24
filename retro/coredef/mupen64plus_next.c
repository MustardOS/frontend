#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "mupen64plus-43screensize", .value = "640x480"},
    {.key = "mupen64plus-EnableNativeResFactor", .value = "1"},
    {.key = "mupen64plus-ThreadedRenderer", .value = "True"},
    {.key = "mupen64plus-HybridFilter", .value = "False"},
    {.key = "mupen64plus-EnableOverscan", .value = "Enabled"},
    {.key = "mupen64plus-OverscanTop", .value = "8"},
    {.key = "mupen64plus-OverscanLeft", .value = "8"},
    {.key = "mupen64plus-OverscanRight", .value = "8"},
    {.key = "mupen64plus-OverscanBottom", .value = "8"},
    {.key = "mupen64plus-cpucore", .value = "dynamic_recompiler"},
    {.key = "mupen64plus-rsp-plugin", .value = "hle"},
};

COREDEF_CORE(mupen64plus_next, "mupen64plus_next", options);
