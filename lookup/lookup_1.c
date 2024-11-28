#include "lookup.h"
#include <string.h>

typedef struct {
    const char *name;
    const char *value;
} LookupName;

static const LookupName lookup_table[] = {
        {"1941",      "1941 - Counter Attack (World)"},
        {"1942",      "1942 (set 1)"},
        {"1943",      "1943 - The Battle of Midway (US)"},
        {"1944",      "1944: The Loop Master (Euro 000620)"},
        {"10yard",    "10-Yard Fight (World, set 1)"},
        {"10yard85",  "10-Yard Fight '85 (US, Taito license)"},
        {"10yardj",   "10-Yard Fight (Japan)"},
        {"18wheelr",  "18 Wheeler: American Pro Trucker (deluxe, Rev A)"},
        {"18wheelro", "18 Wheeler: American Pro Trucker (deluxe)"},
        {"18wheelrt", "18 Wheeler: American Pro Trucker (deluxe, Rev T)"},
        {"18wheels",  "18 Wheeler: American Pro Trucker (standard)"},
        {"18wheelu",  "18 Wheeler: American Pro Trucker (upright)"},
        {"1941j",     "1941 - Counter Attack (Japan)"},
        {"1941r1",    "1941: Counter Attack (World)"},
        {"1941u",     "1941: Counter Attack (USA 900227)"},
        {"1942a",     "1942 (set 2)"},
        {"1942abl",   "1942 (Revision A, bootleg)"},
        {"1942b",     "1942 (set 3)"},
        {"1942h",     "Supercharger 1942"},
        {"1942iti",   "1942 (Itisa bootleg)"},
        {"1942p",     "1942 (Tecfri PCB, bootleg?)"},
        {"1942w",     "1942 (Williams Electronics license)"},
        {"1943b",     "1943: Battle of Midway (bootleg, hack of Japan set)"},
        {"1943bj",    "1943: Midway Kaisen (bootleg)"},
        {"1943j",     "1943 - The Battle of Midway (Japan)"},
        {"1943ja",    "1943: Midway Kaisen (Japan)"},
        {"1943jah",   "1943: Midway Kaisen (Japan, no protection hack)"},
        {"1943kai",   "1943 Kai - Midway Kaisen"},
        {"1943mii",   "1943 - The Battle of Midway Mark II (US)"},
        {"1943u",     "1943: The Battle of Midway (US, Rev C)"},
        {"1943ua",    "1943: The Battle of Midway (US)"},
        {"1944d",     "1944: The Loop Master (USA 000620 Phoenix Edition) (bootleg)"},
        {"1944j",     "1944: The Loop Master (Japan 000620)"},
        {"1944u",     "1944: The Loop Master (USA 000620)"},
        {"1945kiii",  "1945k III"},
        {"1945kiiin", "1945k III (newer, OPCX1 PCB)"},
        {"1945kiiio", "1945k III (older, OPCX1 PCB)"},
        {"19xx",      "19XX: The War Against Destiny (Euro 960104)"},
        {"19xxa",     "19XX: The War Against Destiny (Asia 960104)"},
        {"19xxar1",   "19XX: The War Against Destiny (Asia 951207)"},
        {"19xxb",     "19XX: The War Against Destiny (Brazil 951218)"},
        {"19xxd",     "19XX: The War Against Destiny (USA 951207 Phoenix Edition) (bootleg)"},
        {"19xxh",     "19XX: The War Against Destiny (Hispanic 951218)"},
        {"19xxj",     "19XX: The War Against Destiny (Japan 960104, yellow case)"},
        {"19xxjr1",   "19XX: The War Against Destiny (Japan 951225)"},
        {"19xxjr2",   "19XX: The War Against Destiny (Japan 951207)"},
        {"19xxu",     "19XX: The War Against Destiny (USA 951207)"},
        {"19yy",      "19YY"},
};

const char *lookup_1(const char *name) {
    for (size_t i = 0; i < sizeof(lookup_table) / sizeof(lookup_table[0]); i++) {
        if (strcmp(lookup_table[i].name, name) == 0) {
            return lookup_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_1(const char *value) {
    for (size_t i = 0; i < sizeof(lookup_table) / sizeof(lookup_table[0]); i++) {
        if (strcmp(lookup_table[i].value, value) == 0) {
            return lookup_table[i].name;
        }
    }
    return NULL;
}
