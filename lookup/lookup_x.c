#include <string.h>
#include "../common/common.h"
#include "lookup.h"

typedef struct {
    const char *name;
    const char *value;
} LookupName;

static const LookupName lookup_table[] = {
        {"xday2",       "X-Day 2 (Japan)"},
        {"xeno",        "Xeno Crisis (Neo Geo MVS)"},
        {"xenophob",    "Xenophobe"},
        {"xevi3dg",     "Xevious 3D-G (XV31-VER.A)"},
        {"xevi3dga",    "Xevious 3D/G (World, XV32/VER.A)"},
        {"xevi3dgj",    "Xevious 3D/G (Japan, XV31/VER.A)"},
        {"xevios",      "Xevios"},
        {"xevious",     "Xevious (Namco)"},
        {"xeviousa",    "Xevious (Atari set 1)"},
        {"xeviousb",    "Xevious (Atari set 2)"},
        {"xeviousc",    "Xevious (Atari set 3)"},
        {"xexex",       "Xexex (World)"},
        {"xexexa",      "Xexex (ver AAA)"},
        {"xexexj",      "Xexex (Japan)"},
        {"xfiles",      "X-Files"},
        {"xfilesk",     "The X-Files (Censored, Korea)"},
        {"xmcota",      "X-Men: Children of the Atom (Euro 950331)"},
        {"xmcotaa",     "X-Men: Children of the Atom (Asia 950105)"},
        {"xmcotaar1",   "X-Men: Children of the Atom (Asia 941219)"},
        {"xmcotaar2",   "X-Men: Children of the Atom (Asia 941217)"},
        {"xmcotab",     "X-Men: Children of the Atom (Brazil 950331)"},
        {"xmcotah",     "X-Men: Children of the Atom (Hispanic 950331)"},
        {"xmcotahr1",   "X-Men: Children of the Atom (Hispanic 950105)"},
        {"xmcotaj",     "X-Men: Children of the Atom (Japan 950105)"},
        {"xmcotaj1",    "X-Men: Children of the Atom (Japan 941222)"},
        {"xmcotaj2",    "X-Men: Children of the Atom (Japan 941219)"},
        {"xmcotaj3",    "X-Men: Children of the Atom (Japan 941217)"},
        {"xmcotajr",    "X-Men: Children of the Atom (Japan 941208 rent version)"},
        {"xmcotar1",    "X-Men: Children of the Atom (Euro 950105)"},
        {"xmcotar1d",   "X-Men: Children of the Atom (Europe 950105 Phoenix Edition) (bootleg)"},
        {"xmcotau",     "X-Men: Children of the Atom (USA 950105)"},
        {"xmen",        "X-Men (US 4 Players)"},
        {"xmen2p",      "X-Men (World 2 Players)"},
        {"xmen2pa",     "X-Men (2 Players ver AAA)"},
        {"xmen2pe",     "X-Men (2 Players ver EAA)"},
        {"xmen2pj",     "X-Men (Japan 2 Players)"},
        {"xmen2pu",     "X-Men (2 Players ver UAB)"},
        {"xmen6p",      "X-Men (Euro 6 Players)"},
        {"xmen6pu",     "X-Men (US 6 Players)"},
        {"xmena",       "X-Men (4 Players ver AEA)"},
        {"xmenaa",      "X-Men (4 Players ver ADA)"},
        {"xmenj",       "X-Men (4 Players ver JBA)"},
        {"xmenja",      "X-Men (4 Players ver JEA)"},
        {"xmenu",       "X-Men (4 Players ver UBB)"},
        {"xmenua",      "X-Men (4 Players ver UEB)"},
        {"xmultipl",    "X Multiply (Japan)"},
        {"xmultiplm72", "X Multiply (Japan, M72 hardware)"},
        {"xmvsf",       "X-Men Vs. Street Fighter (Euro 961004)"},
        {"xmvsfa",      "X-Men Vs. Street Fighter (Asia 961023)"},
        {"xmvsfar1",    "X-Men Vs. Street Fighter (Asia 961004)"},
        {"xmvsfar2",    "X-Men Vs. Street Fighter (Asia 960919)"},
        {"xmvsfar3",    "X-Men Vs. Street Fighter (Asia 960910)"},
        {"xmvsfb",      "X-Men Vs. Street Fighter (Brazil 961023)"},
        {"xmvsfh",      "X-Men Vs. Street Fighter (Hispanic 961004)"},
        {"xmvsfj",      "X-Men Vs. Street Fighter (Japan 961023)"},
        {"xmvsfjr1",    "X-Men Vs. Street Fighter (Japan 961004)"},
        {"xmvsfjr2",    "X-Men Vs. Street Fighter (Japan 960910)"},
        {"xmvsfjr3",    "X-Men Vs. Street Fighter (Japan 960909)"},
        {"xmvsfr1",     "X-Men Vs. Street Fighter (Euro 960910)"},
        {"xmvsfu",      "X-Men Vs. Street Fighter (USA 961023)"},
        {"xmvsfu1d",    "X-Men Vs. Street Fighter (USA 961004 Phoenix Edition) (bootleg)"},
        {"xmvsfur1",    "X-Men Vs. Street Fighter (USA 961004)"},
        {"xmvsfur2",    "X-Men Vs. Street Fighter (USA 960910)"},
        {"xorworld",    "Xor World (prototype)"},
        {"xsleena",     "Xain'd Sleena"},
        {"xsleenab",    "Xain'd Sleena (bootleg)"},
        {"xsleenaba",   "Xain'd Sleena (bootleg, bugfixed)"},
        {"xsleenabb",   "Xain'd Sleena (bootleg, set 2)"},
        {"xsleenaj",    "Xain'd Sleena (Japan)"},
        {"xtheball",    "X the Ball"},
        {"xtrmhnt2",    "Extreme Hunting 2"},
        {"xtrmhunt",    "Extreme Hunting"},
        {"xxmissio",    "XX Mission"},
        {"xybots",      "Xybots (rev 2)"},
        {"xybots0",     "Xybots (rev 0)"},
        {"xybots1",     "Xybots (rev 1)"},
        {"xybotsf",     "Xybots (French, rev 3)"},
        {"xybotsg",     "Xybots (German, rev 3)"},
        {"xyonix",      "Xyonix"},
};

const char *lookup_x(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < A_SIZE(lookup_table); i++) {
        if (strcmp(lookup_table[i].name, name) == 0) {
            return lookup_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_x(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < A_SIZE(lookup_table); i++) {
        if (strstr(lookup_table[i].value, value)) {
            return lookup_table[i].name;
        }
    }
    return NULL;
}
