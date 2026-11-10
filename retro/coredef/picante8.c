#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "picante8_mouse_source", .value = "auto"},
};

/*
 * Picante8 has one Mouse Middle Click, on L2, so R2 joins it rather than sitting unbound.
 * Every other source keeps the usual handheld map.
 */
static const int source_target[COREDEF_SOURCE_COUNT] = {
    8,              // A       -> libretro A      O
    0,              // B       -> libretro B      X
    9,              // X       -> libretro X      Keyboard Key
    1,              // Y       -> libretro Y      Mouse D-Pad Hold
    10,             // L1      -> libretro L      Mouse Left Click
    11,             // R1      -> libretro R      Mouse Right Click
    12,             // L2      -> libretro L2     Mouse Middle Click
    12,             // R2      -> libretro L2     Mouse Middle Click
    14,             // L3      -> libretro L3
    15,             // R3      -> libretro R3
    2,              // Select  -> libretro Select
    3,              // Start   -> libretro Start
    4,              // D-pad up
    5,              // D-pad down
    6,              // D-pad left
    7,              // D-pad right
    -1, -1, -1, -1, // Left stick
    -1, -1, -1, -1, // Right stick
};

COREDEF_CORE_MAP(picante8, "picante8", options, source_target);
