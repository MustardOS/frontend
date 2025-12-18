#include <string.h>
#include "../common/common.h"
#include "lookup.h"

const LookupName lookup_i_table[] = {
        {"ibara",       "Ibara (2005/03/22 MASTER VER.., '06. 3. 7 ver.)"},
        {"ibarablk",    "Ibara Kuro Black Label (2006/02/06. MASTER VER.)"},
        {"ibarablka",   "Ibara Kuro Black Label (2006/02/06 MASTER VER.)"},
        {"ibarao",      "Ibara (2005/03/22 MASTER VER..)"},
        {"iceclimb",    "Vs. Ice Climber"},
        {"iceclimba",   "Vs. Ice Climber (set IC4-4 ?)"},
        {"iceclimbj",   "Vs. Ice Climber (Japan)"},
        {"iceclmbj",    "Vs. Ice Climber (Japan)"},
        {"iceclmrd",    "Vs. Ice Climber Dual (set IC4-4 A-1)"},
        {"iceclmrj",    "Vs. Ice Climber Dual (Japan)"},
        {"ichir",       "Puzzle & Action: Ichidant-R (World)"},
        {"ichirbl",     "Puzzle & Action: Ichidant-R (World) (bootleg)"},
        {"ichirj",      "Puzzle & Action: Ichidant-R (Japan)"},
        {"ichirjbl",    "Puzzle & Action: Ichidant-R (Japan) (bootleg)"},
        {"ichirk",      "Puzzle & Action: Ichidant-R (Korea)"},
        {"idhimitu",    "Idol no Himitsu [BET] (Japan 890304)"},
        {"idolmj",      "Idol-Mahjong Housoukyoku (Japan)"},
        {"idsoccer",    "Indoor Soccer"},
        {"idsoccera",   "Indoor Soccer (set 2)"},
        {"iemoto",      "Iemoto (Japan 871020)"},
        {"iemotom",     "Iemoto [BET] (Japan 871118)"},
        {"iganinju",    "Iga Ninjyutsuden (Japan)"},
        {"iganinjub",   "Iga Ninjyutsuden (Japan, bootleg)"},
        {"igmo",        "IGMO"},
        {"igstet341",   "Tetris (v341R)"},
        {"igstet342",   "Tetris (v342R)"},
        {"ikari",       "Ikari Warriors (US)"},
        {"ikari3",      "Ikari III - The Rescue (US, Rotary Joystick)"},
        {"ikari3j",     "Ikari Three (Japan, Rotary Joystick)"},
        {"ikari3k",     "Ikari Three (Korea, 8-Way Joystick)"},
        {"ikari3nr",    "Ikari III - The Rescue (World, 8-Way Joystick)"},
        {"ikari3u",     "Ikari III - The Rescue (US, Rotary Joystick)"},
        {"ikari3w",     "Ikari III - The Rescue (World, Rotary Joystick)"},
        {"ikaria",      "Ikari Warriors (US, set 1)"},
        {"ikaria2",     "Ikari Warriors (US, set 2)"},
        {"ikarijp",     "Ikari (Japan)"},
        {"ikarijpb",    "Ikari (joystick hack bootleg)"},
        {"ikarinc",     "Ikari Warriors (US No Continues)"},
        {"ikariram",    "Rambo 3 (bootleg of Ikari, Joystick hack)"},
        {"ikaruga",     "Ikaruga (GDL-0010)"},
        {"ikki",        "Ikki (Japan)"},
        {"illvelo",     "Illvelo (Illmatic Envelope) (Japan)"},
        {"imago",       "Imago (cocktail set)"},
        {"imagoa",      "Imago (no cocktail set)"},
        {"imekura",     "Imekura Mahjong (Japan)"},
        {"imgfight",    "Image Fight (Japan)"},
        {"imgfightj",   "Image Fight (Japan)"},
        {"imgfightjb",  "Image Fight (Japan, bootleg)"},
        {"imgfighto",   "Image Fight (Japan)"},
        {"imolagp",     "Imola Grand Prix (set 1)"},
        {"imolagpo",    "Imola Grand Prix (set 2)"},
        {"imsorry",     "I'm Sorry (US)"},
        {"imsorryj",    "Gonbee no I'm Sorry (Japan)"},
        {"inca",        "Inca"},
        {"indianbt",    "Indian Battle"},
        {"indiandr",    "Indian Dreaming (B - 15/12/98, Local)"},
        {"indy500",     "INDY 500 Twin (Revision A, Newer)"},
        {"indy500d",    "INDY 500 Deluxe (Revision A)"},
        {"indy500to",   "INDY 500 Twin (Revision A)"},
        {"indyheat",    "Danny Sullivan's Indy Heat"},
        {"indytem2",    "Indiana Jones and the Temple of Doom (set 2)"},
        {"indytem3",    "Indiana Jones and the Temple of Doom (set 3)"},
        {"indytem4",    "Indiana Jones and the Temple of Doom (set 4)"},
        {"indytemd",    "Indiana Jones and the Temple of Doom (German)"},
        {"indytemp",    "Indiana Jones and the Temple of Doom (set 1)"},
        {"indytemp2",   "Indiana Jones and the Temple of Doom (set 2)"},
        {"indytemp3",   "Indiana Jones and the Temple of Doom (set 3)"},
        {"indytemp4",   "Indiana Jones and the Temple of Doom (set 4)"},
        {"indytempc",   "Indiana Jones and the Temple of Doom (Cocktail)"},
        {"indytempd",   "Indiana Jones and the Temple of Doom (German)"},
        {"inferno",     "Inferno (Williams)"},
        {"inidv3ca",    "Initial D Arcade Stage Ver. 3 Cycraft Edition (Export, Rev A) (GDS-0039A)"},
        {"inidv3cy",    "Initial D Arcade Stage Ver. 3 Cycraft Edition (Export, Rev B) (GDS-0039B)"},
        {"initd",       "Initial D Arcade Stage (Japan, Rev B) (GDS-0020B)"},
        {"initdexp",    "Initial D Arcade Stage (Export, Rev A) (GDS-0025A)"},
        {"initdexpo",   "Initial D Arcade Stage (Export) (GDS-0025)"},
        {"initdo",      "Initial D Arcade Stage (Japan) (GDS-0020)"},
        {"initdv2e",    "Initial D Arcade Stage Ver. 2 (Export) (GDS-0027)"},
        {"initdv2j",    "Initial D Arcade Stage Ver. 2 (Japan, Rev B) (GDS-0026B)"},
        {"initdv2ja",   "Initial D Arcade Stage Ver. 2 (Japan, Rev A) (GDS-0026A)"},
        {"initdv2jo",   "Initial D Arcade Stage Ver. 2 (Japan) (GDS-0026)"},
        {"initdv3e",    "Initial D Arcade Stage Ver. 3 (Export) (GDS-0033)"},
        {"initdv3j",    "Initial D Arcade Stage Ver. 3 (Japan, Rev C) (GDS-0032C)"},
        {"initdv3jb",   "Initial D Arcade Stage Ver. 3 (Japan, Rev B) (GDS-0032B)"},
        {"inquiztr",    "Inquizitor"},
        {"insector",    "Insector (prototype)"},
        {"insectx",     "Insector X (World)"},
        {"insectxbl",   "Insector X (bootleg)"},
        {"insectxj",    "Insector X (Japan)"},
        {"intcup94",    "International Cup '94"},
        {"inthunt",     "In The Hunt (World)"},
        {"inthuntu",    "In The Hunt (US)"},
        {"intlaser",    "International Team Laser (prototype)"},
        {"intrepi2",    "Intrepid (set 2)"},
        {"intrepid",    "Intrepid (set 1)"},
        {"intrepid2",   "Intrepid (set 2)"},
        {"intrepidb",   "Intrepid (Elsys bootleg, set 1)"},
        {"intrepidb2",  "Intrepid (Loris bootleg)"},
        {"intrepidb3",  "Intrepid (Elsys bootleg, set 2)"},
        {"intrgirl",    "Intergirl"},
        {"introdon",    "Karaoke Quiz Intro Don Don!"},
        {"intrscti",    "Intersecti"},
        {"intruder",    "Intruder"},
        {"inufuku",     "Quiz and Variety Sukusuku Inufuku (Japan)"},
        {"invad2ct",    "Space Invaders II (Midway, cocktail)"},
        {"invaddlx",    "Space Invaders Deluxe"},
        {"invader4",    "Space Invaders Part Four (bootleg of Space Attack II)"},
        {"invaderl",    "Space Invaders (Logitec)"},
        {"invaders",    "Space Invaders"},
        {"invadersem",  "Space Invaders (Electromar, Spanish)"},
        {"invadpt2",    "Space Invaders Part II (Taito)"},
        {"invadrmr",    "Space Invaders (Model Racing)"},
        {"invasioa",    "Invasion (bootleg, set 1, normal graphics)"},
        {"invasiob",    "Invasion (bootleg, set 2, no copyright)"},
        {"invasion",    "Invasion (Sidam)"},
        {"invasiona",   "UFO Robot Attack (bootleg of Invasion, newer set)"},
        {"invasiona2",  "UFO Robot Attack (bootleg of Invasion, older set)"},
        {"invasionb",   "Invasion (Italian bootleg)"},
        {"invasionrz",  "Invasion (bootleg set 1, R Z SRL Bologna)"},
        {"invasionrza", "Invasion (bootleg, set 4, R Z SRL Bologna)"},
        {"invasnab",    "Invasion - The Abductors (version 5.0)"},
        {"invasnab3",   "Invasion - The Abductors (version 3.0)"},
        {"invasnab4",   "Invasion - The Abductors (version 4.0)"},
        {"invasnv4",    "Invasion - The Abductors (version 4.0)"},
        {"invds",       "Invinco - Deep Scan"},
        {"invho2",      "Invinco - Head On 2"},
        {"invinco",     "Invinco"},
        {"invmulti",    "Space Invaders Multigame (M8.03D)"},
        {"invmultim1a", "Space Invaders Multigame (M8.01A)"},
        {"invmultim2a", "Space Invaders Multigame (M8.02A)"},
        {"invmultim2c", "Space Invaders Multigame (M8.02C)"},
        {"invmultim3a", "Space Invaders Multigame (M8.03A)"},
        {"invmultip",   "Space Invaders Multigame (prototype)"},
        {"invmultis1a", "Space Invaders Multigame (S0.81A)"},
        {"invmultis2a", "Space Invaders Multigame (S0.82A)"},
        {"invmultis3a", "Space Invaders Multigame (S0.83A)"},
        {"invmultit3d", "Space Invaders Multigame (T8.03D)"},
        {"invqix",      "Space Invaders / Qix Silver Anniversary Edition (Ver. 2.03)"},
        {"invrvnga",    "Invader's Revenge (Dutchford)"},
        {"invrvnge",    "Invader's Revenge"},
        {"invrvngea",   "Invader's Revenge (set 2)"},
        {"invrvngeb",   "Invader's Revenge (set 3)"},
        {"invrvngedu",  "Invader's Revenge (Dutchford, single PCB)"},
        {"inyourfa",    "In Your Face (US, prototype)"},
        {"ipminvad",    "IPM Invader"},
        {"ipminvad1",   "IPM Invader (Incomplete Dump)"},
        {"ippatsu",     "Ippatsu Gyakuten [BET] (Japan)"},
        {"iqblock",     "IQ-Block"},
        {"iqblocka",    "Shuzi Leyuan (China, V127M, gambling)"},
        {"iqblockf",    "IQ Block (V113FR, gambling)"},
        {"iqpipe",      "IQ Pipe"},
        {"irion",       "Irion"},
        {"irnclado",    "Ironclad (bootleg)"},
        {"irobot",      "I, Robot"},
        {"iron",        "Iron (SNES bootleg)"},
        {"ironclad",    "Choutetsu Brikin'ger - Ironclad (Prototype)"},
        {"ironclado",   "Choutetsu Brikin'ger / Iron Clad (prototype, bootleg)"},
        {"ironfort",    "Iron Fortress"},
        {"ironfortc",   "Gongtit Jiucoi Iron Fortress (Hong Kong)"},
        {"ironfortj",   "Iron Fortress (Japan)"},
        {"ironhors",    "Iron Horse"},
        {"ironhorsh",   "Iron Horse (version H)"},
        {"irrmaze",     "The Irritating Maze / Ultra Denryu Iraira Bou"},
        {"istellar",    "Interstellar Laser Fantasy"},
        {"istreb",      "Istrebiteli"},
        {"itaten",      "Itazura Tenshi (Japan)"},
        {"ixion",       "Ixion (prototype)"},
};

const size_t lookup_i_count = A_SIZE(lookup_i_table);

const char *lookup_i(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < lookup_i_count; i++) {
        if (strcmp(lookup_i_table[i].name, name) == 0) {
            return lookup_i_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_i(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < lookup_i_count; i++) {
        if (strstr(lookup_i_table[i].value, value)) {
            return lookup_i_table[i].name;
        }
    }
    return NULL;
}

void lookup_i_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_i_count; i++) {
        if (strcasestr(lookup_i_table[i].name, term))
            emit(lookup_i_table[i].name, lookup_i_table[i].value, udata);
    }
}

void r_lookup_i_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_i_count; i++) {
        if (strcasestr(lookup_i_table[i].value, term))
            emit(lookup_i_table[i].name, lookup_i_table[i].value, udata);
    }
}
