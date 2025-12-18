#include <string.h>
#include "../common/common.h"
#include "lookup.h"

const LookupName lookup_4_table[] = {
        {"40love",   "Forty-Love"},
        {"40lovebl", "Forty-Love (bootleg)"},
        {"40lovej",  "Forty-Love (Japan)"},
        {"47pie2",   "Idol Janshi Su-Chi-Pie 2 (v1.1)"},
        {"47pie2o",  "Idol Janshi Su-Chi-Pie 2 (v1.0)"},
        {"48in1",    "48 in 1 MAME bootleg (ver 3.09)"},
        {"48in1a",   "48 in 1 MAME bootleg (ver 3.02)"},
        {"4dwarrio", "4-D Warriors"},
        {"4enraya",  "4 En Raya"},
        {"4enrayaa", "4 En Raya (set 2)"},
        {"4in1",     "4 Fun in 1"},
        {"4in1boot", "Puzzle King (PacMan 2 with Tetris & HyperMan 2 & Snow Bros"},
        {"4play",    "4 player input test"},
        {"4psimasy", "Mahjong 4P Simasyo (Japan)"},
};

const size_t lookup_4_count = A_SIZE(lookup_4_table);

const char *lookup_4(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < lookup_4_count; i++) {
        if (strcmp(lookup_4_table[i].name, name) == 0)
            return lookup_4_table[i].value;
    }
    return NULL;
}

const char *r_lookup_4(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < lookup_4_count; i++) {
        if (strstr(lookup_4_table[i].value, value))
            return lookup_4_table[i].name;
    }
    return NULL;
}

void lookup_4_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_4_count; i++) {
        if (strcasestr(lookup_4_table[i].name, term))
            emit(lookup_4_table[i].name, lookup_4_table[i].value, udata);
    }
}

void r_lookup_4_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_4_count; i++) {
        if (strcasestr(lookup_4_table[i].value, term))
            emit(lookup_4_table[i].name, lookup_4_table[i].value, udata);
    }
}
