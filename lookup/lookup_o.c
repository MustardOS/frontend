#include <string.h>
#include "../common/common.h"
#include "lookup.h"

const LookupName lookup_o_table[] = {
        {"oceanhun",     "The Ocean Hunter (Japan, Revision A)"},
        {"oceanhuna",    "The Ocean Hunter (Japan)"},
        {"oedfight",     "Oedo Fight (Japan, Bloodshed version)"},
        {"oedfighta",    "Oedo Fight (Japan, Bloodless version)"},
        {"offensiv",     "Offensive (Spanish bootleg of Scramble)"},
        {"officeye",     "Office Yeo In Cheon Ha (version 1.2)"},
        {"offroad",      "Ironman Stewart's Super Off-Road"},
        {"offroad3",     "Ironman Ivan Stewart's Super Off-Road (rev 3)"},
        {"offroadc",     "Off Road Challenge"},
        {"offroadc0",    "Off Road Challenge (v1.00)"},
        {"offroadc1",    "Off Road Challenge (v1.10)"},
        {"offroadc3",    "Off Road Challenge (v1.30)"},
        {"offroadc4",    "Off Road Challenge (v1.40)"},
        {"offroadc5",    "Off Road Challenge (v1.50)"},
        {"offroadt",     "Ironman Stewart's Super Off-Road Track Pack"},
        {"offroadt2p",   "Ironman Ivan Stewart's Super Off-Road Track-Pak (rev 4, 2 players)"},
        {"offtwalc",     "Off the Wall (2-player cocktail)"},
        {"offtwall",     "Off the Wall (2-3-player upright)"},
        {"offtwallc",    "Off the Wall (2-player cocktail)"},
        {"ogonsiro",     "Ohgon no Siro (Japan)"},
        {"ohbakyuun",    "Oh! Bakyuuun (Japan, OB1/VER.A)"},
        {"ohmygod",      "Oh My God! (Japan)"},
        {"ohpaipee",     "Oh! Paipee (Japan 890227)"},
        {"oigas",        "Oigas (bootleg)"},
        {"oisipuzl",     "Oishii Puzzle Ha Irimasenka"},
        {"ojanko2",      "Ojanko Yakata 2bankan (Japan)"},
        {"ojankoc",      "Ojanko Club (Japan)"},
        {"ojankohs",     "Ojanko High School (Japan)"},
        {"ojankoy",      "Ojanko Yakata (Japan)"},
        {"ojousan",      "Ojousan (Japan 871204)"},
        {"ojousanm",     "Ojousan [BET] (Japan 870108)"},
        {"olds",         "Oriental Legend Special - Xi You Shi E Zhuan Super (ver. 101, Korean Board)"},
        {"olds100",      "Oriental Legend Special / Xiyou Shi E Zhuan Super (ver. 100, set 1)"},
        {"olds100a",     "Oriental Legend Special / Xiyou Shi E Zhuan Super (ver. 100, set 2)"},
        {"olds103t",     "Oriental Legend Special - Xi You Shi E Zhuan Super (ver. 103, China, Tencent) (unprotected)"},
        {"oldsplus",     "Oriental Legend 2 (Korea) / Xiyou Shi E Zhuan Qunmo Luanwu (World, China, Japan, Hong Kong, Taiwan) (ver. 205) [Oriental Ex]"},
        {"oldsplus203",  "Oriental Legend 2 (Korea) / Xiyou Shi E Zhuan Qunmo Luanwu (World, China, Japan, Hong Kong, Taiwan) (ver. 203) [Oriental Ex]"},
        {"olibochu",     "Oli-Boo-Chu"},
        {"olibug",       "Oli Bug (bootleg of Jump Bug)"},
        {"ollie",        "Ollie King (GDX-0007)"},
        {"olmandingc",   "Olivmandingo (Spanish bootleg of Mandinga on Galaxian hardware, set 2)"},
        {"olmandingo",   "Olivmandingo (Spanish bootleg of Mandinga on Galaxian hardware, set 1)"},
        {"olysoc92",     "Olympic Soccer '92"},
        {"omega",        "Omega"},
        {"omegaa",       "Omega (earlier)"},
        {"omegab",       "Omega (bootleg?)"},
        {"omegaf",       "Omega Fighter"},
        {"omegafs",      "Omega Fighter Special"},
        {"omegrace",     "Omega Race"},
        {"omegrace2",    "Omega Race (set 2)"},
        {"omegrace3",    "Omega Race (set 3, 7/27)"},
        {"omni",         "Omni"},
        {"omotesnd",     "Omotesandou (Japan 890215)"},
        {"oneshot",      "One Shot One Kill"},
        {"onetwo",       "One + Two"},
        {"onetwoe",      "One + Two (earlier)"},
        {"onna34ra",     "Onna Sansirou - Typhoon Gal (bootleg)"},
        {"onna34ro",     "Onna Sansirou - Typhoon Gal"},
        {"onna34roa",    "Onna Sanshirou - Typhoon Gal (bootleg)"},
        {"ooparts",      "OOPArts (Japan, Prototype)"},
        {"opaopa",       "Opa Opa (MC-8123, 317-0042)"},
        {"opaopan",      "Opa Opa (Rev A, unprotected)"},
        {"opengolf",     "Konami's Open Golf Championship (ver EAD)"},
        {"opengolf2",    "Konami's Open Golf Championship (ver EAD)"},
        {"openice",      "2 On 2 Open Ice Challenge (rev 1.21)"},
        {"openicea",     "2 On 2 Open Ice Challenge (rev 1.2A)"},
        {"openmj",       "Open Mahjong [BET] (Japan)"},
        {"optiger",      "Operation Tiger"},
        {"opwolf",       "Operation Wolf (US)"},
        {"opwolf3",      "Operation Wolf 3 (World)"},
        {"opwolf3j",     "Operation Wolf 3 (Japan)"},
        {"opwolf3u",     "Operation Wolf 3 (US)"},
        {"opwolfa",      "Operation Wolf (World, rev 2, set 2)"},
        {"opwolfb",      "Operation Bear"},
        {"opwolfj",      "Operation Wolf (Japan, rev 2)"},
        {"opwolfjsc",    "Operation Wolf (Japan, SC)"},
        {"opwolfp",      "Operation Wolf (Japan, prototype)"},
        {"opwolfu",      "Operation Wolf (US, rev 2)"},
        {"orangec",      "Orange Club - Maruhi Kagai Jugyou (Japan 880213)"},
        {"orangeci",     "Orange Club - Maru-hi Ippatsu Kaihou [BET] (Japan 880221)"},
        {"orbit",        "Orbit"},
        {"orbite",       "Orbite (prototype)"},
        {"orbitron",     "Orbitron"},
        {"orbs",         "Orbs (10/7/94 prototype?)"},
        {"ordyne",       "Ordyne (Japan)"},
        {"ordynej",      "Ordyne (Japan)"},
        {"ordyneje",     "Ordyne (Japan, English Version)"},
        {"orius",        "Orius (ver UAA)"},
        {"orleg2",       "Oriental Legend 2 (V104, Oversea)"},
        {"orleg2_101",   "Oriental Legend 2 (V101, Oversea)"},
        {"orleg2_101cn", "Xiyou Shi E Zhuan 2 (V101, China)"},
        {"orleg2_101jp", "Saiyuu Shakuyakuden 2 (V101, Japan)"},
        {"orleg2_103",   "Oriental Legend 2 (V103, Oversea)"},
        {"orleg2_103cn", "Xiyou Shi E Zhuan 2 (V103, China)"},
        {"orleg2_103jp", "Saiyuu Shakuyakuden 2 (V103, Japan)"},
        {"orleg2_104cn", "Xiyou Shi E Zhuan 2 (V104, China)"},
        {"orleg2_104jp", "Saiyuu Shakuyakuden 2 (V104, Japan)"},
        {"orlegend",     "Oriental Legend - Xi Yo Gi Shi Re Zuang (ver. 126)"},
        {"orlegend105k", "Oriental Legend / Xiyou Shi E Zhuan (ver. 105, Korean Board)"},
        {"orlegend105t", "Oriental Legend / Xiyou Shi E Zhuan (ver. 105, Taiwanese Board)"},
        {"orlegend111c", "Oriental Legend / Xiyou Shi E Zhuan (ver. 111, Chinese Board)"},
        {"orlegend111k", "Oriental Legend / Xiyou Shi E Zhuan (ver. 111, Korean Board)"},
        {"orlegend111t", "Oriental Legend / Xiyou Shi E Zhuan (ver. 111, Taiwanese Board)"},
        {"orlegendc",    "Oriental Legend / Xiyou Shi E Zhuan (ver. 112, Chinese Board)"},
        {"orlegendca",   "Oriental Legend / Xiyou Shi E Zhuan (ver. ???, Chinese Board)"},
        {"orlegende",    "Oriental Legend / Xiyou Shi E Zhuan (ver. 112, set 1)"},
        {"orlegendea",   "Oriental Legend / Xiyou Shi E Zhuan (ver. 112, set 2)"},
        {"orlegndc",     "Oriental Legend - Xi Yo Gi Shi Re Zuang (ver. 112, Chinese Board)"},
        {"orlegnde",     "Oriental Legend - Xi Yo Gi Shi Re Zuang (ver. 112)"},
        {"orunners",     "Outrunners (US)"},
        {"orunnersj",    "OutRunners (Japan)"},
        {"orunnersu",    "OutRunners (US)"},
        {"oscar",        "Psycho-Nics Oscar (US)"},
        {"oscarbl",      "Psycho-Nics Oscar (World revision 0, bootleg)"},
        {"oscarj",       "Psycho-Nics Oscar (Japan revision 2)"},
        {"oscarj0",      "Psycho-Nics Oscar (Japan revision 0)"},
        {"oscarj1",      "Psycho-Nics Oscar (Japan revision 1)"},
        {"oscarj2",      "Psycho-Nics Oscar (Japan revision 2)"},
        {"oscaru",       "Psycho-Nics Oscar (US)"},
        {"osman",        "Osman (World)"},
        {"otatidai",     "Disco Mahjong Otachidai no Okite (Japan)"},
        {"otenamhf",     "Otenami Haiken Final (V2.07JC)"},
        {"otenamih",     "Otenami Haiken (V2.04J)"},
        {"otenki",       "Otenki Kororin (V2.01J)"},
        {"othello",      "Othello (version 3.0)"},
        {"othellos",     "Othello Shiyouyo"},
        {"othldrby",     "Othello Derby (Japan)"},
        {"othunder",     "Operation Thunderbolt (World)"},
        {"othunderj",    "Operation Thunderbolt (Japan)"},
        {"othunderjsc",  "Operation Thunderbolt (Japan, SC)"},
        {"othundero",    "Operation Thunderbolt (World)"},
        {"othunderu",    "Operation Thunderbolt (US, rev 1)"},
        {"othunderua",   "Operation Thunderbolt (US)"},
        {"othunderuo",   "Operation Thunderbolt (US, older)"},
        {"othundu",      "Operation Thunderbolt (US)"},
        {"otonano",      "Otona no Mahjong (Japan 880628)"},
        {"otrigger",     "OutTrigger"},
        {"otwalls",      "Off the Wall (Sente)"},
        {"outfxesj",     "Outfoxies (Japan)"},
        {"outfxies",     "Outfoxies"},
        {"outfxiesa",    "The Outfoxies (Korea?)"},
        {"outfxiesj",    "The Outfoxies (Japan, OU1)"},
        {"outfxiesja",   "The Outfoxies (Japan, OU1, alternate GFX ROMs)"},
        {"outline",      "Outline"},
        {"outr2",        "Out Run 2 (Rev. A) (GDX-0004A)"},
        {"outrun",       "Out Run (set 1)"},
        {"outruna",      "Out Run (set 2)"},
        {"outrunb",      "Out Run (set 3)"},
        {"outrundx",     "Out Run (deluxe sitdown)"},
        {"outrundxa",    "Out Run (deluxe sitdown earlier version)"},
        {"outrundxeh",   "Out Run (deluxe sitdown) (Enhanced Edition v2.0.3)"},
        {"outrundxeha",  "Out Run (deluxe sitdown) (Enhanced Edition v1.0.3)"},
        {"outrundxj",    "Out Run (Japan, deluxe sitdown) (FD1089A 317-0019)"},
        {"outruneh",     "Out Run (sitdown/upright, Rev B) (Enhanced Edition v2.0.3)"},
        {"outruneha",    "Out Run (sitdown/upright, Rev B) (Enhanced Edition v1.1.0)"},
        {"outrunehb",    "Out Run (sitdown/upright, Rev B) (Enhanced Edition v2.0.2)"},
        {"outruno",      "Out Run (sitdown/upright)"},
        {"outrunra",     "Out Run (sitdown/upright, Rev A)"},
        {"outzone",      "Out Zone (set 1)"},
        {"outzonea",     "Out Zone (set 2)"},
        {"outzoneb",     "Out Zone (older set)"},
        {"outzonec",     "Out Zone (oldest set)"},
        {"outzonecv",    "Out Zone (Zero Wing TP-015 PCB conversion)"},
        {"outzoned",     "Out Zone (set 5)"},
        {"outzoneh",     "Out Zone (harder)"},
        {"outzonep",     "Out Zone (bootleg)"},
        {"overdriv",     "Over Drive"},
        {"overrev",      "Over Rev (Model 2C, Revision A)"},
        {"overrevb",     "Over Rev (Model 2B, Revision B)"},
        {"overrevba",    "Over Rev (Model 2B, Revision A)"},
        {"overtop",      "Over Top"},
        {"ozmawar2",     "Ozma Wars (set 2)"},
        {"ozmawars",     "Ozma Wars (set 1)"},
        {"ozmawars2",    "Ozma Wars (set 2)"},
        {"ozmawarsmr",   "Ozma Wars (Model Racing bootleg)"},
        {"ozon1",        "Ozon I"},
};

const size_t lookup_o_count = A_SIZE(lookup_o_table);

const char *lookup_o(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < lookup_o_count; i++) {
        if (strcmp(lookup_o_table[i].name, name) == 0) {
            return lookup_o_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_o(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < lookup_o_count; i++) {
        if (strstr(lookup_o_table[i].value, value)) {
            return lookup_o_table[i].name;
        }
    }
    return NULL;
}

void lookup_o_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_o_count; i++) {
        if (strcasestr(lookup_o_table[i].name, term))
            emit(lookup_o_table[i].name, lookup_o_table[i].value, udata);
    }
}

void r_lookup_o_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_o_count; i++) {
        if (strcasestr(lookup_o_table[i].value, term))
            emit(lookup_o_table[i].name, lookup_o_table[i].value, udata);
    }
}
