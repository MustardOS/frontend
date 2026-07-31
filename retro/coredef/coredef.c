#include <string.h>
#include "coredef.h"

#define COREDEF(IDENT) COREDEF_DECLARE(IDENT)
COREDEF_LIST
#undef COREDEF

static const struct coredef_core *const registry[] = {
#define COREDEF(IDENT) &coredef_##IDENT,
    COREDEF_LIST
#undef COREDEF
};

const char *coredef_lookup(const char *core_name, const char *key) {
    if (!core_name || !*core_name || !key || !*key) return NULL;

    for (size_t i = 0; i < sizeof(registry) / sizeof(registry[0]); i++) {
        const struct coredef_core *core = registry[i];
        if (strcmp(core->name, core_name) != 0) continue;

        for (size_t o = 0; o < core->count; o++)
            if (strcmp(core->options[o].key, key) == 0) return core->options[o].value;

        return NULL;
    }

    return NULL;
}
