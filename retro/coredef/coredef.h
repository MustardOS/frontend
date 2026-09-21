#pragma once

#include <stddef.h> // IWYU pragma: keep

struct coredef_option {
    const char *key;
    const char *value;
};

// Number of physical control sources a default map has to cover, kept in step with
// PORT_SOURCE_COUNT in retro/settings/settings.h
#define COREDEF_SOURCE_COUNT 24

struct coredef_core {
    const char *name;
    const struct coredef_option *options;
    size_t count;

    // Optional starting control map, one libretro joypad id per physical source index, or -1
    // for unbound. Only for cores whose own assignment is a poor fit for a handheld, such as
    // a phone keypad where the action key lands on Y. NULL leaves the usual map in place.
    const int *source_target;
};

#define COREDEF_CORE(IDENT, NAME, ARR)                                                                                 \
    const struct coredef_core coredef_##IDENT = {NAME, ARR, sizeof(ARR) / sizeof((ARR)[0]), NULL}

#define COREDEF_CORE_MAP(IDENT, NAME, ARR, MAP)                                                                        \
    const struct coredef_core coredef_##IDENT = {NAME, ARR, sizeof(ARR) / sizeof((ARR)[0]), MAP}

#define COREDEF_DECLARE(IDENT) extern const struct coredef_core coredef_##IDENT;

const char *coredef_lookup(const char *core_name, const char *key);

// The core's starting control map, or NULL when it has none and the usual map applies
const int *coredef_source_target(const char *core_name);

// One entry per core file in this directory!
#define COREDEF_LIST                                                                                                   \
    COREDEF(dosbox_pure)                                                                                               \
    COREDEF(duckstation)                                                                                               \
    COREDEF(fbalpha2012_cps3)                                                                                          \
    COREDEF(fbneo)                                                                                                     \
    COREDEF(flycast)                                                                                                   \
    COREDEF(flycastvl)                                                                                                 \
    COREDEF(freej2me)                                                                                                  \
    COREDEF(gambatte)                                                                                                  \
    COREDEF(km_fbneo_xtreme_amped)                                                                                     \
    COREDEF(km_ludicrousn64_2k22_xtreme_amped)                                                                         \
    COREDEF(km_mame2003_xtreme_amped)                                                                                  \
    COREDEF(mame0139)                                                                                                  \
    COREDEF(mame078plus)                                                                                               \
    COREDEF(mame2003)                                                                                                  \
    COREDEF(mame2003_midway)                                                                                           \
    COREDEF(mame2003_plus)                                                                                             \
    COREDEF(mame2010)                                                                                                  \
    COREDEF(mednafen_pce)                                                                                              \
    COREDEF(mednafen_pce_fast)                                                                                         \
    COREDEF(mednafen_psx)                                                                                              \
    COREDEF(mednafen_supergrafx)                                                                                       \
    COREDEF(mesen)                                                                                                     \
    COREDEF(mgba)                                                                                                      \
    COREDEF(mgba_rumble)                                                                                               \
    COREDEF(mupen64plus)                                                                                               \
    COREDEF(mupen64plus_next)                                                                                          \
    COREDEF(np2kai)                                                                                                    \
    COREDEF(parallel_n64)                                                                                              \
    COREDEF(pcsx_rearmed)                                                                                              \
    COREDEF(ppsspp)                                                                                                    \
    COREDEF(sameboy)                                                                                                   \
    COREDEF(scummvm)                                                                                                   \
    COREDEF(swanstation)                                                                                               \
    COREDEF(vice_x128)                                                                                                 \
    COREDEF(vice_x64)                                                                                                  \
    COREDEF(vice_x64dtv)                                                                                               \
    COREDEF(vice_x64sc)                                                                                                \
    COREDEF(vice_xcbm2)                                                                                                \
    COREDEF(vice_xcbm5x0)                                                                                              \
    COREDEF(vice_xpet)                                                                                                 \
    COREDEF(vice_xplus4)                                                                                               \
    COREDEF(vice_xscpu64)                                                                                              \
    COREDEF(vice_xvic)                                                                                                 \
    COREDEF(vbam)
