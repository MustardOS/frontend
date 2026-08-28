#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "portable.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include "../device_rumble.h"
#include "../../common/calibration.h"
#include "../../common/uinput.h"
#include "../../drivers/evdev/evdev.h"
#include "../../drivers/rumble/sysfs/rumble_sysfs.h"

#define PRIVATE_DEVICE_DIR "/dev/muinput"
#define PIXEL_SWAP_PATH    "/run/muinput/input_dpad_to_joystick"

enum portable_layout_id {
    LAYOUT_STANDARD,
    LAYOUT_G350,
    LAYOUT_PIXEL2,
    LAYOUT_ZERO28,
};

enum portable_rumble_id {
    RUMBLE_AXP,
    RUMBLE_RK_PWM,
    RUMBLE_VITA_PWM,
};

struct portable_profile {
    const char *id;
    const char *source_name;
    const struct gamepad_desc *gamepad;
    enum portable_layout_id layout;
    enum portable_rumble_id rumble;
    int has_sticks;
    int dpad_switch;
};

struct portable_state {
    struct gamepad *gamepad;
    const struct portable_profile *profile;
    struct evdev_source source;
    struct device_rumble_state rumble;
    struct axis_state *axes[4];
    int axis_values[4];
    int hat_x;
    int hat_y;
    int dpad_left;
    int dpad_right;
    int dpad_up;
    int dpad_down;
    int dpad_to_stick;
};

static const unsigned short PORTABLE_KEYS[] = {
    KEY_VOLUMEDOWN, KEY_VOLUMEUP, BTN_SOUTH,  BTN_EAST,  BTN_NORTH, BTN_WEST,   BTN_TL,     BTN_TR,
    BTN_TL2,        BTN_TR2,      BTN_SELECT, BTN_START, BTN_MODE,  BTN_THUMBL, BTN_THUMBR,
};

static const struct gamepad_abs_desc PORTABLE_AXES_STICKLESS[] = {
    {.code = ABS_X, .min = -32767, .max = 32767},
    {.code = ABS_Y, .min = -32767, .max = 32767},
    {.code = ABS_HAT0X, .min = -1, .max = 1},
    {.code = ABS_HAT0Y, .min = -1, .max = 1},
};

static const struct gamepad_abs_desc PORTABLE_AXES_STICKS[] = {
    {.code = ABS_X, .min = -32767, .max = 32767},  {.code = ABS_Y, .min = -32767, .max = 32767},
    {.code = ABS_RX, .min = -32767, .max = 32767}, {.code = ABS_RY, .min = -32767, .max = 32767},
    {.code = ABS_HAT0X, .min = -1, .max = 1},      {.code = ABS_HAT0Y, .min = -1, .max = 1},
};

#define PORTABLE_DESC(_product, _axes)                                                                                 \
    {                                                                                                                  \
        .name = MUOS_GAMEPAD_NAME,                                                                                     \
        .id = {BUS_VIRTUAL, MUOS_INPUT_VENDOR, (_product), MUOS_INPUT_VERSION},                                        \
        .keys = PORTABLE_KEYS,                                                                                         \
        .key_count = sizeof(PORTABLE_KEYS) / sizeof(PORTABLE_KEYS[0]),                                                 \
        .axes = (_axes),                                                                                               \
        .axis_count = sizeof(_axes) / sizeof((_axes)[0]),                                                              \
        .ff_effects_max = DEVICE_RUMBLE_EFFECT_SLOTS,                                                                  \
        .enable_ff_rumble = 1,                                                                                         \
    }

static const struct gamepad_desc GCS_H36S_GAMEPAD = PORTABLE_DESC(MUOS_PRODUCT_GCS_H36S, PORTABLE_AXES_STICKS);
static const struct gamepad_desc MGX_ZERO28_GAMEPAD = PORTABLE_DESC(MUOS_PRODUCT_MGX_ZERO28, PORTABLE_AXES_STICKS);
static const struct gamepad_desc RK_G350_V_GAMEPAD = PORTABLE_DESC(MUOS_PRODUCT_RK_G350_V, PORTABLE_AXES_STICKS);
static const struct gamepad_desc RK_PIXEL_2_GAMEPAD = PORTABLE_DESC(MUOS_PRODUCT_RK_PIXEL_2, PORTABLE_AXES_STICKLESS);
static const struct gamepad_desc RG_VITA_PRO_GAMEPAD = PORTABLE_DESC(MUOS_PRODUCT_RG_VITA_PRO, PORTABLE_AXES_STICKS);

#undef PORTABLE_DESC

static const struct portable_profile PORTABLE_PROFILES[] = {
    {"gcs-h36s", "adc_gamepad", &GCS_H36S_GAMEPAD, LAYOUT_STANDARD, RUMBLE_AXP, 1, 0},
    {"mgx-zero28", "magicx-input", &MGX_ZERO28_GAMEPAD, LAYOUT_ZERO28, RUMBLE_AXP, 1, 0},
    {"rk-g350-v", "g350_joypad", &RK_G350_V_GAMEPAD, LAYOUT_G350, RUMBLE_RK_PWM, 1, 0},
    {"rk-pixel-2", "pixel2_joypad", &RK_PIXEL_2_GAMEPAD, LAYOUT_PIXEL2, RUMBLE_RK_PWM, 0, 1},
    {"rg-vita-pro", "retrogame_joypad", &RG_VITA_PRO_GAMEPAD, LAYOUT_STANDARD, RUMBLE_VITA_PWM, 1, 0},
};

static const struct portable_profile *find_profile(const char *id) {
    for (size_t i = 0; i < sizeof(PORTABLE_PROFILES) / sizeof(PORTABLE_PROFILES[0]); ++i) {
        if (id && strcmp(id, PORTABLE_PROFILES[i].id) == 0) return &PORTABLE_PROFILES[i];
    }
    return NULL;
}

static int input_class_name(int fd, const char *prefix, char *name, size_t name_size) {
    struct stat info;
    if (fstat(fd, &info) < 0) return -1;
    DIR *directory = opendir("/sys/class/input");
    if (!directory) return -1;
    int found = -1;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strncmp(entry->d_name, prefix, strlen(prefix)) != 0) continue;
        char path[PATH_MAX];
        int length = snprintf(path, sizeof(path), "/sys/class/input/%s/dev", entry->d_name);
        if (length <= 0 || (size_t) length >= sizeof(path)) continue;
        FILE *device = fopen(path, "r");
        if (!device) continue;
        unsigned int device_major = 0;
        unsigned int device_minor = 0;
        int parsed = fscanf(device, "%u:%u", &device_major, &device_minor);
        fclose(device);
        if (parsed == 2 && device_major == major(info.st_rdev) && device_minor == minor(info.st_rdev)) {
            if (snprintf(name, name_size, "%s", entry->d_name) < (int) name_size) found = 0;
            break;
        }
    }
    closedir(directory);
    return found;
}

static int joystick_class_name(const char *event_name, char *name, size_t name_size) {
    char event_path[PATH_MAX];
    char event_real[PATH_MAX];
    int length = snprintf(event_path, sizeof(event_path), "/sys/class/input/%s/device", event_name);
    if (length <= 0 || (size_t) length >= sizeof(event_path) || !realpath(event_path, event_real)) return -1;
    DIR *directory = opendir("/sys/class/input");
    if (!directory) return -1;
    int found = -1;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strncmp(entry->d_name, "js", 2) != 0) continue;
        char joystick_path[PATH_MAX];
        char joystick_real[PATH_MAX];
        length = snprintf(joystick_path, sizeof(joystick_path), "/sys/class/input/%s/device", entry->d_name);
        if (length <= 0 || (size_t) length >= sizeof(joystick_path) || !realpath(joystick_path, joystick_real))
            continue;
        if (strcmp(event_real, joystick_real) == 0) {
            if (snprintf(name, name_size, "%s", entry->d_name) < (int) name_size) found = 0;
            break;
        }
    }
    closedir(directory);
    return found;
}

static void remove_public_links(const char *event_name, const char *joystick_name) {
    DIR *directory = opendir("/dev/input/by-path");
    if (!directory) return;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char path[PATH_MAX];
        char target[PATH_MAX];
        int length = snprintf(path, sizeof(path), "/dev/input/by-path/%s", entry->d_name);
        if (length <= 0 || (size_t) length >= sizeof(path)) continue;
        ssize_t target_length = readlink(path, target, sizeof(target) - 1);
        if (target_length < 0) continue;
        target[target_length] = '\0';
        const char *target_name = strrchr(target, '/');
        target_name = target_name ? target_name + 1 : target;
        if (strcmp(target_name, event_name) == 0 || (joystick_name && strcmp(target_name, joystick_name) == 0))
            unlink(path);
    }
    closedir(directory);
}

static int isolate_source(struct portable_state *state, int verbose) {
    char event_name[32];
    if (input_class_name(state->source.fd, "event", event_name, sizeof(event_name)) < 0) return -1;
    char joystick_name[32];
    int has_joystick = joystick_class_name(event_name, joystick_name, sizeof(joystick_name)) == 0;
    if (mkdir(PRIVATE_DEVICE_DIR, 0755) < 0 && errno != EEXIST) return -1;
    char private_event[128];
    char private_joystick[128];
    if (snprintf(private_event, sizeof(private_event), PRIVATE_DEVICE_DIR "/%s-source", state->profile->id)
            >= (int) sizeof(private_event)
        || snprintf(private_joystick, sizeof(private_joystick), PRIVATE_DEVICE_DIR "/%s-joystick", state->profile->id)
               >= (int) sizeof(private_joystick)) {
        return -1;
    }
    if (strcmp(state->source.path, private_event) != 0 && rename(state->source.path, private_event) < 0) return -1;
    snprintf(state->source.path, sizeof(state->source.path), "%s", private_event);
    if (has_joystick) {
        char source_joystick[PATH_MAX];
        int length = snprintf(source_joystick, sizeof(source_joystick), "/dev/input/%s", joystick_name);
        if (length <= 0 || (size_t) length >= sizeof(source_joystick)
            || (access(source_joystick, F_OK) == 0 && rename(source_joystick, private_joystick) < 0)) {
            return -1;
        }
    }
    remove_public_links(event_name, has_joystick ? joystick_name : NULL);
    if (verbose) fprintf(stderr, "Isolated %s source under %s\n", state->profile->id, PRIVATE_DEVICE_DIR);
    return 0;
}

static int open_source(struct portable_state *state, int verbose) {
    const char *configured_path = getenv("MUOS_INPUT_SOURCE");
    const char *configured_name = getenv("MUOS_INPUT_SOURCE_NAME");
    char private_path[128];
    int length = snprintf(private_path, sizeof(private_path), PRIVATE_DEVICE_DIR "/%s-source", state->profile->id);
    if (length <= 0 || (size_t) length >= sizeof(private_path)) return -1;
    if (configured_path && *configured_path) {
        return evdev_source_open(
            &state->source, configured_path, configured_name && *configured_name ? configured_name : NULL, 1, verbose
        );
    }
    if (access(private_path, F_OK) == 0) {
        return evdev_source_open(&state->source, private_path, NULL, 1, verbose);
    }
    return evdev_source_open(
        &state->source, NULL, configured_name && *configured_name ? configured_name : state->profile->source_name, 1,
        verbose
    );
}

static int portable_probe(void) {
    for (size_t i = 0; i < sizeof(PORTABLE_PROFILES) / sizeof(PORTABLE_PROFILES[0]); ++i) {
        if (evdev_source_probe(NULL, PORTABLE_PROFILES[i].source_name)) return 1;
    }
    return 0;
}

static unsigned short map_dpad_key(unsigned short code) {
    switch (code) {
        case BTN_DPAD_LEFT:
        case KEY_LEFT:
            return ABS_HAT0X;
        case BTN_DPAD_RIGHT:
        case KEY_RIGHT:
            return ABS_HAT0X;
        case BTN_DPAD_UP:
        case KEY_UP:
            return ABS_HAT0Y;
        case BTN_DPAD_DOWN:
        case KEY_DOWN:
            return ABS_HAT0Y;
        default:
            return ABS_CNT;
    }
}

static unsigned short map_key(const struct portable_profile *profile, unsigned short code) {
    if (profile->layout == LAYOUT_ZERO28 && code == KEY_BACK) return BTN_MODE;
    if (profile->layout == LAYOUT_PIXEL2) {
        if (code == BTN_SOUTH) return BTN_EAST;
        if (code == BTN_EAST) return BTN_SOUTH;
        if (code == BTN_TRIGGER_HAPPY1) return BTN_MODE;
    }
    switch (code) {
        case KEY_VOLUMEDOWN:
        case KEY_VOLUMEUP:
        case BTN_SOUTH:
        case BTN_EAST:
        case BTN_NORTH:
        case BTN_WEST:
        case BTN_TL:
        case BTN_TR:
        case BTN_TL2:
        case BTN_TR2:
        case BTN_SELECT:
        case BTN_START:
        case BTN_MODE:
        case BTN_THUMBL:
        case BTN_THUMBR:
            return code;
        default:
            return KEY_RESERVED;
    }
}

static int source_axis_index(const struct portable_profile *profile, unsigned short code, int *invert) {
    *invert = 0;
    if (profile->layout == LAYOUT_G350) {
        switch (code) {
            case ABS_X:
                *invert = 1;
                return 0;
            case ABS_RX:
                *invert = 1;
                return 1;
            case ABS_RY:
                *invert = 1;
                return 2;
            case ABS_Y:
                return 3;
            default:
                return -1;
        }
    }
    switch (code) {
        case ABS_X:
            return 0;
        case ABS_Y:
            return 1;
        case ABS_RX:
            return 2;
        case ABS_RY:
            return 3;
        default:
            return -1;
    }
}

static unsigned short output_axis_code(int index) {
    static const unsigned short codes[] = {ABS_X, ABS_Y, ABS_RX, ABS_RY};
    return index >= 0 && index < 4 ? codes[index] : ABS_CNT;
}

static int scale_axis_raw(const struct evdev_source *source, unsigned short code, int value) {
    if (code >= ABS_CNT || !source->has_abs[code]) return 2048;
    const struct input_absinfo *info = &source->abs_info[code];
    if (info->maximum <= info->minimum) return 2048;
    int64_t position = (int64_t) value - info->minimum;
    int64_t range = (int64_t) info->maximum - info->minimum;
    int64_t scaled = position * 4095 / range;
    if (scaled < 0) return 0;
    if (scaled > 4095) return 4095;
    return (int) scaled;
}

static void emit_axis(struct portable_state *state, unsigned short source_code, int raw_value) {
    int invert = 0;
    int index = source_axis_index(state->profile, source_code, &invert);
    if (index < 0 || (!state->profile->has_sticks && index > 1)) return;
    int raw = scale_axis_raw(&state->source, source_code, raw_value);
    cal_update(state->axes[index], raw);
    int value = cal_apply(state->axes[index], raw);
    if (invert) value = -value;
    if (value != state->axis_values[index]) {
        state->axis_values[index] = value;
        gamepad_emit_abs(state->gamepad, output_axis_code(index), value);
    }
}

static void emit_dpad(struct portable_state *state, unsigned short code, int pressed) {
    unsigned short hat_code = map_dpad_key(code);
    if (hat_code >= ABS_CNT) return;
    int value = pressed ? 1 : 0;
    if (code == BTN_DPAD_LEFT || code == KEY_LEFT)
        state->dpad_left = value;
    else if (code == BTN_DPAD_RIGHT || code == KEY_RIGHT)
        state->dpad_right = value;
    else if (code == BTN_DPAD_UP || code == KEY_UP)
        state->dpad_up = value;
    else
        state->dpad_down = value;
    state->hat_x = state->dpad_left == state->dpad_right ? 0 : state->dpad_left ? -1 : 1;
    state->hat_y = state->dpad_up == state->dpad_down ? 0 : state->dpad_up ? -1 : 1;
    value = hat_code == ABS_HAT0X ? state->hat_x : state->hat_y;
    if (state->dpad_to_stick) {
        gamepad_emit_abs(state->gamepad, hat_code == ABS_HAT0X ? ABS_X : ABS_Y, value * 32767);
    } else {
        gamepad_emit_abs(state->gamepad, hat_code, value);
    }
}

static struct rumble_sysfs_config rumble_config(enum portable_rumble_id rumble) {
    switch (rumble) {
        case RUMBLE_RK_PWM:
            return (struct rumble_sysfs_config) {"/sys/class/pwm/pwmchip0/pwm0/duty_cycle", "1", "1000000"};
        case RUMBLE_VITA_PWM:
            return (struct rumble_sysfs_config) {"/sys/class/pwm/pwmchip1/pwm0/enable", "0", "1"};
        default:
            return (struct rumble_sysfs_config) {"/sys/class/power_supply/axp2202-battery/moto", "1", "0"};
    }
}

static int portable_initialise(
    void **context, struct gamepad *gamepad, struct axis_state *lx, struct axis_state *ly, struct axis_state *rx,
    struct axis_state *ry, const struct device_options *options
) {
    const struct portable_profile *profile = find_profile(options->backend_id);
    if (!profile) return -1;
    struct portable_state *state = calloc(1, sizeof(*state));
    if (!state) return -1;
    state->profile = profile;
    state->gamepad = gamepad;
    state->source.fd = -1;
    state->axes[0] = lx;
    state->axes[1] = ly;
    state->axes[2] = rx;
    state->axes[3] = ry;
    if (open_source(state, options->verbose) < 0 || isolate_source(state, options->verbose) < 0) {
        evdev_source_close(&state->source);
        free(state);
        return -1;
    }
    struct rumble_sysfs_config config = rumble_config(profile->rumble);
    if (device_rumble_initialise(&state->rumble, rumble_sysfs_driver(), &config, options->rumble_strength) < 0
        && options->verbose) {
        fprintf(stderr, "Rumble is unavailable for %s\n", profile->id);
    }
    if (profile->dpad_switch) state->dpad_to_stick = access(PIXEL_SWAP_PATH, F_OK) == 0;
    *context = state;
    return 0;
}

static int portable_poll(void *context) {
    struct portable_state *state = context;
    if (!device_rumble_poll(&state->rumble, state->gamepad)) return 0;
    struct input_event events[64];
    int dirty = 0;
    unsigned int processed = 0;
    while (processed < 256) {
        size_t capacity = sizeof(events) / sizeof(events[0]);
        if (capacity > 256 - processed) capacity = 256 - processed;
        int result = evdev_source_read(&state->source, events, capacity);
        if (result < 0) return 0;
        if (result == 0) break;
        processed += (unsigned int) result;
        for (int i = 0; i < result; ++i) {
            const struct input_event *event = &events[i];
            if (event->type == EV_KEY) {
                if (map_dpad_key(event->code) < ABS_CNT) {
                    emit_dpad(state, event->code, event->value != 0);
                    dirty = 1;
                } else {
                    unsigned short code = map_key(state->profile, event->code);
                    if (code != KEY_RESERVED) {
                        gamepad_emit_key(state->gamepad, code, event->value);
                        dirty = 1;
                    }
                }
            } else if (event->type == EV_ABS) {
                if (event->code == ABS_HAT0X || event->code == ABS_HAT0Y) {
                    if (state->dpad_to_stick)
                        gamepad_emit_abs(
                            state->gamepad, event->code == ABS_HAT0X ? ABS_X : ABS_Y, event->value * 32767
                        );
                    else
                        gamepad_emit_abs(state->gamepad, event->code, event->value);
                    dirty = 1;
                } else {
                    emit_axis(state, event->code, event->value);
                    dirty = 1;
                }
            } else if (event->type == EV_SYN && event->code == SYN_REPORT && dirty) {
                gamepad_sync(state->gamepad);
                dirty = 0;
            }
        }
        if ((size_t) result < capacity) break;
    }
    if (dirty) gamepad_sync(state->gamepad);
    return 1;
}

static void portable_refresh(void *context) {
    struct portable_state *state = context;
    if (!state || !state->profile->dpad_switch) return;
    int enabled = access(PIXEL_SWAP_PATH, F_OK) == 0;
    if (enabled == state->dpad_to_stick) return;
    state->dpad_to_stick = enabled;
    gamepad_emit_abs(state->gamepad, ABS_HAT0X, 0);
    gamepad_emit_abs(state->gamepad, ABS_HAT0Y, 0);
    gamepad_emit_abs(state->gamepad, ABS_X, enabled ? state->hat_x * 32767 : 0);
    gamepad_emit_abs(state->gamepad, ABS_Y, enabled ? state->hat_y * 32767 : 0);
    gamepad_sync(state->gamepad);
}

static void portable_set_rumble_strength(void *context, unsigned int strength_percent) {
    struct portable_state *state = context;
    if (state) device_rumble_set_strength(&state->rumble, strength_percent);
}

static void portable_close(void *context) {
    struct portable_state *state = context;
    if (!state) return;
    device_rumble_close(&state->rumble);
    evdev_source_close(&state->source);
    free(state);
}

#define PORTABLE_BACKEND(_symbol, _id, _name, _gamepad, _calibration)                                                  \
    const struct device_backend _symbol = {                                                                            \
        .id = (_id),                                                                                                   \
        .name = (_name),                                                                                               \
        .probe_priority = 8,                                                                                           \
        .probe = portable_probe,                                                                                       \
        .gamepad = &(_gamepad),                                                                                        \
        .has_analogue_calibration = (_calibration),                                                                    \
        .ops =                                                                                                         \
            {.name = "portable-evdev",                                                                                 \
             .initialise = portable_initialise,                                                                        \
             .poll = portable_poll,                                                                                    \
             .refresh = portable_refresh,                                                                              \
             .set_rumble_strength = portable_set_rumble_strength,                                                      \
             .close = portable_close},                                                                                 \
        .poll_interval_us = 1000,                                                                                      \
    }

PORTABLE_BACKEND(GCS_H36S_PROFILE, "gcs-h36s", "GCS H36S", GCS_H36S_GAMEPAD, 1);
PORTABLE_BACKEND(MGX_ZERO28_PROFILE, "mgx-zero28", "MAGICX ZERO 28", MGX_ZERO28_GAMEPAD, 1);
PORTABLE_BACKEND(RK_G350_V_PROFILE, "rk-g350-v", "G350 V", RK_G350_V_GAMEPAD, 1);
PORTABLE_BACKEND(RK_PIXEL_2_PROFILE, "rk-pixel-2", "GKD PIXEL 2", RK_PIXEL_2_GAMEPAD, 0);
PORTABLE_BACKEND(RG_VITA_PRO_PROFILE, "rg-vita-pro", "ANBERNIC RG VITA PRO", RG_VITA_PRO_GAMEPAD, 1);

#undef PORTABLE_BACKEND
