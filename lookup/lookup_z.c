#include <string.h>
#include "lookup.h"

typedef struct {
    const char *name;
    const char *value;
} LookupName;

static const LookupName lookup_table[] = {
        {"zaryavos",   "Zarya Vostoka"},
        {"zarzon",     "Zarzon"},
        {"zaviga",     "Zaviga"},
        {"zavigaj",    "Zaviga (Japan)"},
        {"zaxxon",     "Zaxxon (set 1)"},
        {"zaxxon2",    "Zaxxon (set 2)"},
        {"zaxxonb",    "Jackson"},
        {"zedblade",   "Zed Blade / Operation Ragnarok"},
        {"zektor",     "Zektor (revision B)"},
        {"zero",       "Zero (bootleg of Defender, set 1)"},
        {"zero2",      "Zero (bootleg of Defender, set 2)"},
        {"zerogu2",    "Zero Gunner 2"},
        {"zerogun",    "Zero Gunner (Export, Model 2B)"},
        {"zeroguna",   "Zero Gunner (Export, Model 2A)"},
        {"zerogunaj",  "Zero Gunner (Japan, Model 2A)"},
        {"zerogunj",   "Zero Gunner (Japan, Model 2B)"},
        {"zerohour",   "Zero Hour"},
        {"zeropnt",    "Zero Point (set 1)"},
        {"zeropnt2",   "Zero Point 2"},
        {"zeropnta",   "Zero Point (set 2)"},
        {"zeropntj",   "Zero Point (Japan)"},
        {"zerotime",   "Zero Time"},
        {"zerotimea",  "Zero Time (Spanish bootleg, set 1)"},
        {"zerotimeb",  "Zero Time (Spanish bootleg, set 2)"},
        {"zerotimed",  "Zero Time (Datamat)"},
        {"zerotimemc", "Zero Time (Marti Colls)"},
        {"zerotm2k",   "Zero Team 2000"},
        {"zerowing",   "Zero Wing"},
        {"zerowing1",  "Zero Wing (1P set)"},
        {"zerowingw",  "Zero Wing (2P set, Williams license)"},
        {"zerozone",   "Zero Zone"},
        {"zigzag",     "Zig Zag (Galaxian hardware, set 1)"},
        {"zigzag2",    "Zig Zag (Galaxian hardware, set 2)"},
        {"zigzagb",    "Zig Zag (bootleg Dig Dug conversion on Galaxian hardware, set 1)"},
        {"zigzagb2",   "Zig Zag (bootleg Dig Dug conversion on Galaxian hardware, set 2)"},
        {"zingzip",    "Zing Zing Zip"},
        {"zintrckb",   "Zintrick - Oshidashi Zentrix (Hack-bootleg)"},
        {"zipzap",     "Zip & Zap (Explicit)"},
        {"zipzapa",    "Zip & Zap (Less Explicit)"},
        {"znpwfv",     "Zen Nippon Pro-Wrestling Featuring Virtua"},
        {"zoar",       "Zoar"},
        {"zodiack",    "Zodiack"},
        {"zombraid",   "Zombie Raid (US)"},
        {"zombraidp",  "Zombie Raid (9/28/95, US, prototype PCB)"},
        {"zombraidpj", "Zombie Raid (9/28/95, Japan, prototype PCB)"},
        {"zombrvn",    "Zombie Revenge (Rev A)"},
        {"zombrvne",   "Zombie Revenge (Export)"},
        {"zombrvno",   "Zombie Revenge"},
        {"zookeep",    "Zoo Keeper (set 1)"},
        {"zookeep2",   "Zoo Keeper (set 2)"},
        {"zookeep3",   "Zoo Keeper (set 3)"},
        {"zookeepbl",  "Zoo Keeper (bootleg)"},
        {"zunkyou",    "Zunzunkyou No Yabou (Japan)"},
        {"zupapa",     "Zupapa!"},
        {"zwackery",   "Zwackery"},
        {"zzyzzyx2",   "Zzyzzyxx (set 2)"},
        {"zzyzzyxx",   "Zzyzzyxx (set 1)"},
        {"zzyzzyxx2",  "Zzyzzyxx (set 2)"},
};

const char *lookup_z(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < sizeof(lookup_table) / sizeof(lookup_table[0]); i++) {
        if (strcmp(lookup_table[i].name, name) == 0) {
            return lookup_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_z(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < sizeof(lookup_table) / sizeof(lookup_table[0]); i++) {
        if (strstr(lookup_table[i].value, value)) {
            return lookup_table[i].name;
        }
    }
    return NULL;
}
