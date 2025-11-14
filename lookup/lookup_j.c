#include <string.h>
#include "../common/common.h"
#include "lookup.h"

const LookupName lookup_j_table[] = {
        {"jack",       "Jack the Giantkiller (set 1)"},
        {"jack2",      "Jack the Giantkiller (set 2)"},
        {"jack3",      "Jack the Giantkiller (set 3)"},
        {"jackal",     "Jackal (World)"},
        {"jackalbl",   "Jackal (bootleg, Rotary Joystick)"},
        {"jackalj",    "Tokushu Butai Jackal (Japan)"},
        {"jackalr",    "Jackal (World, Rotary Joystick)"},
        {"jackler",    "Jackler (bootleg of Jungler)"},
        {"jackrab2",   "Jack Rabbit (set 2)"},
        {"jackrabs",   "Jack Rabbit (special)"},
        {"jackrabt",   "Jack Rabbit (set 1)"},
        {"jackrabt2",  "Jack Rabbit (set 2)"},
        {"jackrabts",  "Jack Rabbit (special)"},
        {"jailbrek",   "Jail Break"},
        {"jailbrekb",  "Jail Break (bootleg)"},
        {"jajamaru",   "Vs. Ninja Jajamaru Kun (Japan)"},
        {"jambo",      "Jambo! Safari (Rev A)"},
        {"janbari",    "Mahjong Janjan Baribari (Japan)"},
        {"jangou",     "Jangou [BET] (Japan)"},
        {"janjans1",   "Lovely Pop Mahjong Jan Jan Shimasyo (Japan)"},
        {"janjans2",   "Lovely Pop Mahjong JangJang Shimasho 2 (Japan)"},
        {"janoh",      "Jan Oh (set 1)"},
        {"janoha",     "Jan Oh (set 2)"},
        {"janptr96",   "Janputer '96 (Japan)"},
        {"janputer",   "New Double Bet Mahjong (Japan)"},
        {"janshi",     "Janshi"},
        {"janshin",    "Jyanshin Densetsu - Quest of Jongmaster"},
        {"jansou",     "Jansou (set 1)"},
        {"jansoua",    "Jansou (set 2)"},
        {"jantotsu",   "4nin-uchi Mahjong Jantotsu"},
        {"jantouki",   "Jong Tou Ki (Japan)"},
        {"janyoup2",   "Janyou Part II (ver 7.03, July 1 1983)"},
        {"jchan",      "Jackie Chan - Kung Fu Master"},
        {"jchan2",     "Jackie Chan in Fists of Fire"},
        {"jchana",     "Jackie Chan - The Kung-Fu Master (rev 3?)"},
        {"jcross",     "Jumping Cross"},
        {"jcrossa",    "Jumping Cross (set 2)"},
        {"jdredd",     "Judge Dredd (Rev C)"},
        {"jdreddb",    "Judge Dredd (Rev B)"},
        {"jdreddp",    "Judge Dredd (rev LA1, prototype)"},
        {"jedi",       "Return of the Jedi"},
        {"jetwave",    "Jet Wave (EAB, Euro v1.04)"},
        {"jetwavej",   "Jet Wave (JAB, Japan v1.04)"},
        {"jgakuen",    "Justice Gakuen (JAPAN 971117)"},
        {"jigkmgri",   "Jigoku Meguri (Japan)"},
        {"jigkmgria",  "Jigoku Meguri (Japan)"},
        {"jin",        "Jin"},
        {"jingling",   "Jingling Jiazu Genie 2000"},
        {"jingystm",   "Jingi Storm - The Arcade (Japan) (GDL-0037)"},
        {"jitsupro",   "Jitsuryoku!! Pro Yakyuu (Japan)"},
        {"jituroku",   "Jitsuroku Maru-chi Mahjong (Japan)"},
        {"jjack",      "Jumping Jack"},
        {"jjparad2",   "Jan Jan Paradise 2"},
        {"jjparads",   "Jan Jan Paradise"},
        {"jjsquawk",   "J. J. Squawkers"},
        {"jjsquawkb",  "J. J. Squawkers (bootleg)"},
        {"jjsquawkb2", "J. J. Squawkers (bootleg, Blandia Conversion)"},
        {"jjsquawko",  "J. J. Squawkers (older)"},
        {"jleague",    "The J.League 1994 (Japan)"},
        {"jleagueo",   "The J.League 1994 (Japan)"},
        {"jmpbreak",   "Jumping Break (set 1)"},
        {"jmpbreaka",  "Jumping Break (set 2)"},
        {"jnero",      "Johnny Nero Action Hero"},
        {"jngolady",   "Jangou Lady (Japan)"},
        {"jockeygp",   "jockey gp"},
        {"joemac",     "Tatakae Genshizin Joe and Mac (Japan)"},
        {"joemacr",    "Joe and Mac Returns (World, Version 1.1)"},
        {"joemacra",   "Joe and Mac Returns (World, Version 1.0)"},
        {"joemacrj",   "Joe & Mac Returns (Japan, Version 1.2, 1994.06.06)"},
        {"jogakuen",   "Mahjong Jogakuen (Japan)"},
        {"joinem",     "Joinem"},
        {"jojo",       "JoJo's Venture (Europe 990128)"},
        {"jojoa",      "JoJo's Venture (Asia 990128)"},
        {"jojoan",     "JoJo's Venture / JoJo no Kimyouna Bouken (Asia, 981202, NO CD)"},
        {"jojoar1",    "JoJo's Venture (Asia 990108)"},
        {"jojoar2",    "JoJo's Venture (Asia 981202)"},
        {"jojoba",     "JoJo's Bizarre Adventure (Europe 991015, NO CD)"},
        {"jojobajr1",  "JoJo no Kimyou na Bouken: Mirai e no Isan (Japan 990927)"},
        {"jojobajr2",  "JoJo no Kimyou na Bouken: Mirai e no Isan (Japan 990913)"},
        {"jojoban",    "JoJo no Kimyou na Bouken: Mirai e no Isan (Japan 991015, NO CD)"},
        {"jojobane",   "JoJo's Bizarre Adventure: Heritage for the Future / JoJo no Kimyouna Bouken: Miraie no Isan (Euro, 990913, NO CD)"},
        {"jojobaner1", "JoJo's Bizarre Adventure (Europe 990927, NO CD)"},
        {"jojobaner2", "JoJo's Bizarre Adventure (Europe 990913, NO CD)"},
        {"jojobanr1",  "JoJo no Kimyou na Bouken: Mirai e no Isan (Japan 990927, NO CD)"},
        {"jojobanr2",  "JoJo no Kimyou na Bouken: Mirai e no Isan (Japan 990913, NO CD)"},
        {"jojobanrb",  "JoJo's Bizarre Adventure (Rainbow Edition v1.0.1 2021)"},
        {"jojobar1",   "JoJo's Bizarre Adventure (Europe 990927)"},
        {"jojobar2",   "JoJo's Bizarre Adventure (Europe 990913)"},
        {"jojoj",      "JoJo no Kimyou na Bouken (Japan 990128)"},
        {"jojojr1",    "JoJo no Kimyou na Bouken (Japan 990108)"},
        {"jojojr2",    "JoJo no Kimyou na Bouken (Japan 981202)"},
        {"jojon",      "JoJo's Venture (Asia 990128, NO CD)"},
        {"jojonr1",    "JoJo's Venture (Asia 990108, NO CD)"},
        {"jojonr2",    "JoJo's Venture (Asia 981202, NO CD)"},
        {"jojor1",     "JoJo's Venture (Europe 990108)"},
        {"jojor2",     "JoJo's Venture (Europe 981202)"},
        {"jojou",      "JoJo's Venture (USA 990128)"},
        {"jojour1",    "JoJo's Venture (USA 990108)"},
        {"jojour2",    "JoJo's Venture (USA 981202)"},
        {"jollyjgr",   "Jolly Jogger"},
        {"jolycdat",   "Jolly Card (Austrian, Funworld)"},
        {"jongbou",    "Mahjong Block Jongbou (Japan)"},
        {"jongbou2",   "Mahjong Block Jongbou 2 (Japan)"},
        {"jongkyo",    "Jongkyo"},
        {"jongpute",   "Jongputer"},
        {"jongtei",    "Mahjong Jong-Tei (Japan, ver. NM532-01)"},
        {"josvolly",   "Joshi Volleyball"},
        {"journey",    "Journey"},
        {"joust",      "Joust (White-Green label)"},
        {"joust2",     "Joust 2 - Survival of the Fittest (set 1)"},
        {"joust2r1",   "Joust 2 - Survival of the Fittest (revision 1)"},
        {"joustr",     "Joust (Solid Red label)"},
        {"joustwr",    "Joust (White-Red label)"},
        {"jousty",     "Joust (Yellow label)"},
        {"joyfulr",    "Joyful Road (Japan)"},
        {"joyjoy",     "Puzzled / Joy Joy Kid"},
        {"joyman",     "Joyman"},
        {"jpark",      "Jurassic Park"},
        {"jpark3",     "Jurassic Park 3 (ver EBC)"},
        {"jparkj",     "Jurassic Park (Japan, Rev A, Deluxe)"},
        {"jparkja",    "Jurassic Park (Japan, Deluxe)"},
        {"jparkjc",    "Jurassic Park (Japan, Rev A, Conversion)"},
        {"jpopnics",   "Jumping Pop (Nics, Korean hack of Plump Pop)"},
        {"jrking",     "Junior King (bootleg of Donkey Kong Jr.)"},
        {"jrpacman",   "Jr. Pac-Man"},
        {"jrpacmanf",  "Jr. Pac-Man (speedup hack)"},
        {"jrpacmbl",   "Jr. Pac-Man (Pengo hardware, bootleg)"},
        {"jrpacmnf",   "Jr. Pac-Man (speedup hack)"},
        {"jsk",        "Joryuu Syougi Kyoushitsu (Japan)"},
        {"jspecter",   "Jatre Specter (set 1)"},
        {"jspecter2",  "Jatre Specter (set 2)"},
        {"jspectr2",   "Jatre Specter (set 2)"},
        {"jt104",      "JT 104 / NinjaKun Ashura no Shou"},
        {"juju",       "JuJu Densetsu (Japan)"},
        {"jujub",      "JuJu Densetsu (Playmark bootleg)"},
        {"jujuba",     "JuJu Densetsu (Japan, bootleg)"},
        {"jumbogod",   "Jumbo Godzilla"},
        {"jumpbug",    "Jump Bug"},
        {"jumpbugb",   "Jump Bug (bootleg)"},
        {"jumpcoas",   "Jump Coaster"},
        {"jumpcoasa",  "Jump Coaster"},
        {"jumpcoast",  "Jump Coaster (Taito)"},
        {"jumping",    "Jumping"},
        {"jumpinga",   "Jumping (set 2)"},
        {"jumpingi",   "Jumping (set 3, Imnoe PCB)"},
        {"jumpjump",   "Jump Jump"},
        {"jumpkids",   "Jump Kids"},
        {"jumpkun",    "Jump Kun (prototype)"},
        {"jumppop",    "Jumping Pop (set 1)"},
        {"jumppope",   "Jumping Pop (set 2)"},
        {"jumpshot",   "Jump Shot"},
        {"jumpshotp",  "Jump Shot Engineering Sample"},
        {"jungleby",   "Jungle Boy (bootleg)"},
        {"jungleh",    "Jungle Hunt (US)"},
        {"junglehbr",  "Jungle Hunt (Brazil)"},
        {"junglek",    "Jungle King (Japan)"},
        {"junglekas",  "Jungle King (alternate sound)"},
        {"junglekj2",  "Jungle King (Japan, earlier)"},
        {"junglekj2a", "Jungle King (Japan, earlier, alt)"},
        {"jungler",    "Jungler"},
        {"junglero",   "Jungler (Olympia)"},
        {"junglers",   "Jungler (Stern)"},
        {"junglhbr",   "Jungle Hunt (Brazil)"},
        {"junglkj2",   "Jungle King (Japan, earlier)"},
        {"junofrst",   "Juno First"},
        {"junofrstg",  "Juno First (Gottlieb)"},
        {"junofstg",   "Juno First (Gottlieb)"},
        {"jurassic99", "Jurassic 99 (Cadillacs and Dinosaurs bootleg with EM78P447AP)"},
        {"jyangoku",   "Jyangokushi: Haoh no Saihai (Japan 990527)"},
        {"jyuohki",    "Jyuohki (Japan)"},
        {"jzth",       "Juezhan Tianhuang"},
};

const size_t lookup_j_count = A_SIZE(lookup_j_table);

const char *lookup_j(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < lookup_j_count; i++) {
        if (strcmp(lookup_j_table[i].name, name) == 0) {
            return lookup_j_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_j(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < lookup_j_count; i++) {
        if (strstr(lookup_j_table[i].value, value)) {
            return lookup_j_table[i].name;
        }
    }
    return NULL;
}

void lookup_j_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_j_count; i++) {
        if (strcasestr(lookup_j_table[i].name, term))
            emit(lookup_j_table[i].name, lookup_j_table[i].value, udata);
    }
}

void r_lookup_j_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_j_count; i++) {
        if (strcasestr(lookup_j_table[i].value, term))
            emit(lookup_j_table[i].name, lookup_j_table[i].value, udata);
    }
}
