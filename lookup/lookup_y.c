#include <string.h>
#include "../common/common.h"
#include "lookup.h"

const LookupName lookup_y_table[] = {
        {"yachtmn",   "Yachtsman"},
        {"yamagchi",  "Go Go Mr. Yamaguchi - Yuke Yuke Yamaguchi-kun"},
        {"yamato",    "Yamato (US)"},
        {"yamato2",   "Yamato (World[Q])"},
        {"yamyam",    "Yam! Yam![Q]"},
        {"yamyamk",   "Yam! Yam! (Korea)"},
        {"yanchamr",  "Kaiketsu Yanchamaru (Japan)"},
        {"yankeedo",  "Yankee DO!"},
        {"yard",      "10 Yard Fight (Japan)"},
        {"yarunara",  "Mahjong Yarunara (Japan)"},
        {"yellowcbb", "Yellow Cab (bootleg)"},
        {"yellowcbj", "Yellow Cab (Japan)"},
        {"yesnoj",    "Yes-No Sinri Tokimeki Chart"},
        {"yiear",     "Yie Ar Kung-Fu (set 1)"},
        {"yiear2",    "Yie Ar Kung-Fu (set 2)"},
        {"yieartf",   "Yie Ar Kung-Fu (bootleg GX361 conversion)"},
        {"ymcapsul",  "Yu-Gi-Oh Monster Capsule"},
        {"yokaidko",  "Yokai Douchuuki (Japan old version)"},
        {"yosakdoa",  "Yosaku To Donbei (set 2)"},
        {"yosakdon",  "Yosaku To Donbei (set 1)"},
        {"yosakdona", "Yosaku To Donbei (set 2)"},
        {"yosimoto",  "Mahjong Yoshimoto Gekijou (Japan)"},
        {"youjyudn",  "Youjyuden (Japan)"},
        {"youkaidk",  "Yokai Douchuuki (Japan new version)"},
        {"youkaidk1", "Youkai Douchuuki (Japan, old version (YD1))"},
        {"youkaidk2", "Youkai Douchuuki (Japan, new version (YD2, Rev B))"},
        {"youkaidko", "Yokai Douchuuki (Japan old version)"},
        {"youma",     "Youma Ninpou Chou (Japan)"},
        {"youma2",    "Youma Ninpou Chou (Japan, alt)"},
        {"youmab",    "Youma Ninpou Chou (Game Electronics bootleg, set 1)"},
        {"youmab2",   "Youma Ninpou Chou (Game Electronics bootleg, set 2)"},
        {"yoyoshkn",  "Yo-Yo Shuriken (HB)"},
        {"yoyospel",  "YoYo Spell (prototype)"},
        {"yujan",     "Yu-Jan"},
        {"yuka",      "Yu-Ka"},
        {"yukiwo",    "Yukiwo (World, prototype)"},
        {"yumefuda",  "Yumefuda [BET]"},
        {"yuyugogo",  "Yuuyu no Quiz de GO!GO! (Japan)"},
};

const size_t lookup_y_count = A_SIZE(lookup_y_table);

const char *lookup_y(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < lookup_y_count; i++) {
        if (strcmp(lookup_y_table[i].name, name) == 0) {
            return lookup_y_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_y(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < lookup_y_count; i++) {
        if (strstr(lookup_y_table[i].value, value)) {
            return lookup_y_table[i].name;
        }
    }
    return NULL;
}

void lookup_y_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_y_count; i++) {
        if (strcasestr(lookup_y_table[i].name, term))
            emit(lookup_y_table[i].name, lookup_y_table[i].value, udata);
    }
}

void r_lookup_y_multi(const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata) {
    if (!term) return;
    for (size_t i = 0; i < lookup_y_count; i++) {
        if (strcasestr(lookup_y_table[i].value, term))
            emit(lookup_y_table[i].name, lookup_y_table[i].value, udata);
    }
}
