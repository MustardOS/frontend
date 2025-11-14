#include <string.h>
#include "../common/common.h"
#include "lookup.h"

const LookupName lookup_7_table[] = {
        {"720",     "720 Degrees (rev 4)"},
        {"720b",    "720 Degrees (set 2)"},
        {"720g",    "720 Degrees (German, rev 2)"},
        {"720gr1",  "720 Degrees (German, rev 1)"},
        {"720r1",   "720 Degrees (rev 1)"},
        {"720r2",   "720 Degrees (rev 2)"},
        {"720r3",   "720 Degrees (rev 3)"},
        {"7jigen",  "7jigen no Youseitachi - Mahjong 7 Dimensions (Japan)"},
        {"7mezzo",  "7 e Mezzo"},
        {"7toitsu", "Chi-Toitsu"},
};

const size_t lookup_7_count = A_SIZE(lookup_7_table);

const char *lookup_7(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < lookup_7_count; i++) {
        if (strcmp(lookup_7_table[i].name, name) == 0) {
            return lookup_7_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_7(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < lookup_7_count; i++) {
        if (strstr(lookup_7_table[i].value, value)) {
            return lookup_7_table[i].name;
        }
    }
    return NULL;
}

void lookup_7_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_7_count; i++) {
        if (strcasestr(lookup_7_table[i].name, term))
            emit(lookup_7_table[i].name, lookup_7_table[i].value, udata);
    }
}

void r_lookup_7_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_7_count; i++) {
        if (strcasestr(lookup_7_table[i].value, term))
            emit(lookup_7_table[i].name, lookup_7_table[i].value, udata);
    }
}
