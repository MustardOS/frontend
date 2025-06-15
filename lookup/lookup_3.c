#include <string.h>
#include "../common/common.h"
#include "lookup.h"

typedef struct {
    const char *name;
    const char *value;
} LookupName;

static const LookupName lookup_table[] = {
        {"39in1",      "39 in 1 MAME bootleg (GNO-V000)"},
        {"3countb",    "3 Count Bout / Fire Suplex (NGM-043 ~ NGH-043)"},
        {"3in1semi",   "New HyperMan (3-in-1 with Cookie & Bibi & HyperMan)"},
        {"3in1semia",  "New HyperMan (3-in-1 with Cookie & Bibi & HyperMan) (set 2)"},
        {"3kokushi",   "Sankokushi (Japan)"},
        {"3on3dunk",   "3 On 3 Dunk Madness (US, prototype? 1997/02/04)"},
        {"3stooges",   "The Three Stooges In Brides Is Brides"},
        {"3stoogesa",  "The Three Stooges In Brides Is Brides (set 2)"},
        {"3wonders",   "Three Wonders (World 910520)"},
        {"3wondersb",  "Three Wonders (bootleg)"},
        {"3wondersh",  "Three Wonders (hack)"},
        {"3wondersr1", "Three Wonders (World 910513)"},
        {"3wondersu",  "Three Wonders (USA 910520)"},
        {"3wonderu",   "Three Wonders (US 910520)"},
        {"3x3puzzl",   "3X3 Puzzle (Enterprise)"},
        {"3x3puzzla",  "3X3 Puzzle (Normal)"},
};

const char *lookup_3(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < A_SIZE(lookup_table); i++) {
        if (strcmp(lookup_table[i].name, name) == 0) {
            return lookup_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_3(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < A_SIZE(lookup_table); i++) {
        if (strstr(lookup_table[i].value, value)) {
            return lookup_table[i].name;
        }
    }
    return NULL;
}
