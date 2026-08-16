#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "reicast_threaded_rendering", .value = "enabled"},
    {.key = "reicast_auto_skip_frame", .value = "disabled"},
    {.key = "reicast_detect_vsync_swap_interval", .value = "enabled"},
    {.key = "reicast_anisotropic_filtering", .value = "off"},
    {.key = "reicast_synchronous_rendering", .value = "disabled"},
    {.key = "reicast_delay_frame_swapping", .value = "disabled"},
    {.key = "reicast_enable_dsp", .value = "disabled"},
};

COREDEF_CORE(flycastvl, "flycastvl", options);
