#include <string.h>
#include "../common/common.h"
#include "lookup.h"

const LookupName lookup_z_table[] = {
        {"zaryavos",   "Zarya Vostoka"},
        {"zarzon",     "Zarzon"},
        {"zaviga",     "Zaviga"},
        {"zavigaj",    "Zaviga (Japan)"},
        {"zaxxon",     "Zaxxon (set 1)"},
        {"zaxxon2",    "Zaxxon (set 2)"},
        {"zaxxon3",    "Zaxxon (set 3, unknown rev)"},
        {"zaxxonb",    "Jackson"},
        {"zaxxonj",    "Zaxxon (Japan)"},
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
        {"zerohoura",  "Zero Hour (set 2)"},
        {"zerohouri",  "Zero Hour (bootleg)"},
        {"zeropnt",    "Zero Point (set 1)"},
        {"zeropnt2",   "Zero Point 2"},
        {"zeropnta",   "Zero Point (set 2)"},
        {"zeropntj",   "Zero Point (Japan)"},
        {"zeroteam",   "Zero Team USA (US)"},
        {"zeroteama",  "Zero Team (Japan?, earlier?, set 1)"},
        {"zeroteamb",  "Zero Team (Japan?, later batteryless)"},
        {"zeroteamc",  "Zero Team (Taiwan)"},
        {"zeroteamd",  "Zero Team (Korea)"},
        {"zeroteame",  "Zero Team (Japan?, earlier?, set 2)"},
        {"zeroteams",  "Zero Team Selection"},
        {"zeroteamsr", "Zero Team Suicide Revival Kit"},
        {"zerotime",   "Zero Time"},
        {"zerotimea",  "Zero Time (Spanish bootleg, set 1)"},
        {"zerotimeb",  "Zero Time (Spanish bootleg, set 2)"},
        {"zerotimed",  "Zero Time (Datamat)"},
        {"zerotimemc", "Zero Time (Marti Colls)"},
        {"zerotimeu",  "Zero Time (Spanish bootleg)"},
        {"zerotm2k",   "Zero Team 2000"},
        {"zerotrgt",   "Zero Target (World)"},
        {"zerowing",   "Zero Wing"},
        {"zerowing1",  "Zero Wing (1P set)"},
        {"zerowing2",  "Zero Wing (dual players)"},
        {"zerowingw",  "Zero Wing (2P set, Williams license)"},
        {"zerozone",   "Zero Zone"},
        {"zgundm",     "Mobile Suit Z-Gundam: A.E.U.G. vs Titans (ZGA1 Ver. A)"},
        {"zgundmdx",   "Mobile Suit Z-Gundam: A.E.U.G. vs Titans DX (ZDX1 Ver. A)"},
        {"zigzag",     "Zig Zag (Galaxian hardware, set 1)"},
        {"zigzag2",    "Zig Zag (Galaxian hardware, set 2)"},
        {"zigzagb",    "Zig Zag (bootleg Dig Dug conversion on Galaxian hardware, set 1)"},
        {"zigzagb2",   "Zig Zag (bootleg Dig Dug conversion on Galaxian hardware, set 2)"},
        {"zingzip",    "Zing Zing Zip"},
        {"zintrckb",   "Zintrick - Oshidashi Zentrix (Hack-bootleg)"},
        {"zintrkcd",   "Zintrick / Oshidashi Zentrix (Neo CD conversion)"},
        {"zipzap",     "Zip & Zap (Explicit)"},
        {"zipzapa",    "Zip & Zap (Less Explicit)"},
        {"znpwfv",     "Zen Nippon Pro-Wrestling Featuring Virtua"},
        {"zoar",       "Zoar"},
        {"zodiack",    "Zodiack"},
        {"zokuoten",   "Zoku Otenamihaiken (V2.03J)"},
        {"zolapac",    "Super Zola Pac Gal"},
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
        {"zoom909",    "Zoom 909"},
        {"zooo",       "Zooo (V2.01J)"},
        {"zortonbr",   "Zorton Brothers (Los Justicieros)"},
        {"zunkyou",    "Zunzunkyou No Yabou (Japan)"},
        {"zupapa",     "Zupapa!"},
        {"zwackery",   "Zwackery"},
        {"zzyzzyx2",   "Zzyzzyxx (set 2)"},
        {"zzyzzyxx",   "Zzyzzyxx (set 1)"},
        {"zzyzzyxx2",  "Zzyzzyxx (set 2)"},
};

const size_t lookup_z_count = A_SIZE(lookup_z_table);

const char *lookup_z(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < lookup_z_count; i++) {
        if (strcmp(lookup_z_table[i].name, name) == 0) {
            return lookup_z_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_z(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < lookup_z_count; i++) {
        if (strstr(lookup_z_table[i].value, value)) {
            return lookup_z_table[i].name;
        }
    }
    return NULL;
}

void lookup_z_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_z_count; i++) {
        if (strcasestr(lookup_z_table[i].name, term))
            emit(lookup_z_table[i].name, lookup_z_table[i].value, udata);
    }
}

void r_lookup_z_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_z_count; i++) {
        if (strcasestr(lookup_z_table[i].value, term))
            emit(lookup_z_table[i].name, lookup_z_table[i].value, udata);
    }
}
