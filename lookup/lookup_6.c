#include <string.h>
#include "../common/common.h"
#include "lookup.h"

const LookupName lookup_6_table[] = {
        {"600",        "600"},
        {"64streej",   "64th. Street - A Detective Story (Japan)"},
        {"64street",   "64th. Street - A Detective Story (World)"},
        {"64streetj",  "64th. Street - A Detective Story (Japan, set 1)"},
        {"64streetja", "64th. Street - A Detective Story (Japan, set 2)"},
};

const size_t lookup_6_count = A_SIZE(lookup_6_table);

const char *lookup_6(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < lookup_6_count; i++) {
        if (strcmp(lookup_6_table[i].name, name) == 0) {
            return lookup_6_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_6(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < lookup_6_count; i++) {
        if (strstr(lookup_6_table[i].value, value)) {
            return lookup_6_table[i].name;
        }
    }
    return NULL;
}

void lookup_6_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_6_count; i++) {
        if (strcasestr(lookup_6_table[i].name, term))
            emit(lookup_6_table[i].name, lookup_6_table[i].value, udata);
    }
}

void r_lookup_6_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_6_count; i++) {
        if (strcasestr(lookup_6_table[i].value, term))
            emit(lookup_6_table[i].name, lookup_6_table[i].value, udata);
    }
}
