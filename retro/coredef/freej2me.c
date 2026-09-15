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
 * the box the action key OK/Fire sits on Y while A dials the number nine. Only four entries move
 * from the usual map, and they move in pairs so the result reads the same in either direction:
 *
 *   A  <-> OK/Fire   (was Num 9)
 *   B  <-> CLR       (was Num 7)
 *   Y  <-> Num 9     (was OK/Fire)
 *   R3 <-> Num 7     (was CLR)
 *
 * Everything else keeps the core's own assignment: X is Num 0, the shoulders are 1 and 3, the
 * triggers are star and hash, L3 is Num 5 and the pointer press, Select and Start are the two
 * softkeys, and the left stick covers 2, 4, 6 and 8.
 */
static const int source_target[COREDEF_SOURCE_COUNT] = {
    1,  // A       -> libretro Y      OK/Fire
    15, // B       -> libretro R3     CLR
    9,  // X       -> libretro X      Num 0
    8,  // Y       -> libretro A      Num 9
    10, // L1      -> libretro L      Num 1
    11, // R1      -> libretro R      Num 3
    12, // L2      -> libretro L2     Num *
    13, // R2      -> libretro R2     Num #
    14, // L3      -> libretro L3     Num 5 and pointer press
    0,  // R3      -> libretro B      Num 7
    2,  // Select  -> libretro Select Left softkey
    3,  // Start   -> libretro Start  Right softkey
    4,  // D-pad up
    5,  // D-pad down
    6,  // D-pad left
    7,  // D-pad right
    -1, -1, -1, -1, // Left stick, read by the core as its own analog axes for 2, 4, 6 and 8
    -1, -1, -1, -1, // Right stick, read by the core as the pointer
};

COREDEF_CORE_MAP(freej2me, "freej2me", options, source_target);
