#include <string.h>
#include "../common/common.h"
#include "lookup.h"

const LookupName lookup_2_table[] = {
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

const size_t lookup_2_count = A_SIZE(lookup_2_table);

const char *lookup_2(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < lookup_2_count; i++) {
        if (strcmp(lookup_2_table[i].name, name) == 0) {
            return lookup_2_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_2(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < lookup_2_count; i++) {
        if (strstr(lookup_2_table[i].value, value)) {
            return lookup_2_table[i].name;
        }
    }
    return NULL;
}

void lookup_2_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_2_count; i++) {
        if (strcasestr(lookup_2_table[i].name, term))
            emit(lookup_2_table[i].name, lookup_2_table[i].value, udata);
    }
}

void r_lookup_2_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_2_count; i++) {
        if (strcasestr(lookup_2_table[i].value, term))
            emit(lookup_2_table[i].name, lookup_2_table[i].value, udata);
    }
}
