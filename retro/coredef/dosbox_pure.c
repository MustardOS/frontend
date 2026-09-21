#include "coredef.h"

// Auto only reaches for the recompiler on protected mode content and leaves real mode
// games on the interpreter. Asking for it everywhere is the faster choice on this hardware.
static const struct coredef_option options[] = {
    {.key = "dosbox_pure_cpu_core", .value = "dynamic"},
    {.key = "dosbox_pure_audiorate", .value = "44100"},
};

COREDEF_CORE(dosbox_pure, "dosbox_pure", options);
