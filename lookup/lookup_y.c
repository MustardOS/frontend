#include <string.h>
#include "lookup.h"

typedef struct {
    const char *name;
    const char *value;
} LookupName;

static const LookupName lookup_table[] = {
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
        {"youjyudn",  "Youjyuden (Japan)"},
        {"youkaidk",  "Yokai Douchuuki (Japan new version)"},
        {"youkaidk1", "Youkai Douchuuki (Japan, old version (YD1))"},
        {"youkaidk2", "Youkai Douchuuki (Japan, new version (YD2, Rev B))"},
        {"youma",     "Youma Ninpou Chou (Japan)"},
        {"yoyospel",  "YoYo Spell (prototype)"},
        {"yukiwo",    "Yukiwo (World, prototype)"},
        {"yumefuda",  "Yumefuda [BET]"},
        {"yuyugogo",  "Yuuyu no Quiz de GO!GO! (Japan)"},
};

const char *lookup_y(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < sizeof(lookup_table) / sizeof(lookup_table[0]); i++) {
        if (strcmp(lookup_table[i].name, name) == 0) {
            return lookup_table[i].value;
        }
    }
    return NULL;
}

const char *r_lookup_y(const char *value) {
    if (!value) return NULL;
    for (size_t i = 0; i < sizeof(lookup_table) / sizeof(lookup_table[0]); i++) {
        if (strstr(lookup_table[i].value, value)) {
            return lookup_table[i].name;
        }
    }
    return NULL;
}
