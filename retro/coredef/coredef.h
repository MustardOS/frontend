#pragma once

#include <stddef.h> // IWYU pragma: keep

struct coredef_option {
    const char *key;
    const char *value;
};

struct coredef_core {
    const char *name;
    const struct coredef_option *options;
    size_t count;
};

#define COREDEF_CORE(IDENT, NAME, ARR)                                                                                 \
    const struct coredef_core coredef_##IDENT = {NAME, ARR, sizeof(ARR) / sizeof((ARR)[0])}

#define COREDEF_DECLARE(IDENT) extern const struct coredef_core coredef_##IDENT;

const char *coredef_lookup(const char *core_name, const char *key);

// One entry per core file in this directory!
#define COREDEF_LIST                                                                                                   \
    COREDEF(gambatte)                                                                                                  \
    COREDEF(mednafen_pce)                                                                                              \
    COREDEF(mednafen_pce_fast)                                                                                         \
    COREDEF(mednafen_supergrafx)                                                                                       \
    COREDEF(mupen64plus_next)                                                                                          \
    COREDEF(sameboy)
