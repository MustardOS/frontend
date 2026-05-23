#pragma once

typedef enum {
    BT_TYPE_AUDIO_HEADSET = 0,
    BT_TYPE_AUDIO_HEADPHONES,
    BT_TYPE_AUDIO_SPEAKER,
    BT_TYPE_AUDIO_MICROPHONE,
    BT_TYPE_AUDIO_CARD,
    BT_TYPE_INPUT_GAMEPAD,
    BT_TYPE_INPUT_KEYBOARD,
    BT_TYPE_INPUT_MOUSE,
    BT_TYPE_INPUT_COMBO,
    BT_TYPE_INPUT_REMOTE,
    BT_TYPE_PHONE,
    BT_TYPE_COMPUTER,
    BT_TYPE_NETWORK,
    BT_TYPE_UNKNOWN,
    BT_TYPE_COUNT,
} bt_type_t;

struct bt_type_info {
    const char *key;
    const char *label;
};

extern const struct bt_type_info bt_type_infos[BT_TYPE_COUNT];
extern const char *const bt_type_keys[BT_TYPE_COUNT];

const char *bt_type_key(bt_type_t type);

const char *bt_type_label(bt_type_t type);

bt_type_t bt_type_from_string(const char *key);

bt_type_t bt_type_derive(const char *icon, const char *class_str, const char *uuids, const char *name);
