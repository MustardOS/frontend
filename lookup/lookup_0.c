#include <string.h>
#include "../common/common.h"
#include "lookup.h"

const LookupName lookup_0_table[] = {
        {"005", "005"},
};

const size_t lookup_0_count = A_SIZE(lookup_0_table);

const char *lookup_0(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < lookup_0_count; i++) {
        if (strcmp(lookup_0_table[i].name, name) == 0) {
            return lookup_0_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_0(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < lookup_0_count; i++) {
        if (strstr(lookup_0_table[i].value, value)) {
            return lookup_0_table[i].name;
        }
    }
    return NULL;
}

void lookup_0_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_0_count; i++) {
        if (strcasestr(lookup_0_table[i].name, term))
            emit(lookup_0_table[i].name, lookup_0_table[i].value, udata);
    }
}

void r_lookup_0_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_0_count; i++) {
        if (strcasestr(lookup_0_table[i].value, term))
            emit(lookup_0_table[i].name, lookup_0_table[i].value, udata);
    }
}
