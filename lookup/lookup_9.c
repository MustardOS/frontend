#include <string.h>
#include "../common/common.h"
#include "lookup.h"

typedef struct {
    const char *name;
    const char *value;
} LookupName;

static const LookupName lookup_table[] = {
        {"99lstwar",  "'99: The Last War"},
        {"99lstwara", "'99: The Last War (set 2)"},
        {"99lstwarb", "'99: The Last War (bootleg)"},
        {"99lstwark", "'99: The Last War (Kyugo)"},
        {"99lstwra",  "'99: The Last War (alternate)"},
        {"9ballsh2",  "9-Ball Shootout (set 2)"},
        {"9ballsh3",  "9-Ball Shootout (set 3)"},
        {"9ballsht",  "9-Ball Shootout (set 1)"},
        {"9ballsht2", "9-Ball Shootout (set 2)"},
        {"9ballsht3", "9-Ball Shootout (set 3)"},
        {"9ballshtc", "9-Ball Shootout Championship"},
};

const char *lookup_9(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < A_SIZE(lookup_table); i++) {
        if (strcmp(lookup_table[i].name, name) == 0) {
            return lookup_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_9(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < A_SIZE(lookup_table); i++) {
        if (strstr(lookup_table[i].value, value)) {
            return lookup_table[i].name;
        }
    }
    return NULL;
}
