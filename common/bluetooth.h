#pragma once

typedef enum {
    bt_type_audio_headset = 0,
    bt_type_audio_headphones,
    bt_type_audio_speaker,
    bt_type_audio_microphone,
    bt_type_audio_card,
    bt_type_input_gamepad,
    bt_type_input_keyboard,
    bt_type_input_mouse,
    bt_type_input_combo,
    bt_type_input_remote,
    bt_type_phone,
    bt_type_computer,
    bt_type_network,
    bt_type_unknown,
    bt_type_count,
} bt_type_t;

struct bt_type_info {
    const char *key;
    const char *label;
};

extern const struct bt_type_info bt_type_infos[bt_type_count];
extern const char *const bt_type_keys[bt_type_count];

const char *bt_type_key(bt_type_t type);

const char *bt_type_label(bt_type_t type);

bt_type_t bt_type_from_string(const char *key);

bt_type_t bt_type_derive(const char *icon, const char *class_str, const char *uuids, const char *name);
