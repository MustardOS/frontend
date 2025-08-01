#include <string.h>
#include "../common/common.h"
#include "lookup.h"

typedef struct {
    const char *name;
    const char *value;
} LookupName;

static const LookupName lookup_table[] = {
        {"r2dtank",     "R2D Tank"},
        {"r2dx_v33",    "Raiden II New / Raiden DX (newer V33 PCB) (Raiden DX EEPROM)"},
        {"r2dx_v33_r2", "Raiden II New / Raiden DX (newer V33 PCB) (Raiden II EEPROM)"},
        {"rabbit",      "Rabbit"},
        {"rabbita",     "Rabbit (Asia 1/28?)"},
        {"rabbitj",     "Rabbit (Japan 3/6?)"},
        {"rabbitjt",    "Rabbit (Japan 1/28, location test)"},
        {"rabiolep",    "Rabio Lepus (Japan)"},
        {"racedcb4",    "Race Drivin' (compact, British, rev 4)"},
        {"racedcg4",    "Race Drivin' (compact, German, rev 4)"},
        {"racedrb1",    "Race Drivin' (cockpit, British, rev 1)"},
        {"racedrb4",    "Race Drivin' (cockpit, British, rev 4)"},
        {"racedrc1",    "Race Drivin' (compact, rev 1)"},
        {"racedrc2",    "Race Drivin' (compact, rev 2)"},
        {"racedrc4",    "Race Drivin' (compact, rev 4)"},
        {"racedrcb",    "Race Drivin' (compact, British, rev 5)"},
        {"racedrcg",    "Race Drivin' (compact, German, rev 5)"},
        {"racedrg1",    "Race Drivin' (cockpit, German, rev 2)"},
        {"racedrg4",    "Race Drivin' (cockpit, German, rev 4)"},
        {"racedriv",    "Race Drivin' (cockpit, rev 5)"},
        {"racedriv1",   "Race Drivin' (cockpit, rev 1)"},
        {"racedriv2",   "Race Drivin' (cockpit, rev 2)"},
        {"racedriv3",   "Race Drivin' (cockpit, rev 3)"},
        {"racedriv4",   "Race Drivin' (cockpit, rev 4)"},
        {"racedrivb",   "Race Drivin' (cockpit, British, rev 5)"},
        {"racedrivb1",  "Race Drivin' (cockpit, British, rev 1)"},
        {"racedrivb4",  "Race Drivin' (cockpit, British, rev 4)"},
        {"racedrivc",   "Race Drivin' (compact, rev 5)"},
        {"racedrivc1",  "Race Drivin' (compact, rev 1)"},
        {"racedrivc2",  "Race Drivin' (compact, rev 2)"},
        {"racedrivc4",  "Race Drivin' (compact, rev 4)"},
        {"racedrivcb",  "Race Drivin' (compact, British, rev 5)"},
        {"racedrivcb4", "Race Drivin' (compact, British, rev 4)"},
        {"racedrivcg",  "Race Drivin' (compact, German, rev 5)"},
        {"racedrivcg4", "Race Drivin' (compact, German, rev 4)"},
        {"racedrivcp",  "Race Drivin' (compact, prototype)"},
        {"racedrivg",   "Race Drivin' (cockpit, German, rev 5)"},
        {"racedrivg1",  "Race Drivin' (cockpit, German, rev 2)"},
        {"racedrivg4",  "Race Drivin' (cockpit, German, rev 4)"},
        {"racedrivpan", "Race Drivin' Panorama (prototype, rev 2.1)"},
        {"racedrv1",    "Race Drivin' (cockpit, rev 1)"},
        {"racedrv2",    "Race Drivin' (cockpit, rev 2)"},
        {"racedrv3",    "Race Drivin' (cockpit, rev 3)"},
        {"racedrv4",    "Race Drivin' (cockpit, rev 4)"},
        {"racedrvb",    "Race Drivin' (cockpit, British, rev 5)"},
        {"racedrvc",    "Race Drivin' (compact, rev 5)"},
        {"racedrvg",    "Race Drivin' (cockpit, German, rev 5)"},
        {"rachero",     "Racing Hero (FD1094 317-0144)"},
        {"racherod",    "Racing Hero (bootleg of FD1094 317-0144 set)"},
        {"racinfrc",    "Racin' Force (ver UAB)"},
        {"racingb",     "Racing Beat (World)"},
        {"racingbj",    "Racing Beat (Japan)"},
        {"rackemup",    "Rack 'em Up"},
        {"racknrol",    "Rack + Roll"},
        {"racoon",      "Racoon World"},
        {"radarscp",    "Radar Scope (TRS02, rev. D)"},
        {"radarscp1",   "Radar Scope (TRS01)"},
        {"radarscpc",   "Radar Scope (TRS02?, rev. C)"},
        {"radarzn1",    "Radar Zone (Rev.1)"},
        {"radarznt",    "Radar Zone (Tuni)"},
        {"radarzon",    "Radar Zone"},
        {"radarzon1",   "Radar Zone (Rev.1)"},
        {"radarzont",   "Radar Zone (Tuni)"},
        {"radikalb",    "Radikal Bikers (version 2.02)"},
        {"radikalba",   "Radikal Bikers (version 2.02, Atari license)"},
        {"radirgy",     "Radirgy (Japan, Rev A) (GDL-0032A)"},
        {"radirgya",    "Radirgy (Rev A) (GDL-0032A)"},
        {"radirgyn",    "Radirgy Noa (Japan)"},
        {"radirgyo",    "Radirgy (Japan) (GDL-0032)"},
        {"radm",        "Rad Mobile"},
        {"radmu",       "Rad Mobile (US)"},
        {"radr",        "Rad Rally"},
        {"radrad",      "Radical Radial"},
        {"radradj",     "Radical Radial (Japan)"},
        {"radrj",       "Rad Rally (Japan)"},
        {"radru",       "Rad Rally (US)"},
        {"raflesia",    "Rafflesia"},
        {"raflesiau",   "Rafflesia (not encrypted)"},
        {"ragnagrd",    "Ragnagard / Shin-Oh-Ken"},
        {"ragtime",     "The Great Ragtime Show (Japan v1.5)"},
        {"ragtimea",    "The Great Ragtime Show (Japan v1.3)"},
        {"raiden",      "Raiden"},
        {"raiden2",     "Raiden 2"},
        {"raiden2au",   "Raiden II (Australia)"},
        {"raiden2dx",   "Raiden II (harder, Raiden DX hardware, Korea)"},
        {"raiden2e",    "Raiden II (easier, Korea)"},
        {"raiden2ea",   "Raiden II (easier, Japan)"},
        {"raiden2eg",   "Raiden II (easier, Germany)"},
        {"raiden2es",   "Raiden II (Spain)"},
        {"raiden2eu",   "Raiden II (easier, US set 2)"},
        {"raiden2eua",  "Raiden II (easier, US set 1)"},
        {"raiden2eub",  "Raiden II (easier, US set 3)"},
        {"raiden2eup",  "Raiden II (easier, US, prototype? 11-16)"},
        {"raiden2f",    "Raiden II (France)"},
        {"raiden2g",    "Raiden II (Germany)"},
        {"raiden2hk",   "Raiden II (Hong Kong)"},
        {"raiden2i",    "Raiden II (Italy)"},
        {"raiden2j",    "Raiden II (Japan)"},
        {"raiden2k",    "Raiden II (harder, Korea)"},
        {"raiden2nl",   "Raiden II (Holland)"},
        {"raiden2sw",   "Raiden II (Switzerland)"},
        {"raiden2u",    "Raiden II (US, set 2)"},
        {"raidena",     "Raiden (Alternate Hardware)"},
        {"raidenb",     "Raiden (World set 2, newer hardware)"},
        {"raidendx",    "Raiden DX (UK)"},
        {"raidendxa1",  "Raiden DX (Hong Kong, set 1)"},
        {"raidendxa2",  "Raiden DX (Hong Kong, set 2)"},
        {"raidendxch",  "Raiden DX (China)"},
        {"raidendxg",   "Raiden DX (Germany)"},
        {"raidendxj",   "Raiden DX (Japan, set 1)"},
        {"raidendxja",  "Raiden DX (Japan, set 2)"},
        {"raidendxk",   "Raiden DX (Korea)"},
        {"raidendxnl",  "Raiden DX (Holland)"},
        {"raidendxpt",  "Raiden DX (Portugal)"},
        {"raidendxu",   "Raiden DX (US)"},
        {"raidenj",     "Raiden (Japan)"},
        {"raidenk",     "Raiden (Korea)"},
        {"raidenkb",    "Raiden (Korea, bootleg)"},
        {"raident",     "Raiden (Taiwan)"},
        {"raidenu",     "Raiden (US set 1)"},
        {"raidenua",    "Raiden (US set 2, SEI8904 hardware)"},
        {"raidenub",    "Raiden (US set 3, newer hardware)"},
        {"raiders5",    "Raiders5"},
        {"raiders5t",   "Raiders5 (Japan, set 1)"},
        {"raiders5ta",  "Raiders5 (Japan, set 2, bootleg?)"},
        {"raidrs5t",    "Raiders5 (Japan)"},
        {"raiga",       "Raiga - Strato Fighter (Japan)"},
        {"raimais",     "Raimais (Japan)"},
        {"raimaisj",    "Raimais (Japan, rev 1)"},
        {"raimaisjo",   "Raimais (Japan)"},
        {"rainbow",     "Rainbow Islands (new version)"},
        {"rainbowe",    "Rainbow Islands (Extra)"},
        {"rainbowo",    "Rainbow Islands (old version)"},
        {"rallybik",    "Rally Bike - Dash Yarou"},
        {"rallys",      "Rallys (bootleg[Q])"},
        {"rallyx",      "Rally X"},
        {"rallyxm",     "Rally X (Midway)"},
        {"rambo3",      "Rambo III (Europe set 1)"},
        {"rambo3a",     "Rambo III (US)"},
        {"rambo3ae",    "Rambo III (Europe set 2)"},
        {"rambo3p",     "Rambo III (Europe, Proto?)"},
        {"rambo3u",     "Rambo III (US)"},
        {"rampage",     "Rampage (revision 3)"},
        {"rampage2",    "Rampage (revision 2)"},
        {"rampart",     "Rampart (Trackball)"},
        {"rampart2p",   "Rampart (Joystick, bigger ROMs)"},
        {"rampart2pa",  "Rampart (Joystick, smaller ROMs)"},
        {"rampartj",    "Rampart (Japan, Joystick)"},
        {"ramprt2p",    "Rampart (Joystick)"},
        {"rangrmsn",    "Ranger Mission"},
        {"raphero",     "Rapid Hero"},
        {"rapheroa",    "Rapid Hero (Media Trading)"},
        {"rapidfir",    "Rapid Fire (v1.1, Build 239)"},
        {"rapidfira",   "Rapid Fire (v1.1, Build 238)"},
        {"rapidfire",   "Rapid Fire (v1.0, Build 236)"},
        {"rastan",      "Rastan (World)"},
        {"rastana",     "Rastan (World)"},
        {"rastanb",     "Rastan (World, earlier code base)"},
        {"rastanu",     "Rastan (US set 1)"},
        {"rastanu2",    "Rastan (US set 2)"},
        {"rastanua",    "Rastan (US)"},
        {"rastanub",    "Rastan (US, earlier code base)"},
        {"rastsag2",    "Rastan Saga 2 (Japan)"},
        {"rastsaga",    "Rastan Saga (Japan)"},
        {"rastsagaa",   "Rastan Saga (Japan Rev 1, earlier code base)"},
        {"rastsagaabl", "Rastan Saga (bootleg, Japan Rev 1, earlier code base)"},
        {"rastsagab",   "Rastan Saga (Japan, earlier code base)"},
        {"raverace",    "Rave Racer (World, RV2 Ver.B)"},
        {"raveracej",   "Rave Racer (Japan, RV1 Ver.B)"},
        {"raveraceja",  "Rave Racer (Japan, RV1)"},
        {"raveracw",    "Rave Racer (Rev. RV2, World)"},
        {"rayforce",    "Rayforce (US)"},
        {"rayforcej",   "Ray Force (Ver 2.3J 1994/01/20)"},
        {"rayforcj",    "Rayforce (Japan)"},
        {"raystorm",    "Ray Storm (JAPAN)"},
        {"razmataz",    "Razzmatazz"},
        {"rbff1",       "Real Bout Fatal Fury / Real Bout Garou Densetsu"},
        {"rbff1a",      "Real Bout Fatal Fury / Real Bout Garou Densetsu (bug fix revision)"},
        {"rbff1k",      "Real Bout Fatal Fury / Real Bout Garou Densetsu (Korean release)"},
        {"rbff1ka",     "Real Bout Fatal Fury / Real Bout Garou Densetsu (Korean release, bug fix revision)"},
        {"rbff2",       "Real Bout Fatal Fury 2 - The Newcomers / Real Bout Garou Densetsu 2 - The Newcomers (set 1)"},
        {"rbff2a",      "Real Bout Fatal Fury 2 - The Newcomers / Real Bout Garou Densetsu 2 - The Newcomers (set 2)"},
        {"rbff2h",      "Real Bout Fatal Fury 2 - The Newcomers / Real Bout Garou Densetsu 2 - The Newcomers (NGH-2400)"},
        {"rbff2k",      "Real Bout Fatal Fury 2 - The Newcomers (Korean release)"},
        {"rbffspec",    "Real Bout Fatal Fury Special / Real Bout Garou Densetsu Special"},
        {"rbffspeck",   "Real Bout Fatal Fury Special / Real Bout Garou Densetsu Special (Korean release)"},
        {"rbibb",       "Vs. Atari R.B.I. Baseball (set 1)"},
        {"rbibba",      "Vs. Atari R.B.I. Baseball (set 2)"},
        {"rbisland",    "Rainbow Islands (rev 1)"},
        {"rbislande",   "Rainbow Islands - Extra Version"},
        {"rbislando",   "Rainbow Islands"},
        {"rbtapper",    "Tapper (Root Beer)"},
        {"rchase",      "Rail Chase (World)"},
        {"rchase2",     "Rail Chase 2 (Revision A)"},
        {"rchasej",     "Rail Chase (Japan)"},
        {"rchasejb",    "Rail Chase (Japan, Rev B)"},
        {"rdaction",    "Rad Action"},
        {"rdft",        "Raiden Fighters (Germany)"},
        {"rdft2",       "Raiden Fighters 2 - Operation Hell Dive (Germany)"},
        {"rdft22kc",    "Raiden Fighters 2 - Operation Hell Dive 2000 (China, SYS386I)"},
        {"rdft2a",      "Raiden Fighters 2 - Operation Hell Dive (Hong Kong)"},
        {"rdft2aa",     "Raiden Fighters 2 - Operation Hell Dive (Korea)"},
        {"rdft2it",     "Raiden Fighters 2 - Operation Hell Dive (Italy)"},
        {"rdft2j",      "Raiden Fighters 2 - Operation Hell Dive (Japan set 1)"},
        {"rdft2ja",     "Raiden Fighters 2 - Operation Hell Dive (Japan set 2)"},
        {"rdft2jb",     "Raiden Fighters 2 - Operation Hell Dive (Japan set 3)"},
        {"rdft2jc",     "Raiden Fighters 2 - Operation Hell Dive (Japan set 4)"},
        {"rdft2s",      "Raiden Fighters 2 - Operation Hell Dive (Switzerland)"},
        {"rdft2t",      "Raiden Fighters 2 - Operation Hell Dive (Taiwan)"},
        {"rdft2u",      "Raiden Fighters 2 - Operation Hell Dive (US)"},
        {"rdft2us",     "Raiden Fighters 2 - Operation Hell Dive (US, single board)"},
        {"rdfta",       "Raiden Fighters (Austria)"},
        {"rdftadi",     "Raiden Fighters (Korea)"},
        {"rdftam",      "Raiden Fighters (Hong Kong)"},
        {"rdftau",      "Raiden Fighters (Australia)"},
        {"rdftauge",    "Raiden Fighters (Evaluation Software For Show, Germany)"},
        {"rdftgb",      "Raiden Fighters (Great Britain)"},
        {"rdftgr",      "Raiden Fighters (Greece)"},
        {"rdftit",      "Raiden Fighters (Italy)"},
        {"rdftj",       "Raiden Fighters (Japan, earlier)"},
        {"rdftja",      "Raiden Fighters (Japan, earliest)"},
        {"rdftjb",      "Raiden Fighters (Japan, newer)"},
        {"rdfts",       "Raiden Fighters (Taiwan, single board)"},
        {"rdftu",       "Raiden Fighters (US, earlier)"},
        {"rdftua",      "Raiden Fighters (US, newer)"},
        {"reactor",     "Reactor"},
        {"reaktor",     "Reaktor (Track & Field conversion)"},
        {"realbrk",     "Billiard Academy Real Break (Japan)"},
        {"realbrkj",    "Billiard Academy Real Break (Japan)"},
        {"realbrkk",    "Billiard Academy Real Break (Korea)"},
        {"realbrko",    "Billiard Academy Real Break (Europe, older)"},
        {"rebound",     "Rebound (Rev B)"},
        {"recalh",      "Recalhorn (Prototype)"},
        {"recordbr",    "Recordbreaker (World)"},
        {"redalert",    "Red Alert"},
        {"redbaron",    "Red Baron"},
        {"redbarona",   "Red Baron"},
        {"redclash",    "Red Clash"},
        {"redclask",    "Red Clash (Kaneko)"},
        {"redearth",    "Red Earth (Europe 961121)"},
        {"redearthn",   "Red Earth (Asia 961121, NO CD)"},
        {"redearthnr1", "Red Earth (Asia 961023, NO CD)"},
        {"redearthr1",  "Red Earth (Europe 961023)"},
        {"redfoxwp2",   "Hong Hu Zhanji II (China, set 1)"},
        {"redfoxwp2a",  "Hong Hu Zhanji II (China, set 2)"},
        {"redhawk",     "Red Hawk (US)"},
        {"redhawkb",    "Red Hawk (horizontal, bootleg)"},
        {"redhawke",    "Red Hawk (Excellent Co., Ltd)"},
        {"redhawkg",    "Red Hawk (horizontal, Greece)"},
        {"redhawki",    "Red Hawk (horizontal, Italy)"},
        {"redhawkk",    "Red Hawk (Korea)"},
        {"redhawks",    "Red Hawk (horizontal, Spain, set 1)"},
        {"redhawksa",   "Red Hawk (horizontal, Spain, set 2)"},
        {"redlin2p",    "Redline Racer (2 players)"},
        {"redrobin",    "Red Robin"},
        {"redufo",      "Defend the Terra Attack on the Red UFO"},
        {"redufob",     "Defend the Terra Attack on the Red UFO (bootleg, set 1)"},
        {"redufob2",    "Defend the Terra Attack on the Red UFO (bootleg, set 2)"},
        {"redufob3",    "Defend the Terra Attack on the Red UFO (bootleg, set 3)"},
        {"reelfun",     "Reel Fun (Version 7.03)"},
        {"reelfun0",    "Reel Fun (Version 7.00)"},
        {"reelfun1",    "Reel Fun (Version 7.01)"},
        {"regency",     "Regency"},
        {"regulus",     "Regulus (New Ver.)"},
        {"reguluso",    "Regulus (Old Ver.)"},
        {"regulusu",    "Regulus (not encrypted)"},
        {"reikaids",    "Reikai Doushi (Japan)"},
        {"relief",      "Relief Pitcher (set 1)"},
        {"relief2",     "Relief Pitcher (set 2)"},
        {"relief3",     "Relief Pitcher (Rev B, 10 Apr 1992 / 08 Apr 1992)"},
        {"renaiclb",    "Mahjong Ren-ai Club (Japan)"},
        {"renegade",    "Renegade (US)"},
        {"renegadeb",   "Renegade (US bootleg)"},
        {"renju",       "Renju Kizoku - Kira Kira Gomoku Narabe"},
        {"repulse",     "Repulse"},
        {"rescraid",    "Rescue Raider"},
        {"rescraida",   "Rescue Raider (stand-alone)"},
        {"rescrdsa",    "Rescue Raider (Stand-Alone)"},
        {"rescue",      "Rescue"},
        {"rescueb",     "Tuono Blu (bootleg of Rescue)"},
        {"retofin1",    "Return of the Invaders (bootleg set 1)"},
        {"retofin2",    "Return of the Invaders (bootleg set 2)"},
        {"retofinv",    "Return of the Invaders"},
        {"retofinvb",   "Return of the Invaders (bootleg w/MCU)"},
        {"retofinvb1",  "Return of the Invaders (bootleg no MCU set 1)"},
        {"retofinvb2",  "Return of the Invaders (bootleg no MCU set 2)"},
        {"retofinvb3",  "Return of the Invaders (bootleg no MCU set 3)"},
        {"revngr84",    "Revenger '84 (newer)"},
        {"revx",        "Revolution X (Rev. 1.0 6-16-94)"},
        {"revxp5",      "Revolution X (prototype, rev 5.0 5/23/94)"},
        {"rezon",       "Rezon"},
        {"rezono",      "Rezon (earlier)"},
        {"rf2",         "Konami RF2 - Red Fighter"},
        {"rfjet",       "Raiden Fighters Jet (Germany)"},
        {"rfjet2kc",    "Raiden Fighters Jet 2000 (China, SYS386I)"},
        {"rfjeta",      "Raiden Fighters Jet (Korea)"},
        {"rfjetj",      "Raiden Fighters Jet (Japan)"},
        {"rfjets",      "Raiden Fighters Jet (US, single board)"},
        {"rfjetsa",     "Raiden Fighters Jet (US, single board, test version?)"},
        {"rfjett",      "Raiden Fighters Jet (Taiwan)"},
        {"rfjetu",      "Raiden Fighters Jet (US)"},
        {"ribbit",      "Ribbit!"},
        {"ribbitj",     "Ribbit! (Japan)"},
        {"ridefgtj",    "Riding Fight (Japan)"},
        {"ridefgtu",    "Riding Fight (US)"},
        {"ridger2j",    "Ridge Racer 2 (Rev. RRS1, Japan)"},
        {"ridgera2",    "Ridge Racer 2 (World, RRS2)"},
        {"ridgera28",   "Ridge Racer 2 (World, RRS8)"},
        {"ridgera2j",   "Ridge Racer 2 (Japan, RRS1 Ver.B)"},
        {"ridgera2ja",  "Ridge Racer 2 (Japan, RRS1)"},
        {"ridgerac",    "Ridge Racer (World, RR2 Ver.B)"},
        {"ridgeraca",   "Ridge Racer (World, RR2)"},
        {"ridgeracb",   "Ridge Racer (US, RR3 Ver.B)"},
        {"ridgeracc",   "Ridge Racer (US, RR3)"},
        {"ridgeracj",   "Ridge Racer (Japan, RR1)"},
        {"ridgeraj",    "Ridge Racer (Rev. RR1, Japan)"},
        {"ridhero",     "Riding Hero (set 1)"},
        {"ridheroh",    "Riding Hero (set 2)"},
        {"ridingf",     "Riding Fight (World)"},
        {"ridingfj",    "Riding Fight (Ver 1.0J)"},
        {"ridingfu",    "Riding Fight (Ver 1.0A)"},
        {"ridleofp",    "Riddle of Pythagoras (Japan)"},
        {"rimrck12",    "Rim Rockin' Basketball (V1.2)"},
        {"rimrck16",    "Rim Rockin' Basketball (V1.6)"},
        {"rimrck20",    "Rim Rockin' Basketball (V2.0)"},
        {"rimrockn",    "Rim Rockin' Basketball (V2.2)"},
        {"rimrockn12",  "Rim Rockin' Basketball (V1.2)"},
        {"rimrockn12b", "Rim Rockin' Basketball (V1.2, bootleg)"},
        {"rimrockn15",  "Rim Rockin' Basketball (V1.5)"},
        {"rimrockn16",  "Rim Rockin' Basketball (V1.6)"},
        {"rimrockn20",  "Rim Rockin' Basketball (V2.0)"},
        {"ringdest",    "Ring of Destruction: Slammasters II (Euro 940902)"},
        {"ringdesta",   "Ring of Destruction: Slammasters II (Asia 940831)"},
        {"ringdestb",   "Ring of Destruction: Slammasters II (Brazil 940902)"},
        {"ringdesth",   "Ring of Destruction: Slammasters II (Hispanic 940902)"},
        {"ringdstd",    "Ring of Destruction: Slammasters II (Europe 940902 Phoenix Edition) (bootleg)"},
        {"ringfgt",     "Ring Fighter (set 1)"},
        {"ringfgt2",    "Ring Fighter (set 2)"},
        {"ringfgta",    "Ring Fighter"},
        {"ringkin2",    "Ring King (US set 2)"},
        {"ringkin3",    "Ring King (US set 3)"},
        {"ringking",    "Ring King (US set 1)"},
        {"ringking2",   "Ring King (US set 2)"},
        {"ringking3",   "Ring King (US set 3)"},
        {"ringkingw",   "Ring King (US, Woodplace Inc.)"},
        {"ringohja",    "Ring no Ohja (Japan 2 Players ver. N)"},
        {"ringout",     "Ring Out 4x4 (Rev A)"},
        {"ringouto",    "Ring Out 4x4"},
        {"ringrage",    "Ring Rage (World)"},
        {"ringragej",   "Ring Rage (Ver 2.3J 1992/08/09)"},
        {"ringrageu",   "Ring Rage (Ver 2.3A 1992/08/09)"},
        {"ringragj",    "Ring Rage (Japan)"},
        {"ringragu",    "Ring Rage (US)"},
        {"riot",        "Riot"},
        {"riotcity",    "Riot City"},
        {"riotw",       "Riot (Woong Bi license)"},
        {"ripcord",     "Rip Cord"},
        {"ripoff",      "Rip Off"},
        {"ripribit",    "Ripper Ribbit (Version 3.5)"},
        {"ripribita",   "Ripper Ribbit (Version 2.8.4)"},
        {"riskchal",    "Risky Challenge"},
        {"rittam",      "R&T (Rod-Land prototype)"},
        {"rjammer",     "Roller Jammer"},
        {"rltennis",    "Reality Tennis (set 1)"},
        {"rltennisa",   "Reality Tennis (set 2)"},
        {"rmancp2j",    "Rockman: The Power Battle (CPS2, Japan 950922)"},
        {"rmhaihai",    "Real Mahjong Haihai (Japan)"},
        {"rmhaihib",    "Real Mahjong Haihai [BET] (Japan)"},
        {"rmhaijin",    "Real Mahjong Haihai Jinji Idou Hen (Japan)"},
        {"rmhaisei",    "Real Mahjong Haihai Seichouhen (Japan)"},
        {"rmpgwt",      "Rampage - World Tour (rev 1.3)"},
        {"rmpgwt11",    "Rampage - World Tour (rev 1.1)"},
        {"roadblc1",    "Road Blasters (cockpit, rev 1)"},
        {"roadblcg",    "Road Blasters (cockpit, German, rev 1)"},
        {"roadblg1",    "Road Blasters (upright, German, rev 1)"},
        {"roadblg2",    "Road Blasters (upright, German, rev 2)"},
        {"roadbls1",    "Road Blasters (upright, rev 1)"},
        {"roadbls2",    "Road Blasters (upright, rev 2)"},
        {"roadbls3",    "Road Blasters (upright, rev 3)"},
        {"roadblsc",    "Road Blasters (cockpit, rev 2)"},
        {"roadblsg",    "Road Blasters (upright, German, rev 3)"},
        {"roadblst",    "Road Blasters (upright, rev 4)"},
        {"roadblst1",   "Road Blasters (upright, rev 1)"},
        {"roadblst2",   "Road Blasters (upright, rev 2)"},
        {"roadblst3",   "Road Blasters (upright, rev 3)"},
        {"roadblstc",   "Road Blasters (cockpit, rev 2)"},
        {"roadblstc1",  "Road Blasters (cockpit, rev 1)"},
        {"roadblstcg",  "Road Blasters (cockpit, German, rev 1)"},
        {"roadblstg",   "Road Blasters (upright, German, rev 3)"},
        {"roadblstg1",  "Road Blasters (upright, German, rev 1)"},
        {"roadblstg2",  "Road Blasters (upright, German, rev 2)"},
        {"roadblstgu",  "Road Blasters (upright, German, rev ?)"},
        {"roadburn",    "Road Burners (ver 1.04)"},
        {"roadburn1",   "Road Burners (ver 1.0)"},
        {"roadf",       "Road Fighter (set 1)"},
        {"roadf2",      "Road Fighter (set 2)"},
        {"roadfh",      "Road Fighter (bootleg GX330 conversion)"},
        {"roadfu",      "Road Fighter (set 3, unencrypted)"},
        {"roadriot",    "Road Riot 4WD"},
        {"roadrun1",    "Road Runner (rev 1)"},
        {"roadrun2",    "Road Runner (rev 1+)"},
        {"roadrunm",    "Road Runner (Midway)"},
        {"roadrunn",    "Road Runner (rev 2)"},
        {"roadrunn1",   "Road Runner (rev 1)"},
        {"roadrunn2",   "Road Runner (rev 1+)"},
        {"robby",       "Robby Roto"},
        {"roboarmy",    "Robo Army"},
        {"roboarmya",   "Robo Army (NGM-032 ~ NGH-032)"},
        {"robocop",     "Robocop (World revision 4)"},
        {"robocop2",    "Robocop 2 (World)"},
        {"robocop2j",   "Robocop 2 (Japan v0.11)"},
        {"robocop2u",   "Robocop 2 (US v0.10)"},
        {"robocop2ua",  "Robocop 2 (US v0.05)"},
        {"robocopb",    "Robocop (World bootleg)"},
        {"robocopj",    "Robocop (Japan)"},
        {"robocopu",    "Robocop (US revision 1)"},
        {"robocopu0",   "Robocop (US, revision 0)"},
        {"robocopw",    "Robocop (World revision 3)"},
        {"robocp2j",    "Robocop 2 (Japan)"},
        {"robocp2u",    "Robocop 2 (US)"},
        {"robocpu0",    "Robocop (US revision 0)"},
        {"robokid",     "Atomic Robo-kid"},
        {"robokidj",    "Atomic Robo-kid (Japan)"},
        {"robotbwl",    "Robot Bowl"},
        {"robotron",    "Robotron (Solid Blue label)"},
        {"robotron12",  "Robotron: 2084 (2012 'wave 201 start' hack)"},
        {"robotron87",  "Robotron: 2084 (1987 'shot-in-the-corner' bugfix)"},
        {"robotrontd",  "Robotron: 2084 (2015 'tie-die V2' hack)"},
        {"robotronun",  "Robotron: 2084 (Unidesa license)"},
        {"robotronyo",  "Robotron: 2084 (Yellow/Orange label)"},
        {"robotryo",    "Robotron (Yellow-Orange label)"},
        {"robowres",    "Robo Wres 2001"},
        {"robowresb",   "Robo Wres 2001 (bootleg)"},
        {"rockclim",    "Rock Climber"},
        {"rockman2j",   "Rockman 2: The Power Fighters (Japan 960708)"},
        {"rockmanj",    "Rockman - The Power Battle (CPS1 Japan 950922)"},
        {"rockn",       "Rock'n Tread (Japan)"},
        {"rockn2",      "Rock'n Tread 2 (Japan)"},
        {"rockn3",      "Rock'n 3 (Japan)"},
        {"rockn4",      "Rock'n 4 (Japan)"},
        {"rockna",      "Rock'n Tread (Japan, alternate)"},
        {"rocknms",     "Rock'n MegaSession (Japan)"},
        {"rockrage",    "Rock 'n Rage (World[Q])"},
        {"rockragea",   "Rock'n Rage (prototype?)"},
        {"rockragej",   "Koi no Hotrock (Japan)"},
        {"rockragj",    "Koi no Hotrock (Japan)"},
        {"rocktris",    "Rock Tris"},
        {"rocktrv2",    "MTV Rock-N-Roll Trivia (Part 2)"},
        {"rocnrope",    "Roc'n Rope"},
        {"rocnropek",   "Roc'n Rope (Kosuka)"},
        {"rocnropk",    "Roc'n Rope (Kosuka)"},
        {"rodland",     "Rod-Land (World)"},
        {"rodlanda",    "Rod-Land (World, set 2)"},
        {"rodlandj",    "Rod-Land (Japan)"},
        {"rodlandjb",   "Rod-Land (Japan bootleg with unencrypted program)"},
        {"rodlandjb2",  "Rod-Land (Japan bootleg with unencrypted program and GFX)"},
        {"rodlndjb",    "Rod-Land (Japan bootleg)"},
        {"rohga",       "Rohga Armour Force (Asia-Europe v3.0)"},
        {"rohga1",      "Rohga Armor Force (Asia/Europe v3.0 set 1)"},
        {"rohga2",      "Rohga Armor Force (Asia/Europe v3.0 set 2)"},
        {"rohgah",      "Rohga Armour Force (Hong Kong v3.0)"},
        {"rohgau",      "Rohga Armour Force (US v1.0)"},
        {"roishtar",    "The Return of Ishtar"},
        {"rolcrush",    "Rolling Crush (version 1.07.E - 1999/02/11)"},
        {"rolcrusha",   "Rolling Crush (version 1.03.E - 1999/01/29)"},
        {"roldfrga",    "The Return of Lady Frog (set 2)"},
        {"roldfrog",    "The Return of Lady Frog"},
        {"roldfroga",   "The Return of Lady Frog (set 2)"},
        {"rollace",     "Roller Aces (set 1)"},
        {"rollace2",    "Roller Aces (set 2)"},
        {"rollerg",     "Rollergames (US)"},
        {"rollergj",    "Rollergames (Japan)"},
        {"rollingc",    "Rolling Crash / Moon Base"},
        {"rompers",     "Rompers (Japan)"},
        {"romperso",    "Rompers (Japan old version)"},
        {"rongrong",    "Rong Rong (Germany)"},
        {"rongrongg",   "Puzzle Game Rong Rong (Germany)"},
        {"rongrongj",   "Puzzle Game Rong Rong (Japan)"},
        {"ropeman",     "Ropeman (bootleg of Roc'n Rope)"},
        {"rotaryf",     "Rotary Fighter"},
        {"rotd",        "Rage of the Dragons"},
        {"rotdh",       "Rage of the Dragons (NGH-2640?)"},
        {"rotr",        "Rise of the Robots (prototype)"},
        {"rotra",       "Rise of the Robots (prototype, older)"},
        {"roughrac",    "Rough Racer (Japan, Floppy Based, FD1094 317-0058-06b)"},
        {"rougien",     "Rougien"},
        {"roundup",     "Round-Up"},
        {"roundup5",    "Round Up 5 - Super Delta Force"},
        {"route16",     "Route 16"},
        {"route16a",    "Route 16 (set 2)"},
        {"route16b",    "Route 16 (bootleg)"},
        {"route16bl",   "Route 16 (bootleg)"},
        {"route16c",    "Route 16 (Centuri license, set 3, bootleg?)"},
        {"route16d",    "Route 16 (Sun Electronics, set 2)"},
        {"routex",      "Route X (bootleg)"},
        {"routexa",     "Route X (bootleg, set 2)"},
        {"royalmah",    "Royal Mahjong (Japan)"},
        {"rpatrol",     "River Patrol (Japan)"},
        {"rpatrolb",    "River Patrol (bootleg)"},
        {"rpatroln",    "River Patrol (Japan, unprotected)"},
        {"rpunch",      "Rabbit Punch (US)"},
        {"rranger",     "Rough Ranger (v2.0)"},
        {"rrangerb",    "Rough Ranger (v2.0, bootleg)"},
        {"rrreveng",    "Road Riot's Revenge (prototype)"},
        {"rrrevenp",    "Road Riot's Revenge (prototype alt)"},
        {"rsgun",       "Radiant Silvergun"},
        {"rshark",      "R-Shark"},
        {"rthun2",      "Rolling Thunder 2"},
        {"rthun2j",     "Rolling Thunder 2 (Japan)"},
        {"rthunder",    "Rolling Thunder (new version)"},
        {"rthunder0",   "Rolling Thunder (oldest)"},
        {"rthunder1",   "Rolling Thunder (rev 1)"},
        {"rthunder2",   "Rolling Thunder (rev 2)"},
        {"rthundera",   "Rolling Thunder (rev 3, hack)"},
        {"rthundro",    "Rolling Thunder (old version)"},
        {"rtype",       "R-Type (Japan)"},
        {"rtype2",      "R-Type II"},
        {"rtype2j",     "R-Type II (Japan)"},
        {"rtype2jc",    "R-Type II (Japan, revision C)"},
        {"rtypeb",      "R-Type (World bootleg)"},
        {"rtypej",      "R-Type (Japan)"},
        {"rtypejp",     "R-Type (Japan prototype)"},
        {"rtypelej",    "R-Type Leo (Japan rev. D)"},
        {"rtypeleo",    "R-Type Leo (World rev. C)"},
        {"rtypeleoj",   "R-Type Leo (Japan)"},
        {"rtypepj",     "R-Type (Japan prototype)"},
        {"rtypeu",      "R-Type (US)"},
        {"rugrats",     "Rug Rats"},
        {"rumba",       "Rumba Lumber"},
        {"rumblef",     "The Rumble Fish"},
        {"rumblef2",    "The Rumble Fish 2"},
        {"rumblefp",    "The Rumble Fish (prototype)"},
        {"rumblf2p",    "The Rumble Fish 2 (prototype)"},
        {"runark",      "Runark (Japan)"},
        {"runaway",     "Runaway (prototype)"},
        {"rundeep",     "Run Deep"},
        {"rungun",      "Run and Gun (World ver. EAA)"},
        {"rungun2",     "Run and Gun 2 (ver UAA)"},
        {"runguna",     "Run and Gun (ver EAA 1993 10.4)"},
        {"rungunad",    "Run and Gun (ver EAA 1993 10.4) (dual screen with demux adapter)"},
        {"rungunb",     "Run and Gun (ver EAA 1993 9.10, prototype?)"},
        {"rungunbd",    "Run and Gun (ver EAA 1993 9.10, prototype?) (dual screen with demux adapter)"},
        {"rungund",     "Run and Gun (ver EAA 1993 10.8) (dual screen with demux adapter)"},
        {"rungunu",     "Run and Gun (US ver. UAB)"},
        {"rungunua",    "Run and Gun (ver UBA 1993 10.8)"},
        {"rungunuad",   "Run and Gun (ver UBA 1993 10.8) (dual screen with demux adapter)"},
        {"rungunud",    "Run and Gun (ver UAB 1993 10.12, dedicated twin cabinet)"},
        {"runpuppy",    "Run Run Puppy"},
        {"runrun",      "Run Run (Do! Run Run bootleg)"},
        {"rushatck",    "Rush'n Attack"},
        {"rushbets",    "Rushing Beat Shura (SNES bootleg)"},
        {"rushcrsh",    "Rush and Crash (Japan)"},
        {"rushhero",    "Rushing Heroes (ver UAB)"},
        {"rvschool",    "Rival Schools (ASIA 971117)"},
        {"rygar",       "Rygar (US set 1)"},
        {"rygar2",      "Rygar (US set 2)"},
        {"rygar3",      "Rygar (US set 3 Old Version)"},
        {"rygarj",      "Argus no Senshi (Japan)"},
        {"rygarj2",     "Argus no Senshi (Japan set 2)"},
        {"ryorioh",     "Gourmet Battle Quiz Ryohrioh CooKing (Japan)"},
        {"ryouran",     "VS Mahjong Otome Ryouran"},
        {"ryujin",      "Ryu Jin (Japan)"},
        {"ryujina",     "Ryu Jin (Japan, ET910000A PCB)"},
        {"ryukendn",    "Ninja Ryukenden (Japan)"},
        {"ryukendna",   "Ninja Ryukenden (Japan, set 2)"},
        {"ryukyu",      "Ryukyu"},
        {"ryukyua",     "RyuKyu (Japan) (FD1094 317-5023)"},
        {"ryukyud",     "RyuKyu (Japan) (bootleg of FD1094 317-5023 set)"},
};

const char *lookup_r(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < A_SIZE(lookup_table); i++) {
        if (strcmp(lookup_table[i].name, name) == 0) {
            return lookup_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_r(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < A_SIZE(lookup_table); i++) {
        if (strstr(lookup_table[i].value, value)) {
            return lookup_table[i].name;
        }
    }
    return NULL;
}
