#include <string.h>
#include "lookup.h"

typedef struct {
    const char *name;
    const char *value;
} LookupName;

static const LookupName lookup_table[] = {
        {"800fath",   "800 Fathoms"},
        {"800fatha",  "800 Fathoms (older)"},
        {"88games",   "'88 Games"},
        {"8ball",     "Video Eight Ball"},
        {"8ball1",    "Video Eight Ball (Rev.1)"},
        {"8ballact",  "Eight Ball Action (DK conversion)"},
        {"8ballact2", "Eight Ball Action (DKJr conversion)"},
        {"8ballat2",  "Eight Ball Action (DKJr conversion)"},
        {"8bpm",      "Eight Ball Action (Pac-Man conversion)"},
};

const char *lookup_8(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < sizeof(lookup_table) / sizeof(lookup_table[0]); i++) {
        if (strcmp(lookup_table[i].name, name) == 0) {
            return lookup_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_8(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < sizeof(lookup_table) / sizeof(lookup_table[0]); i++) {
        if (strstr(lookup_table[i].value, value)) {
            return lookup_table[i].name;
        }
    }
    return NULL;
}
