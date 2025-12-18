#include <string.h>
#include "../common/common.h"
#include "lookup.h"

const LookupName lookup_5_table[] = {
        {"500gp", "500 GP (5GP3 Ver. C)"},
};

const size_t lookup_5_count = A_SIZE(lookup_5_table);

const char *lookup_5(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < lookup_5_count; i++) {
        if (strcmp(lookup_5_table[i].name, name) == 0) {
            return lookup_5_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_5(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < lookup_5_count; i++) {
        if (strstr(lookup_5_table[i].value, value)) {
            return lookup_5_table[i].name;
        }
    }
    return NULL;
}

void lookup_5_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_5_count; i++) {
        if (strcasestr(lookup_5_table[i].name, term))
            emit(lookup_5_table[i].name, lookup_5_table[i].value, udata);
    }
}

void r_lookup_5_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_5_count; i++) {
        if (strcasestr(lookup_5_table[i].value, term))
            emit(lookup_5_table[i].name, lookup_5_table[i].value, udata);
    }
}
