#include <string.h>
#include "../common/common.h"
#include "lookup.h"

typedef struct {
    const char *name;
    const char *value;
} LookupName;

static const LookupName lookup_table[] = {
        {"qad",        "Quiz and Dragons (US 920701)"},
        {"qadj",       "Quiz and Dragons (Japan 940921)"},
        {"qadjr",      "Quiz & Dragons: Capcom Quiz Game (Japan Resale Ver. 940921)"},
        {"qb3",        "QB-3 (prototype)"},
        {"qbert",      "Q*bert (US set 1)"},
        {"qberta",     "Q*bert (US set 2)"},
        {"qbertj",     "Q*bert (Japan)"},
        {"qbertjp",    "Q*bert (Japan)"},
        {"qbertqub",   "Q*bert's Qubes"},
        {"qberttst",   "Q*bert (early test version)"},
        {"qcrayon",    "Quiz Crayon Shinchan (Japan)"},
        {"qcrayon2",   "Crayon Shinchan Orato Asobo (Japan)"},
        {"qdrmfgp",    "Quiz Do Re Mi Fa Grand Prix (Japan)"},
        {"qdrmfgp2",   "Quiz Do Re Mi Fa Grand Prix 2 - Shin-Kyoku Nyuukadayo (Japan)"},
        {"qgakumon",   "Quiz Gakumon no Susume (Japan ver. JA2 Type L)"},
        {"qgh",        "Quiz Ghost Hunter"},
        {"qix",        "Qix (set 1)"},
        {"qix2",       "Qix II (Tournament)"},
        {"qixa",       "Qix (set 2)"},
        {"qixb",       "Qix (set 3)"},
        {"qixo",       "Qix (set 3, earlier)"},
        {"qjinsei",    "Quiz Jinsei Gekijoh (Japan)"},
        {"qmegamis",   "Quiz Ah Megamisama (JPN, USA, EXP, KOR, AUS)"},
        {"qmhayaku",   "Quiz-Mahjong Hayaku Yatteyo! (Japan)"},
        {"qndream",    "Quiz Nanairo Dreams: Nijiirochou no Kiseki (Japan 96086)"},
        {"qos",        "A Question of Sport (set 1, 39-960-107)"},
        {"qosa",       "A Question of Sport (set 2, 39-960-099)"},
        {"qosb",       "A Question of Sport (set 3, 39-960-089)"},
        {"qotn",       "Queen of the Nile (B - 13-05-97, NSW/ACT)"},
        {"qrouka",     "Quiz Rouka Ni Tattenasai"},
        {"qsangoku",   "Quiz Sangokushi (Japan)"},
        {"qsww",       "Quiz Syukudai wo Wasuremashita"},
        {"qtheater",   "Quiz Theater - 3tsu no Monogatari (Japan)"},
        {"qtono1",     "Quiz Tonosama no Yabou (Japan)"},
        {"qtono2",     "Quiz Tonosama no Yabou 2 Zenkoku-ban (Japan 950123)"},
        {"qtono2j",    "Quiz Tonosama no Yabou 2: Zenkoku-ban (Japan 950123)"},
        {"qtorimon",   "Quiz Torimonochou (Japan)"},
        {"quaak",      "Quaak (bootleg of Frogger)"},
        {"quake",      "Quake Arcade Tournament (Release Beta 2)"},
        {"quantum",    "Quantum (rev 2)"},
        {"quantum1",   "Quantum (rev 1)"},
        {"quantump",   "Quantum (prototype)"},
        {"quaquiz2",   "Quadro Quiz II"},
        {"quarterb",   "Quarterback"},
        {"quarterba",  "Quarterback (rev 2)"},
        {"quarterbc",  "Quarterback (rev 1, cocktail)"},
        {"quartet",    "Quartet (Rev A, 8751 315-5194)"},
        {"quartet2",   "Quartet 2 (8751 317-0010)"},
        {"quartet2a",  "Quartet 2 (unprotected)"},
        {"quarteta",   "Quartet (8751 315-5194)"},
        {"quartetj",   "Quartet (8751 315-5194)"},
        {"quarth",     "Quarth (Japan)"},
        {"quartrba",   "Quarterback (set 2)"},
        {"quartt2j",   "Quartet 2 (unprotected)"},
        {"quasar",     "Quasar (set 1)"},
        {"quasara",    "Quasar (set 2)"},
        {"quester",    "Quester (Japan)"},
        {"questers",   "Quester Special Edition (Japan)"},
        {"quidgrid",   "Ten Quid Grid (v1.2)"},
        {"quidgrid2",  "Ten Quid Grid (v2.4)"},
        {"quidgrid2d", "Ten Quid Grid (v2.4, Datapak)"},
        {"quidgridd",  "Ten Quid Grid (v1.2, Datapak)"},
        {"quiz",       "Quiz (Revision 2)"},
        {"quiz18k",    "Miyasu Nonki no Quiz 18-Kin"},
        {"quiz211",    "Quiz (Revision 2.11)"},
        {"quiz365",    "Quiz 365 (Hong Kong and Taiwan)"},
        {"quiz365t",   "Quiz 365 (Hong Kong & Taiwan)"},
        {"quizard",    "Quizard 1.7"},
        {"quizchq",    "Quiz Channel Question (Ver 1.00) (Japan)"},
        {"quizchqk",   "Quiz Channel Question (Korea, Ver 1.10)"},
        {"quizchql",   "Quiz Channel Question (Ver 1.23) (Taiwan[Q])"},
        {"quizdai2",   "Quiz Meitantei Neo and Geo - Quiz Daisousa Sen part 2"},
        {"quizdais",   "Quiz Daisousa Sen - The Last Count Down"},
        {"quizdaisk",  "Quiz Salibtamjeong - The Last Count Down (Korean localized Quiz Daisousa Sen)"},
        {"quizdna",    "Quiz DNA no Hanran (Japan)"},
        {"quizf1",     "Quiz F-1 1,2finish"},
        {"quizhq",     "Quiz H.Q. (Japan)"},
        {"quizhuhu",   "Moriguchi Hiroko no Quiz de Hyuu!Hyuu! (Japan)"},
        {"quizkof",    "Quiz King of Fighters"},
        {"quizkofk",   "Quiz King of Fighters (Korea)"},
        {"quizmeku",   "Quiz Mekurumeku Story"},
        {"quizmoon",   "Quiz Bisyoujo Senshi Sailor Moon - Chiryoku Tairyoku Toki no Un"},
        {"quizmstr",   "Quizmaster (German)"},
        {"quizo",      "Quiz Olympic (set 1)"},
        {"quizoa",     "Quiz Olympic (set 2)"},
        {"quizpani",   "Quiz Panicuru Fantasy"},
        {"quizpun2",   "Quiz Punch 2"},
        {"quizqgd",    "Quiz Keitai Q mode (GDL-0017)"},
        {"quizrd22",   "Quizard 2.2"},
        {"quizrd32",   "Quizard 3.2"},
        {"quizrr41",   "Quizard Rainbow 4.1"},
        {"quiztou",    "Nettou! Gekitou! Quiztou!! (Japan)"},
        {"quiztvqq",   "Quiz TV Gassyuukoku QandQ (Japan)"},
        {"quizvadr",   "Quizvaders (39-360-078)"},
        {"quizvid",    "Video Quiz"},
        {"qwak",       "Qwak (prototype)"},
        {"qwakprot",   "Qwak (prototype)"},
        {"qzchikyu",   "Quiz Chikyu Bouei Gun (Japan)"},
        {"qzkklgy2",   "Quiz Kokology 2"},
        {"qzkklogy",   "Quiz Kokology"},
        {"qzquest",    "Quiz Quest - Hime to Yuusha no Monogatari (Japan)"},
        {"qzshowby",   "Quiz Sekai wa SHOW by shobai (Japan)"},
};

const char *lookup_q(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < A_SIZE(lookup_table); i++) {
        if (strcmp(lookup_table[i].name, name) == 0) {
            return lookup_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_q(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < A_SIZE(lookup_table); i++) {
        if (strstr(lookup_table[i].value, value)) {
            return lookup_table[i].name;
        }
    }
    return NULL;
}
