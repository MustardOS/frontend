#include <string.h>
#include "lookup.h"

typedef struct {
    const char *name;
    const char *value;
} LookupName;

static const LookupName lookup_table[] = {
        {"uballoon",  "Ultra Balloon"},
        {"uccops",    "Undercover Cops (World)"},
        {"uccopsar",  "Undercover Cops (Alpha Renewal Version)"},
        {"uccopsaru", "Undercover Cops - Alpha Renewal Version (US)"},
        {"uccopsj",   "Undercover Cops (Japan)"},
        {"uccopsu",   "Undercover Cops (US)"},
        {"uchuuai",   "Mahjong Uchuu yori Ai wo komete (Japan)"},
        {"ucytokyu",  "Uchuu Tokkyuu Medalian"},
        {"uecology",  "Ultimate Ecology (Japan 931203)"},
        {"ufosensb",  "Ufo Senshi Yohko Chan (not encrypted)"},
        {"ufosensi",  "Ufo Senshi Yohko Chan"},
        {"ufosensib", "Ufo Senshi Yohko Chan (bootleg, not encrypted)"},
        {"ujlnow",    "Um Jammer Lammy NOW! (Japan, UL1/VER.A)"},
        {"ultennis",  "Ultimate Tennis"},
        {"ultennisj", "Ultimate Tennis (v 1.4, Japan)"},
        {"ultracin",  "Waku Waku Ultraman Racing"},
        {"ultrainv",  "Ultra Invaders"},
        {"ultraman",  "Ultraman (Japan)"},
        {"ultramhm",  "Ultra Maru-hi Mahjong (Japan)"},
        {"ultratnk",  "Ultra Tank"},
        {"ultrax",    "Ultra X Weapons - Ultra Keibitai"},
        {"ultraxg",   "Ultra X Weapons / Ultra Keibitai (Gamest review build)"},
        {"ultrchmp",  "Se Gye Hweng Dan Ultra Champion (Korea)"},
        {"ultrchmph", "Cheng Ba Shi Jie - Chao Shi Kong Guan Jun (Taiwan)"},
        {"umanclub",  "Ultraman Club - Tatakae! Ultraman Kyoudai!!"},
        {"umk3",      "Ultimate Mortal Kombat 3 (rev 1.2)"},
        {"umk3p",     "Ultimate Mortal Kombat 3 Plus (Beta 2)"},
        {"umk3r10",   "Ultimate Mortal Kombat 3 (rev 1.0)"},
        {"umk3r11",   "Ultimate Mortal Kombat 3 (rev 1.1)"},
        {"unclepoo",  "Uncle Poo"},
        {"undefeat",  "Under Defeat (Japan) (GDL-0035)"},
        {"undoukai",  "The Undoukai (Japan)"},
        {"undrfire",  "Under Fire (World)"},
        {"undrfirej", "Under Fire (Japan)"},
        {"undrfireu", "Under Fire (US)"},
        {"undrfirj",  "Under Fire (Japan)"},
        {"undrfiru",  "Under Fire (US)"},
        {"uniwars",   "UniWar S"},
        {"uniwarsa",  "UniWar S (bootleg)"},
        {"unkpacgc",  "Coco Louco"},
        {"unsquad",   "U.N. Squadron (US)"},
        {"untoucha",  "Untouchable (Japan)"},
        {"uopoko",    "Uo Poko (Japan)"},
        {"uopokoj",   "Puzzle Uo Poko (Japan)"},
        {"upndown",   "Up'n Down"},
        {"upndownu",  "Up'n Down (not encrypted)"},
        {"upscope",   "Up Scope"},
        {"upyoural",  "Up Your Alley"},
        {"usclssic",  "U.S. Classic"},
        {"usg182",    "Games V18.2"},
        {"usg185",    "Games V18.7C"},
        {"usg187c",   "Games V18.7C"},
        {"usg211c",   "Games V21.1C"},
        {"usg251",    "Games V25.1"},
        {"usg252",    "Games V25.4X"},
        {"usg32",     "Super Duper Casino (California V3.2)"},
        {"usg82",     "Super Ten V8.2"},
        {"usg83",     "Super Ten V8.3"},
        {"usg83x",    "Super Ten V8.3X"},
        {"usgames",   "Games V25.4X"},
        {"usvsthem",  "Us vs. Them"},
        {"utoukond",  "Ultra Toukon Densetsu (Japan)"},
};

const char *lookup_u(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < sizeof(lookup_table) / sizeof(lookup_table[0]); i++) {
        if (strcmp(lookup_table[i].name, name) == 0) {
            return lookup_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_u(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < sizeof(lookup_table) / sizeof(lookup_table[0]); i++) {
        if (strstr(lookup_table[i].value, value)) {
            return lookup_table[i].name;
        }
    }
    return NULL;
}
