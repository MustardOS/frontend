#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "bluetooth.h"
#include "language.h"
#include "strutil.h"

#define UUID_DUN          "00001103"
#define UUID_HSP          "00001108"
#define UUID_A2DP_SOURCE  "0000110a"
#define UUID_A2DP_SINK    "0000110b"
#define UUID_AVRCP_REMOTE "0000110e"
#define UUID_PANU         "00001115"
#define UUID_NAP          "00001116"
#define UUID_GN           "00001117"
#define UUID_HFP          "0000111e"
#define UUID_HID          "00001124"
#define UUID_PBAP_PCE     "0000112e"
#define UUID_PBAP_PSE     "0000112f"
#define UUID_MAP_MAS      "00001132"
#define UUID_MAP_MNS      "00001133"
#define UUID_GATT_HID     "00001812"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define KW_MATCH(map, hay) match_keyword((map), ARRAY_SIZE(map), (hay))

const struct bt_type_info bt_type_infos[BT_TYPE_COUNT] = {
        [BT_TYPE_AUDIO_HEADSET]    = {"audio-headset", lang.MUXBTDEV.TYPE_NAME.AUDIO_HEADSET},
        [BT_TYPE_AUDIO_HEADPHONES] = {"audio-headphones", lang.MUXBTDEV.TYPE_NAME.AUDIO_HEADPHONES},
        [BT_TYPE_AUDIO_SPEAKER]    = {"audio-speaker", lang.MUXBTDEV.TYPE_NAME.AUDIO_SPEAKER},
        [BT_TYPE_AUDIO_MICROPHONE] = {"audio-microphone", lang.MUXBTDEV.TYPE_NAME.AUDIO_MICROPHONE},
        [BT_TYPE_AUDIO_CARD]       = {"audio-card", lang.MUXBTDEV.TYPE_NAME.AUDIO_CARD},
        [BT_TYPE_INPUT_GAMEPAD]    = {"input-gamepad", lang.MUXBTDEV.TYPE_NAME.INPUT_GAMEPAD},
        [BT_TYPE_INPUT_KEYBOARD]   = {"input-keyboard", lang.MUXBTDEV.TYPE_NAME.INPUT_KEYBOARD},
        [BT_TYPE_INPUT_MOUSE]      = {"input-mouse", lang.MUXBTDEV.TYPE_NAME.INPUT_MOUSE},
        [BT_TYPE_INPUT_COMBO]      = {"input-combo", lang.MUXBTDEV.TYPE_NAME.INPUT_COMBO},
        [BT_TYPE_INPUT_REMOTE]     = {"input-remote", lang.MUXBTDEV.TYPE_NAME.INPUT_REMOTE},
        [BT_TYPE_PHONE]            = {"phone", lang.MUXBTDEV.TYPE_NAME.PHONE},
        [BT_TYPE_COMPUTER]         = {"computer", lang.MUXBTDEV.TYPE_NAME.COMPUTER},
        [BT_TYPE_NETWORK]          = {"network", lang.MUXBTDEV.TYPE_NAME.NETWORK},
        [BT_TYPE_UNKNOWN]          = {"unknown", lang.GENERIC.UNKNOWN},
};

const char *const bt_type_keys[BT_TYPE_COUNT] = {
        [BT_TYPE_AUDIO_HEADSET]    = "audio-headset",
        [BT_TYPE_AUDIO_HEADPHONES] = "audio-headphones",
        [BT_TYPE_AUDIO_SPEAKER]    = "audio-speaker",
        [BT_TYPE_AUDIO_MICROPHONE] = "audio-microphone",
        [BT_TYPE_AUDIO_CARD]       = "audio-card",
        [BT_TYPE_INPUT_GAMEPAD]    = "input-gamepad",
        [BT_TYPE_INPUT_KEYBOARD]   = "input-keyboard",
        [BT_TYPE_INPUT_MOUSE]      = "input-mouse",
        [BT_TYPE_INPUT_COMBO]      = "input-combo",
        [BT_TYPE_INPUT_REMOTE]     = "input-remote",
        [BT_TYPE_PHONE]            = "phone",
        [BT_TYPE_COMPUTER]         = "computer",
        [BT_TYPE_NETWORK]          = "network",
        [BT_TYPE_UNKNOWN]          = "unknown",
};

struct kw_map {
    const char *keyword;
    bt_type_t type;
};

static const struct kw_map audio_icon_map[] = {
        {"audio-headphones", BT_TYPE_AUDIO_HEADPHONES},
        {"headphone",        BT_TYPE_AUDIO_HEADPHONES},
        {"audio-headset",    BT_TYPE_AUDIO_HEADSET},
        {"headset",          BT_TYPE_AUDIO_HEADSET},
        {"audio-speakers",   BT_TYPE_AUDIO_SPEAKER},
        {"speaker",          BT_TYPE_AUDIO_SPEAKER},
        {"microphone",       BT_TYPE_AUDIO_MICROPHONE},
        {"audio-card",       BT_TYPE_AUDIO_CARD},
        {"audio",            BT_TYPE_AUDIO_CARD},
};

static const struct kw_map audio_name_map[] = {
        {"headphones",   BT_TYPE_AUDIO_HEADPHONES},
        {"headphone",    BT_TYPE_AUDIO_HEADPHONES},
        {"earphones",    BT_TYPE_AUDIO_HEADPHONES},
        {"earphone",     BT_TYPE_AUDIO_HEADPHONES},
        {"earbuds",      BT_TYPE_AUDIO_HEADPHONES},
        {"earbud",       BT_TYPE_AUDIO_HEADPHONES},
        {"airpods",      BT_TYPE_AUDIO_HEADPHONES},
        {"airpod",       BT_TYPE_AUDIO_HEADPHONES},
        {"buds",         BT_TYPE_AUDIO_HEADPHONES},
        {"wf-",          BT_TYPE_AUDIO_HEADPHONES},
        {"wh-",          BT_TYPE_AUDIO_HEADPHONES},
        {"quietcomfort", BT_TYPE_AUDIO_HEADPHONES},
        {"soundcore",    BT_TYPE_AUDIO_HEADPHONES},
        {"jabra",        BT_TYPE_AUDIO_HEADPHONES},
        {"beats",        BT_TYPE_AUDIO_HEADPHONES},
        {"sennheiser",   BT_TYPE_AUDIO_HEADPHONES},
        {"skullcandy",   BT_TYPE_AUDIO_HEADPHONES},
        {"qcy",          BT_TYPE_AUDIO_HEADPHONES},
        {"anker",        BT_TYPE_AUDIO_HEADPHONES},
        {"headset",      BT_TYPE_AUDIO_HEADSET},
        {"hands-free",   BT_TYPE_AUDIO_HEADSET},
        {"handsfree",    BT_TYPE_AUDIO_HEADSET},
        {"microphone",   BT_TYPE_AUDIO_MICROPHONE},
        {"mic",          BT_TYPE_AUDIO_MICROPHONE},
        {"soundbar",     BT_TYPE_AUDIO_SPEAKER},
        {"speaker",      BT_TYPE_AUDIO_SPEAKER},
        {"boombox",      BT_TYPE_AUDIO_SPEAKER},
        {"jbl",          BT_TYPE_AUDIO_SPEAKER},
        {"marshall",     BT_TYPE_AUDIO_SPEAKER},
        {"harman",       BT_TYPE_AUDIO_SPEAKER},
        {"ue boom",      BT_TYPE_AUDIO_SPEAKER},
        {"car audio",    BT_TYPE_AUDIO_SPEAKER},
        {"car kit",      BT_TYPE_AUDIO_SPEAKER},
        {"hifi",         BT_TYPE_AUDIO_SPEAKER},
        {"hi-fi",        BT_TYPE_AUDIO_SPEAKER},
};

static const struct kw_map hid_icon_map[] = {
        {"input-gaming", BT_TYPE_INPUT_GAMEPAD},
        {"gamepad",      BT_TYPE_INPUT_GAMEPAD},
        {"joystick",     BT_TYPE_INPUT_GAMEPAD},
        {"keyboard",     BT_TYPE_INPUT_KEYBOARD},
        {"mouse",        BT_TYPE_INPUT_MOUSE},
        {"remote",       BT_TYPE_INPUT_REMOTE},
};

static const struct kw_map hid_name_map[] = {
        {"8bitdo",          BT_TYPE_INPUT_GAMEPAD},
        {"xbox wireless",   BT_TYPE_INPUT_GAMEPAD},
        {"xbox controller", BT_TYPE_INPUT_GAMEPAD},
        {"dualshock",       BT_TYPE_INPUT_GAMEPAD},
        {"dualsense",       BT_TYPE_INPUT_GAMEPAD},
        {"switch pro",      BT_TYPE_INPUT_GAMEPAD},
        {"pro controller",  BT_TYPE_INPUT_GAMEPAD},
        {"joy-con",         BT_TYPE_INPUT_GAMEPAD},
        {"joycon",          BT_TYPE_INPUT_GAMEPAD},
        {"controller",      BT_TYPE_INPUT_GAMEPAD},
        {"gamepad",         BT_TYPE_INPUT_GAMEPAD},
        {"game pad",        BT_TYPE_INPUT_GAMEPAD},
        {"joystick",        BT_TYPE_INPUT_GAMEPAD},
        {"arcade stick",    BT_TYPE_INPUT_GAMEPAD},
        {"ps4",             BT_TYPE_INPUT_GAMEPAD},
        {"ps5",             BT_TYPE_INPUT_GAMEPAD},
        {"xbox",            BT_TYPE_INPUT_GAMEPAD},
        {"keyboard",        BT_TYPE_INPUT_KEYBOARD},
        {"keychron",        BT_TYPE_INPUT_KEYBOARD},
        {"mx keys",         BT_TYPE_INPUT_KEYBOARD},
        {"magic keyboard",  BT_TYPE_INPUT_KEYBOARD},
        {"magic mouse",     BT_TYPE_INPUT_MOUSE},
        {"mx master",       BT_TYPE_INPUT_MOUSE},
        {"trackball",       BT_TYPE_INPUT_MOUSE},
        {"mouse",           BT_TYPE_INPUT_MOUSE},
        {"media remote",    BT_TYPE_INPUT_REMOTE},
        {"remote",          BT_TYPE_INPUT_REMOTE},
};

static const struct kw_map icon_map[] = {
        {"audio-headphones", BT_TYPE_AUDIO_HEADPHONES},
        {"headphone",        BT_TYPE_AUDIO_HEADPHONES},
        {"audio-headset",    BT_TYPE_AUDIO_HEADSET},
        {"headset",          BT_TYPE_AUDIO_HEADSET},
        {"audio-speakers",   BT_TYPE_AUDIO_SPEAKER},
        {"speaker",          BT_TYPE_AUDIO_SPEAKER},
        {"microphone",       BT_TYPE_AUDIO_MICROPHONE},
        {"audio-card",       BT_TYPE_AUDIO_CARD},
        {"audio",            BT_TYPE_AUDIO_CARD},
        {"input-gaming",     BT_TYPE_INPUT_GAMEPAD},
        {"gamepad",          BT_TYPE_INPUT_GAMEPAD},
        {"joystick",         BT_TYPE_INPUT_GAMEPAD},
        {"keyboard",         BT_TYPE_INPUT_KEYBOARD},
        {"mouse",            BT_TYPE_INPUT_MOUSE},
        {"remote",           BT_TYPE_INPUT_REMOTE},
        {"phone",            BT_TYPE_PHONE},
        {"tablet",           BT_TYPE_PHONE},
        {"computer",         BT_TYPE_COMPUTER},
        {"network-wireless", BT_TYPE_NETWORK},
        {"network",          BT_TYPE_NETWORK},
};

static const struct kw_map name_map[] = {
        {"headphones",      BT_TYPE_AUDIO_HEADPHONES},
        {"headphone",       BT_TYPE_AUDIO_HEADPHONES},
        {"earphones",       BT_TYPE_AUDIO_HEADPHONES},
        {"earphone",        BT_TYPE_AUDIO_HEADPHONES},
        {"earbuds",         BT_TYPE_AUDIO_HEADPHONES},
        {"earbud",          BT_TYPE_AUDIO_HEADPHONES},
        {"airpods",         BT_TYPE_AUDIO_HEADPHONES},
        {"airpod",          BT_TYPE_AUDIO_HEADPHONES},
        {"buds",            BT_TYPE_AUDIO_HEADPHONES},
        {"wf-",             BT_TYPE_AUDIO_HEADPHONES},
        {"wh-",             BT_TYPE_AUDIO_HEADPHONES},
        {"quietcomfort",    BT_TYPE_AUDIO_HEADPHONES},
        {"soundcore",       BT_TYPE_AUDIO_HEADPHONES},
        {"jabra",           BT_TYPE_AUDIO_HEADPHONES},
        {"beats",           BT_TYPE_AUDIO_HEADPHONES},
        {"sennheiser",      BT_TYPE_AUDIO_HEADPHONES},
        {"skullcandy",      BT_TYPE_AUDIO_HEADPHONES},
        {"qcy",             BT_TYPE_AUDIO_HEADPHONES},
        {"anker",           BT_TYPE_AUDIO_HEADPHONES},
        {"headset",         BT_TYPE_AUDIO_HEADSET},
        {"hands-free",      BT_TYPE_AUDIO_HEADSET},
        {"handsfree",       BT_TYPE_AUDIO_HEADSET},
        {"microphone",      BT_TYPE_AUDIO_MICROPHONE},
        {"mic",             BT_TYPE_AUDIO_MICROPHONE},
        {"soundbar",        BT_TYPE_AUDIO_SPEAKER},
        {"speaker",         BT_TYPE_AUDIO_SPEAKER},
        {"boombox",         BT_TYPE_AUDIO_SPEAKER},
        {"jbl",             BT_TYPE_AUDIO_SPEAKER},
        {"marshall",        BT_TYPE_AUDIO_SPEAKER},
        {"harman",          BT_TYPE_AUDIO_SPEAKER},
        {"ue boom",         BT_TYPE_AUDIO_SPEAKER},
        {"car audio",       BT_TYPE_AUDIO_SPEAKER},
        {"car kit",         BT_TYPE_AUDIO_SPEAKER},
        {"hifi",            BT_TYPE_AUDIO_SPEAKER},
        {"hi-fi",           BT_TYPE_AUDIO_SPEAKER},
        {"8bitdo",          BT_TYPE_INPUT_GAMEPAD},
        {"xbox wireless",   BT_TYPE_INPUT_GAMEPAD},
        {"xbox controller", BT_TYPE_INPUT_GAMEPAD},
        {"dualshock",       BT_TYPE_INPUT_GAMEPAD},
        {"dualsense",       BT_TYPE_INPUT_GAMEPAD},
        {"switch pro",      BT_TYPE_INPUT_GAMEPAD},
        {"pro controller",  BT_TYPE_INPUT_GAMEPAD},
        {"joy-con",         BT_TYPE_INPUT_GAMEPAD},
        {"joycon",          BT_TYPE_INPUT_GAMEPAD},
        {"controller",      BT_TYPE_INPUT_GAMEPAD},
        {"gamepad",         BT_TYPE_INPUT_GAMEPAD},
        {"game pad",        BT_TYPE_INPUT_GAMEPAD},
        {"joystick",        BT_TYPE_INPUT_GAMEPAD},
        {"arcade stick",    BT_TYPE_INPUT_GAMEPAD},
        {"ps4",             BT_TYPE_INPUT_GAMEPAD},
        {"ps5",             BT_TYPE_INPUT_GAMEPAD},
        {"xbox",            BT_TYPE_INPUT_GAMEPAD},
        {"keyboard",        BT_TYPE_INPUT_KEYBOARD},
        {"keychron",        BT_TYPE_INPUT_KEYBOARD},
        {"mx keys",         BT_TYPE_INPUT_KEYBOARD},
        {"magic keyboard",  BT_TYPE_INPUT_KEYBOARD},
        {"magic mouse",     BT_TYPE_INPUT_MOUSE},
        {"mx master",       BT_TYPE_INPUT_MOUSE},
        {"trackball",       BT_TYPE_INPUT_MOUSE},
        {"mouse",           BT_TYPE_INPUT_MOUSE},
        {"media remote",    BT_TYPE_INPUT_REMOTE},
        {"remote",          BT_TYPE_INPUT_REMOTE},
        {"iphone",          BT_TYPE_PHONE},
        {"android",         BT_TYPE_PHONE},
        {"pixel",           BT_TYPE_PHONE},
        {"galaxy",          BT_TYPE_PHONE},
        {"phone",           BT_TYPE_PHONE},
        {"ipad",            BT_TYPE_PHONE},
        {"tablet",          BT_TYPE_PHONE},
        {"macbook",         BT_TYPE_COMPUTER},
        {"laptop",          BT_TYPE_COMPUTER},
        {"desktop",         BT_TYPE_COMPUTER},
        {"computer",        BT_TYPE_COMPUTER},
        {"raspberry pi",    BT_TYPE_COMPUTER},
        {"router",          BT_TYPE_NETWORK},
        {"hotspot",         BT_TYPE_NETWORK},
        {"network",         BT_TYPE_NETWORK},
};

static bt_type_t safe_type(bt_type_t type) {
    if (type >= BT_TYPE_COUNT) return BT_TYPE_UNKNOWN;

    return type;
}

static bt_type_t match_keyword(const struct kw_map *map, size_t n, const char *haystack) {
    if (!map || !*haystack) return BT_TYPE_UNKNOWN;

    for (size_t i = 0; i < n; i++) {
        if (strstr(haystack, map[i].keyword)) return map[i].type;
    }

    return BT_TYPE_UNKNOWN;
}

static inline char *str_tolower_dup(const char *str) {
    return str_tolower((char *) (str ? str : ""));
}

static int has_uuid(const char *uuids, const char *uuid) {
    return strstr(uuids, uuid) != NULL;
}

static bt_type_t derive_audio_type(const char *icon, const char *name) {
    bt_type_t t = KW_MATCH(audio_icon_map, icon);
    if (t != BT_TYPE_UNKNOWN) return t;

    char *lower = str_tolower_dup(name ? name : "");
    t = lower ? KW_MATCH(audio_name_map, lower) : BT_TYPE_UNKNOWN;
    free(lower);

    return t;
}

static bt_type_t derive_hid_type(const char *icon, const char *name) {
    bt_type_t t = KW_MATCH(hid_icon_map, icon);
    if (t != BT_TYPE_UNKNOWN) return t;

    char *lower = str_tolower_dup(name ? name : "");
    t = lower ? KW_MATCH(hid_name_map, lower) : BT_TYPE_UNKNOWN;
    free(lower);

    return t != BT_TYPE_UNKNOWN ? t : BT_TYPE_INPUT_GAMEPAD;
}

static bt_type_t derive_from_class(long cod) {
    int major = (int) ((cod >> 8) & 0x1f);
    int minor = (int) ((cod >> 2) & 0x3f);

    switch (major) {
        case 0x01:
            return BT_TYPE_COMPUTER;
        case 0x02:
            return BT_TYPE_PHONE;
        case 0x03:
            return BT_TYPE_NETWORK;
        case 0x04:
            if (minor == 0x01 || minor == 0x02) return BT_TYPE_AUDIO_HEADSET;
            if (minor == 0x04) return BT_TYPE_AUDIO_MICROPHONE;
            if (minor == 0x05) return BT_TYPE_AUDIO_SPEAKER;
            if (minor == 0x06) return BT_TYPE_AUDIO_HEADPHONES;
            if (minor == 0x07 || minor == 0x08 || minor == 0x0a) return BT_TYPE_AUDIO_CARD;
            return BT_TYPE_AUDIO_CARD;
        case 0x05: {
            int combo = minor & 0x30;
            int sub = minor & 0x0f;

            if (sub == 0x01 || sub == 0x02) return BT_TYPE_INPUT_GAMEPAD;
            if (sub == 0x03) return BT_TYPE_INPUT_REMOTE;
            if (combo == 0x30) return BT_TYPE_INPUT_COMBO;
            if (combo == 0x10) return BT_TYPE_INPUT_KEYBOARD;
            if (combo == 0x20) return BT_TYPE_INPUT_MOUSE;

            return BT_TYPE_INPUT_GAMEPAD;
        }
        default:
            break;
    }

    return BT_TYPE_UNKNOWN;
}

const char *bt_type_key(bt_type_t type) {
    type = safe_type(type);

    if (!bt_type_infos[type].key) return bt_type_infos[BT_TYPE_UNKNOWN].key;

    return bt_type_infos[type].key;
}

const char *bt_type_label(bt_type_t type) {
    type = safe_type(type);

    if (!bt_type_infos[type].label) return bt_type_infos[BT_TYPE_UNKNOWN].label;

    return bt_type_infos[type].label;
}

bt_type_t bt_type_from_string(const char *key) {
    if (!key || !*key) return BT_TYPE_UNKNOWN;

    for (int i = 0; i < BT_TYPE_COUNT; i++) {
        if (bt_type_infos[i].key && strcasecmp(bt_type_infos[i].key, key) == 0) return (bt_type_t) i;
        if (bt_type_infos[i].label && strcasecmp(bt_type_infos[i].label, key) == 0) return (bt_type_t) i;
    }

    return BT_TYPE_UNKNOWN;
}

bt_type_t bt_type_derive(const char *icon, const char *class_str, const char *uuids, const char *name) {
    bt_type_t result = BT_TYPE_UNKNOWN;

    char *r_icon = str_tolower_dup(icon ? icon : "");
    char *r_uuids = str_tolower_dup(uuids ? uuids : "");

    if (!r_icon || !r_uuids) goto done;

    if (*r_uuids) {
        int has_hfp = has_uuid(r_uuids, UUID_HFP);
        int has_hsp = has_uuid(r_uuids, UUID_HSP);
        int has_a2dp_source = has_uuid(r_uuids, UUID_A2DP_SOURCE);
        int has_a2dp_sink = has_uuid(r_uuids, UUID_A2DP_SINK);
        int has_hid = has_uuid(r_uuids, UUID_HID) || has_uuid(r_uuids, UUID_GATT_HID);

        if (has_hfp || has_hsp || has_a2dp_sink || has_a2dp_source) {
            bt_type_t t = derive_audio_type(r_icon, name);
            if (t != BT_TYPE_UNKNOWN) {
                result = t;
                goto done;
            }

            if (has_hfp || has_hsp) {
                result = BT_TYPE_AUDIO_HEADSET;
                goto done;
            }
            if (has_a2dp_sink) {
                result = BT_TYPE_AUDIO_HEADPHONES;
                goto done;
            }
        }

        if (has_hid) {
            result = derive_hid_type(r_icon, name);
            goto done;
        }

        if (has_uuid(r_uuids, UUID_PANU) ||
            has_uuid(r_uuids, UUID_NAP) ||
            has_uuid(r_uuids, UUID_GN)) {
            result = BT_TYPE_NETWORK;
            goto done;
        }

        if (has_uuid(r_uuids, UUID_DUN) ||
            has_uuid(r_uuids, UUID_PBAP_PCE) ||
            has_uuid(r_uuids, UUID_PBAP_PSE) ||
            has_uuid(r_uuids, UUID_MAP_MAS) ||
            has_uuid(r_uuids, UUID_MAP_MNS)) {
            result = BT_TYPE_PHONE;
            goto done;
        }

        if (has_uuid(r_uuids, UUID_AVRCP_REMOTE)) {
            bt_type_t t = KW_MATCH(hid_icon_map, r_icon);
            if (t == BT_TYPE_UNKNOWN) {
                char *lower = str_tolower_dup(name ? name : "");
                if (lower) t = KW_MATCH(hid_name_map, lower);
                free(lower);
            }
            result = (t != BT_TYPE_UNKNOWN) ? t : BT_TYPE_INPUT_REMOTE;
            goto done;
        }
    }

    if (*r_icon) {
        bt_type_t t = KW_MATCH(icon_map, r_icon);
        if (t != BT_TYPE_UNKNOWN) {
            result = t;
            goto done;
        }
    }

    if (class_str && *class_str) {
        char *endp;
        long cod = strtol(class_str, &endp, 0);
        if (endp != class_str) {
            bt_type_t t = derive_from_class(cod);
            if (t != BT_TYPE_UNKNOWN) {
                result = t;
                goto done;
            }
        }
    }

    if (name && *name) {
        char *lower = str_tolower_dup(name);
        if (lower) {
            result = KW_MATCH(name_map, lower);
            free(lower);
        }
    }

    done:
    free(r_icon);
    free(r_uuids);
    return result;
}
