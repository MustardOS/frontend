#include <string.h>
#include "../common/common.h"
#include "lookup.h"

typedef struct {
    const char *name;
    const char *value;
} LookupName;

static const LookupName lookup_table[] = {
        {"2020bb",     "2020 Super Baseball (set 1)"},
        {"2020bba",    "2020 Super Baseball (set 2)"},
        {"2020bbcd",   "2020 Super Baseball (Neo CD conversion)"},
        {"2020bbh",    "2020 Super Baseball (set 2)"},
        {"20pacgal",   "Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.08)"},
        {"20pacgalr0", "Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.00)"},
        {"20pacgalr1", "Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.01)"},
        {"20pacgalr2", "Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.02)"},
        {"20pacgalr3", "Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.03)"},
        {"20pacgalr4", "Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.04)"},
        {"25pacman",   "Pac-Man - 25th Anniversary Edition (Rev 3.00)"},
        {"25pacmano",  "Pac-Man - 25th Anniversary Edition (Rev 2.00)"},
        {"280zzzap",   "Datsun 280 Zzzap"},
};

const char *lookup_2(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < A_SIZE(lookup_table); i++) {
        if (strcmp(lookup_table[i].name, name) == 0) {
            return lookup_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_2(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < A_SIZE(lookup_table); i++) {
        if (strstr(lookup_table[i].value, value)) {
            return lookup_table[i].name;
        }
    }
    return NULL;
}
