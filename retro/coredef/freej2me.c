#include "coredef.h"

static const struct coredef_option options[] = {
    // The core reads a phone keypad, so the dialling keys need somewhere sensible to sit and
    // the pointer needs to be usable without a touchscreen
    {.key = "freej2me_resolution", .value = "320x240"},
    {.key = "freej2me_rotate", .value = "0"},
    {.key = "freej2me_phone", .value = "Default"},
    {.key = "freej2me_fps", .value = "60"},
    {.key = "freej2me_sound", .value = "on"},
    {.key = "freej2me_pointertype", .value = "Mouse"},
    {.key = "freej2me_pointerxspeed", .value = "4"},
    {.key = "freej2me_pointeryspeed", .value = "4"},
    {.key = "freej2me_backlightcolor", .value = "Disabled"},
    {.key = "freej2me_logginglevel", .value = "0"},
};

/*
 * FreeJ2ME hands its libretro buttons out in phone order rather than handheld order, so out of
 * the box OK/Fire sits on Y, CLR on R3, and A and B dial nine and seven. Not every device has
 * sticks or stick clicks, so everything a game needs sits on the face buttons, shoulders,
 * triggers, Select, Start and the D-pad, checked against the core's own key table:
 *
 *   A OK/Fire, B CLR, X Num 5, Y Num 0, L1 Num 1, R1 Num 3, L2 star, R2 hash,
 *   Select and Start the two softkeys, and the D-pad the four directions.
 *
 * Num 7 and Num 9 are the keys games lean on least, so they are left for a remap rather than
 * taking a button from something more common. The sticks and their clicks stay unbound.
 */
static const int source_target[COREDEF_SOURCE_COUNT] = {
    1,              // A       -> libretro Y      OK/Fire
    15,             // B       -> libretro R3     CLR
    14,             // X       -> libretro L3     Num 5
    9,              // Y       -> libretro X      Num 0
    10,             // L1      -> libretro L      Num 1
    11,             // R1      -> libretro R      Num 3
    12,             // L2      -> libretro L2     Num *
    13,             // R2      -> libretro R2     Num #
    -1,             // L3      unbound
    -1,             // R3      unbound
    2,              // Select  -> libretro Select Left softkey
    3,              // Start   -> libretro Start  Right softkey
    4,              // D-pad up
    5,              // D-pad down
    6,              // D-pad left
    7,              // D-pad right
    -1, -1, -1, -1, // Left stick
    -1, -1, -1, -1, // Right stick
};

COREDEF_CORE_MAP_FROZEN(freej2me, "freej2me", options, source_target);
