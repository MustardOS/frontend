#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "bluetooth.h"
#include "language.h"
#include "strutil.h"

#define UUID_DUN          "00001103"
#define UUID_HSP          "00001108"
#define UUID_A2_DP_SOURCE "0000110a"
#define UUID_A2_DP_SINK   "0000110b"
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

#define ARRAY_SIZE(a)      (sizeof(a) / sizeof((a)[0]))
#define KW_MATCH(map, hay) match_keyword((map), ARRAY_SIZE(map), (hay))

const struct bt_type_info bt_type_infos[bt_type_count] = {
    [bt_type_audio_headset] = {"audio-headset", lang.muxbtdev.type_name.audio_headset},
    [bt_type_audio_headphones] = {"audio-headphones", lang.muxbtdev.type_name.audio_headphones},
    [bt_type_audio_speaker] = {"audio-speaker", lang.muxbtdev.type_name.audio_speaker},
    [bt_type_audio_microphone] = {"audio-microphone", lang.muxbtdev.type_name.audio_microphone},
    [bt_type_audio_card] = {"audio-card", lang.muxbtdev.type_name.audio_card},
    [bt_type_input_gamepad] = {"input-gamepad", lang.muxbtdev.type_name.input_gamepad},
    [bt_type_input_keyboard] = {"input-keyboard", lang.muxbtdev.type_name.input_keyboard},
    [bt_type_input_mouse] = {"input-mouse", lang.muxbtdev.type_name.input_mouse},
    [bt_type_input_combo] = {"input-combo", lang.muxbtdev.type_name.input_combo},
    [bt_type_input_remote] = {"input-remote", lang.muxbtdev.type_name.input_remote},
    [bt_type_phone] = {"phone", lang.muxbtdev.type_name.phone},
    [bt_type_computer] = {"computer", lang.muxbtdev.type_name.computer},
    [bt_type_network] = {"network", lang.muxbtdev.type_name.network},
    [bt_type_unknown] = {"unknown", lang.generic.unknown},
};

const char *const bt_type_keys[bt_type_count] = {
    [bt_type_audio_headset] = "audio-headset",
    [bt_type_audio_headphones] = "audio-headphones",
    [bt_type_audio_speaker] = "audio-speaker",
    [bt_type_audio_microphone] = "audio-microphone",
    [bt_type_audio_card] = "audio-card",
    [bt_type_input_gamepad] = "input-gamepad",
    [bt_type_input_keyboard] = "input-keyboard",
    [bt_type_input_mouse] = "input-mouse",
    [bt_type_input_combo] = "input-combo",
    [bt_type_input_remote] = "input-remote",
    [bt_type_phone] = "phone",
    [bt_type_computer] = "computer",
    [bt_type_network] = "network",
    [bt_type_unknown] = "unknown",
};

typedef struct kw_map {
    const char *keyword;
    bt_type_t type;
} kw_map;

static const kw_map audio_icon_map[] = {
    {"audio-headphones", bt_type_audio_headphones},
    {"headphone", bt_type_audio_headphones},
    {"audio-headset", bt_type_audio_headset},
    {"headset", bt_type_audio_headset},
    {"audio-speakers", bt_type_audio_speaker},
    {"speaker", bt_type_audio_speaker},
    {"microphone", bt_type_audio_microphone},
    {"audio-card", bt_type_audio_card},
    {"audio", bt_type_audio_card},
};

static const kw_map audio_name_map[] = {
    {"headphones", bt_type_audio_headphones}, {"headphone", bt_type_audio_headphones},
    {"earphones", bt_type_audio_headphones},  {"earphone", bt_type_audio_headphones},
    {"earbuds", bt_type_audio_headphones},    {"earbud", bt_type_audio_headphones},
    {"airpods", bt_type_audio_headphones},    {"airpod", bt_type_audio_headphones},
    {"buds", bt_type_audio_headphones},       {"wf-", bt_type_audio_headphones},
    {"wh-", bt_type_audio_headphones},        {"quietcomfort", bt_type_audio_headphones},
    {"soundcore", bt_type_audio_headphones},  {"jabra", bt_type_audio_headphones},
    {"beats", bt_type_audio_headphones},      {"sennheiser", bt_type_audio_headphones},
    {"skullcandy", bt_type_audio_headphones}, {"qcy", bt_type_audio_headphones},
    {"anker", bt_type_audio_headphones},      {"headset", bt_type_audio_headset},
    {"hands-free", bt_type_audio_headset},    {"handsfree", bt_type_audio_headset},
    {"microphone", bt_type_audio_microphone}, {"mic", bt_type_audio_microphone},
    {"soundbar", bt_type_audio_speaker},      {"speaker", bt_type_audio_speaker},
    {"boombox", bt_type_audio_speaker},       {"jbl", bt_type_audio_speaker},
    {"marshall", bt_type_audio_speaker},      {"harman", bt_type_audio_speaker},
    {"ue boom", bt_type_audio_speaker},       {"car audio", bt_type_audio_speaker},
    {"car kit", bt_type_audio_speaker},       {"hifi", bt_type_audio_speaker},
    {"hi-fi", bt_type_audio_speaker},
};

static const kw_map hid_icon_map[] = {
    {"input-gaming", bt_type_input_gamepad}, {"gamepad", bt_type_input_gamepad}, {"joystick", bt_type_input_gamepad},
    {"keyboard", bt_type_input_keyboard},    {"mouse", bt_type_input_mouse},     {"remote", bt_type_input_remote},
};

static const kw_map hid_name_map[] = {
    {"8bitdo", bt_type_input_gamepad},
    {"xbox wireless", bt_type_input_gamepad},
    {"xbox controller", bt_type_input_gamepad},
    {"dualshock", bt_type_input_gamepad},
    {"dualsense", bt_type_input_gamepad},
    {"switch pro", bt_type_input_gamepad},
    {"pro controller", bt_type_input_gamepad},
    {"joy-con", bt_type_input_gamepad},
    {"joycon", bt_type_input_gamepad},
    {"controller", bt_type_input_gamepad},
    {"gamepad", bt_type_input_gamepad},
    {"game pad", bt_type_input_gamepad},
    {"joystick", bt_type_input_gamepad},
    {"arcade stick", bt_type_input_gamepad},
    {"ps4", bt_type_input_gamepad},
    {"ps5", bt_type_input_gamepad},
    {"xbox", bt_type_input_gamepad},
    {"keyboard", bt_type_input_keyboard},
    {"keychron", bt_type_input_keyboard},
    {"mx keys", bt_type_input_keyboard},
    {"magic keyboard", bt_type_input_keyboard},
    {"magic mouse", bt_type_input_mouse},
    {"mx master", bt_type_input_mouse},
    {"trackball", bt_type_input_mouse},
    {"mouse", bt_type_input_mouse},
    {"media remote", bt_type_input_remote},
    {"remote", bt_type_input_remote},
};

static const kw_map icon_map[] = {
    {"audio-headphones", bt_type_audio_headphones},
    {"headphone", bt_type_audio_headphones},
    {"audio-headset", bt_type_audio_headset},
    {"headset", bt_type_audio_headset},
    {"audio-speakers", bt_type_audio_speaker},
    {"speaker", bt_type_audio_speaker},
    {"microphone", bt_type_audio_microphone},
    {"audio-card", bt_type_audio_card},
    {"audio", bt_type_audio_card},
    {"input-gaming", bt_type_input_gamepad},
    {"gamepad", bt_type_input_gamepad},
    {"joystick", bt_type_input_gamepad},
    {"keyboard", bt_type_input_keyboard},
    {"mouse", bt_type_input_mouse},
    {"remote", bt_type_input_remote},
    {"phone", bt_type_phone},
    {"tablet", bt_type_phone},
    {"computer", bt_type_computer},
    {"network-wireless", bt_type_network},
    {"network", bt_type_network},
};

static const kw_map name_map[] = {
    {"headphones", bt_type_audio_headphones},
    {"headphone", bt_type_audio_headphones},
    {"earphones", bt_type_audio_headphones},
    {"earphone", bt_type_audio_headphones},
    {"earbuds", bt_type_audio_headphones},
    {"earbud", bt_type_audio_headphones},
    {"airpods", bt_type_audio_headphones},
    {"airpod", bt_type_audio_headphones},
    {"buds", bt_type_audio_headphones},
    {"wf-", bt_type_audio_headphones},
    {"wh-", bt_type_audio_headphones},
    {"quietcomfort", bt_type_audio_headphones},
    {"soundcore", bt_type_audio_headphones},
    {"jabra", bt_type_audio_headphones},
    {"beats", bt_type_audio_headphones},
    {"sennheiser", bt_type_audio_headphones},
    {"skullcandy", bt_type_audio_headphones},
    {"qcy", bt_type_audio_headphones},
    {"anker", bt_type_audio_headphones},
    {"headset", bt_type_audio_headset},
    {"hands-free", bt_type_audio_headset},
    {"handsfree", bt_type_audio_headset},
    {"microphone", bt_type_audio_microphone},
    {"mic", bt_type_audio_microphone},
    {"soundbar", bt_type_audio_speaker},
    {"speaker", bt_type_audio_speaker},
    {"boombox", bt_type_audio_speaker},
    {"jbl", bt_type_audio_speaker},
    {"marshall", bt_type_audio_speaker},
    {"harman", bt_type_audio_speaker},
    {"ue boom", bt_type_audio_speaker},
    {"car audio", bt_type_audio_speaker},
    {"car kit", bt_type_audio_speaker},
    {"hifi", bt_type_audio_speaker},
    {"hi-fi", bt_type_audio_speaker},
    {"8bitdo", bt_type_input_gamepad},
    {"xbox wireless", bt_type_input_gamepad},
    {"xbox controller", bt_type_input_gamepad},
    {"dualshock", bt_type_input_gamepad},
    {"dualsense", bt_type_input_gamepad},
    {"switch pro", bt_type_input_gamepad},
    {"pro controller", bt_type_input_gamepad},
    {"joy-con", bt_type_input_gamepad},
    {"joycon", bt_type_input_gamepad},
    {"controller", bt_type_input_gamepad},
    {"gamepad", bt_type_input_gamepad},
    {"game pad", bt_type_input_gamepad},
    {"joystick", bt_type_input_gamepad},
    {"arcade stick", bt_type_input_gamepad},
    {"ps4", bt_type_input_gamepad},
    {"ps5", bt_type_input_gamepad},
    {"xbox", bt_type_input_gamepad},
    {"keyboard", bt_type_input_keyboard},
    {"keychron", bt_type_input_keyboard},
    {"mx keys", bt_type_input_keyboard},
    {"magic keyboard", bt_type_input_keyboard},
    {"magic mouse", bt_type_input_mouse},
    {"mx master", bt_type_input_mouse},
    {"trackball", bt_type_input_mouse},
    {"mouse", bt_type_input_mouse},
    {"media remote", bt_type_input_remote},
    {"remote", bt_type_input_remote},
    {"iphone", bt_type_phone},
    {"android", bt_type_phone},
    {"pixel", bt_type_phone},
    {"galaxy", bt_type_phone},
    {"phone", bt_type_phone},
    {"ipad", bt_type_phone},
    {"tablet", bt_type_phone},
    {"macbook", bt_type_computer},
    {"laptop", bt_type_computer},
    {"desktop", bt_type_computer},
    {"computer", bt_type_computer},
    {"raspberry pi", bt_type_computer},
    {"router", bt_type_network},
    {"hotspot", bt_type_network},
    {"network", bt_type_network},
};

static bt_type_t safe_type(const bt_type_t type) {
    if (type >= bt_type_count) return bt_type_unknown;

    return type;
}

static bt_type_t match_keyword(const kw_map *map, const size_t n, const char *haystack) {
    if (!map || !*haystack) return bt_type_unknown;

    for (size_t i = 0; i < n; i++) {
        if (strstr(haystack, map[i].keyword)) return map[i].type;
    }

    return bt_type_unknown;
}

static char *str_tolower_dup(const char *str) {
    return str_tolower((char *) (str ? str : ""));
}

static int has_uuid(const char *uuids, const char *uuid) {
    return strstr(uuids, uuid) != NULL;
}

static bt_type_t derive_audio_type(const char *icon, const char *name) {
    bt_type_t t = KW_MATCH(audio_icon_map, icon);
    if (t != bt_type_unknown) return t;

    char *lower = str_tolower_dup(name ? name : "");
    t = lower ? KW_MATCH(audio_name_map, lower) : bt_type_unknown;
    free(lower);

    return t;
}

static bt_type_t derive_hid_type(const char *icon, const char *name) {
    bt_type_t t = KW_MATCH(hid_icon_map, icon);
    if (t != bt_type_unknown) return t;

    char *lower = str_tolower_dup(name ? name : "");
    t = lower ? KW_MATCH(hid_name_map, lower) : bt_type_unknown;
    free(lower);

    return t != bt_type_unknown ? t : bt_type_input_gamepad;
}

static bt_type_t derive_from_class(const long cod) {
    const int major = (int) (cod >> 8 & 0x1f);
    const int minor = (int) (cod >> 2 & 0x3f);

    switch (major) {
        case 0x01:
            return bt_type_computer;
        case 0x02:
            return bt_type_phone;
        case 0x03:
            return bt_type_network;
        case 0x04:
            if (minor == 0x01 || minor == 0x02) return bt_type_audio_headset;
            if (minor == 0x04) return bt_type_audio_microphone;
            if (minor == 0x05) return bt_type_audio_speaker;
            if (minor == 0x06) return bt_type_audio_headphones;
            if (minor == 0x07 || minor == 0x08 || minor == 0x0a) return bt_type_audio_card;
            return bt_type_audio_card;
        case 0x05: {
            const int combo = minor & 0x30;
            const int sub = minor & 0x0f;

            if (sub == 0x01 || sub == 0x02) return bt_type_input_gamepad;
            if (sub == 0x03) return bt_type_input_remote;
            if (combo == 0x30) return bt_type_input_combo;
            if (combo == 0x10) return bt_type_input_keyboard;
            if (combo == 0x20) return bt_type_input_mouse;

            return bt_type_input_gamepad;
        }
        default:
            break;
    }

    return bt_type_unknown;
}

const char *bt_type_key(bt_type_t type) {
    type = safe_type(type);

    if (!bt_type_infos[type].key) return bt_type_infos[bt_type_unknown].key;

    return bt_type_infos[type].key;
}

const char *bt_type_label(bt_type_t type) {
    type = safe_type(type);

    if (!bt_type_infos[type].label) return bt_type_infos[bt_type_unknown].label;

    return bt_type_infos[type].label;
}

bt_type_t bt_type_from_string(const char *key) {
    if (!key || !*key) return bt_type_unknown;

    for (int i = 0; i < bt_type_count; i++) {
        if (bt_type_infos[i].key && strcasecmp(bt_type_infos[i].key, key) == 0) return (bt_type_t) i;
        if (bt_type_infos[i].label && strcasecmp(bt_type_infos[i].label, key) == 0) return (bt_type_t) i;
    }

    return bt_type_unknown;
}

bt_type_t bt_type_derive(const char *icon, const char *class_str, const char *uuids, const char *name) {
    bt_type_t result = bt_type_unknown;

    char *r_icon = str_tolower_dup(icon ? icon : "");
    char *r_uuids = str_tolower_dup(uuids ? uuids : "");

    if (!r_icon || !r_uuids) goto done;

    if (*r_uuids) {
        const int has_hfp = has_uuid(r_uuids, UUID_HFP);
        const int has_hsp = has_uuid(r_uuids, UUID_HSP);
        const int has_a2_dp_source = has_uuid(r_uuids, UUID_A2_DP_SOURCE);
        const int has_a2_dp_sink = has_uuid(r_uuids, UUID_A2_DP_SINK);
        const int has_hid = has_uuid(r_uuids, UUID_HID) || has_uuid(r_uuids, UUID_GATT_HID);

        if (has_hfp || has_hsp || has_a2_dp_sink || has_a2_dp_source) {
            const bt_type_t t = derive_audio_type(r_icon, name);
            if (t != bt_type_unknown) {
                result = t;
                goto done;
            }

            if (has_hfp || has_hsp) {
                result = bt_type_audio_headset;
                goto done;
            }
            if (has_a2_dp_sink) {
                result = bt_type_audio_headphones;
                goto done;
            }
        }

        if (has_hid) {
            result = derive_hid_type(r_icon, name);
            goto done;
        }

        if (has_uuid(r_uuids, UUID_PANU) || has_uuid(r_uuids, UUID_NAP) || has_uuid(r_uuids, UUID_GN)) {
            result = bt_type_network;
            goto done;
        }

        if (has_uuid(r_uuids, UUID_DUN) || has_uuid(r_uuids, UUID_PBAP_PCE) || has_uuid(r_uuids, UUID_PBAP_PSE)
            || has_uuid(r_uuids, UUID_MAP_MAS) || has_uuid(r_uuids, UUID_MAP_MNS)) {
            result = bt_type_phone;
            goto done;
        }

        if (has_uuid(r_uuids, UUID_AVRCP_REMOTE)) {
            bt_type_t t = KW_MATCH(hid_icon_map, r_icon);
            if (t == bt_type_unknown) {
                char *lower = str_tolower_dup(name ? name : "");
                if (lower) t = KW_MATCH(hid_name_map, lower);
                free(lower);
            }
            result = t != bt_type_unknown ? t : bt_type_input_remote;
            goto done;
        }
    }

    if (*r_icon) {
        const bt_type_t t = KW_MATCH(icon_map, r_icon);
        if (t != bt_type_unknown) {
            result = t;
            goto done;
        }
    }

    if (class_str && *class_str) {
        char *endp;
        const long cod = strtol(class_str, &endp, 0);
        if (endp != class_str) {
            const bt_type_t t = derive_from_class(cod);
            if (t != bt_type_unknown) {
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
