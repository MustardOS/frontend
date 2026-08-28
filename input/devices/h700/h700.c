#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "h700.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include "profiles.h"
#include "mapping.h"
#include "../device_rumble.h"
#include "../../drivers/evdev/evdev.h"
#include "../../drivers/rumble/evdev/rumble_evdev.h"
#include "../../common/uinput.h"

#define H700_SOURCE_NAME        "muOS-Input-Source"
#define H700_PRIVATE_DEVICE_DIR "/dev/muinput"
#define H700_PRIVATE_SOURCE     H700_PRIVATE_DEVICE_DIR "/h700-source"
#define H700_PRIVATE_JOYSTICK   H700_PRIVATE_DEVICE_DIR "/h700-joystick"

struct h700_state {
    struct gamepad *gamepad;
    const struct gamepad_desc *desc;
    int has_sticks;
    struct evdev_source source;
    struct device_rumble_state rumble;
};

struct h700_layout {
    const char *id;
    const struct gamepad_desc *desc;
    int has_sticks;
};

static const struct h700_layout H700_LAYOUTS[] = {
    {"rgsp", &RGSP_GAMEPAD, 0},
    {"rg28xx-h", &RG28XX_H_GAMEPAD, 0},
    {"rg34xx-h", &RG34XX_H_GAMEPAD, 0},
    {"rg35xx-2024", &RG35XX_2024_GAMEPAD, 0},
    {"rg35xx-plus", &RG35XX_PLUS_GAMEPAD, 0},
    {"rg35xx-sp", &RG35XX_SP_GAMEPAD, 0},
    {"rg34xx-sp", &RG34XX_SP_GAMEPAD, 1},
    {"rg35xx-h", &RG35XX_H_GAMEPAD, 1},
    {"rg35xx-pro", &RG35XX_PRO_GAMEPAD, 1},
    {"rg40xx-h", &RG40XX_H_GAMEPAD, 1},
    {"rg40xx-v", &RG40XX_V_GAMEPAD, 1},
    {"rgcubexx-h", &RGCUBEXX_H_GAMEPAD, 1},
};

static const struct h700_layout *find_layout(const char *id) {
    for (size_t i = 0; i < sizeof(H700_LAYOUTS) / sizeof(H700_LAYOUTS[0]); ++i) {
        if (id && strcmp(id, H700_LAYOUTS[i].id) == 0) return &H700_LAYOUTS[i];
    }
    return NULL;
}

static int normalise_stick_axis(const struct evdev_source *source, unsigned short code, int value) {
    if (code >= ABS_CNT || !source->has_abs[code]) {
        return value < 0 ? -32767 : value > 0 ? 32767 : 0;
    }
    const struct input_absinfo *info = &source->abs_info[code];
    if (info->maximum <= info->minimum) return 0;
    int64_t position = (int64_t) value - info->minimum;
    int64_t range = (int64_t) info->maximum - info->minimum;
    int64_t normalised = position * 65534 / range - 32767;
    if (normalised < -32767) return -32767;
    if (normalised > 32767) return 32767;
    return (int) normalised;
}

static const char *h700_source_path(void) {
    const char *configured = getenv("MUOS_INPUT_SOURCE");
    if (configured && *configured) return configured;
    return access(H700_PRIVATE_SOURCE, F_OK) == 0 ? H700_PRIVATE_SOURCE : NULL;
}

static int h700_open_source(struct evdev_source *source, int grab, int verbose) {
    const char *path = h700_source_path();
    const char *configured_name = getenv("MUOS_INPUT_SOURCE_NAME");
    if (path && *path) {
        return evdev_source_open(
            source, path, configured_name && *configured_name ? configured_name : NULL, grab, verbose
        );
    }
    if (configured_name && *configured_name) {
        return evdev_source_open(source, NULL, configured_name, grab, verbose);
    }
    if (evdev_source_open(source, NULL, H700_SOURCE_NAME, grab, verbose) == 0) {
        return 0;
    }

    return evdev_source_open(source, NULL, MUOS_GAMEPAD_NAME, grab, verbose);
}

static int h700_input_class_name(int fd, const char *prefix, char *name, size_t name_size) {
    struct stat info;
    if (fstat(fd, &info) < 0) return -1;

    DIR *directory = opendir("/sys/class/input");
    if (!directory) return -1;
    int found = -1;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strncmp(entry->d_name, prefix, strlen(prefix)) != 0) continue;
        char dev_path[PATH_MAX];
        int length = snprintf(dev_path, sizeof(dev_path), "/sys/class/input/%s/dev", entry->d_name);
        if (length <= 0 || (size_t) length >= sizeof(dev_path)) continue;
        FILE *dev = fopen(dev_path, "r");
        if (!dev) continue;
        unsigned int device_major = 0;
        unsigned int device_minor = 0;
        int parsed = fscanf(dev, "%u:%u", &device_major, &device_minor);
        fclose(dev);
        if (parsed == 2 && device_major == major(info.st_rdev) && device_minor == minor(info.st_rdev)) {
            size_t entry_length = strnlen(entry->d_name, name_size);
            if (entry_length >= name_size) continue;
            memcpy(name, entry->d_name, entry_length + 1);
            found = 0;
            break;
        }
    }
    closedir(directory);
    return found;
}

static int h700_joystick_class_name(const char *event_name, char *name, size_t name_size) {
    char event_device[PATH_MAX];
    char event_real[PATH_MAX];
    int length = snprintf(event_device, sizeof(event_device), "/sys/class/input/%s/device", event_name);
    if (length <= 0 || (size_t) length >= sizeof(event_device) || !realpath(event_device, event_real)) return -1;

    DIR *directory = opendir("/sys/class/input");
    if (!directory) return -1;
    int found = -1;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strncmp(entry->d_name, "js", 2) != 0) continue;
        char joystick_device[PATH_MAX];
        char joystick_real[PATH_MAX];
        length = snprintf(joystick_device, sizeof(joystick_device), "/sys/class/input/%s/device", entry->d_name);
        if (length <= 0 || (size_t) length >= sizeof(joystick_device) || !realpath(joystick_device, joystick_real))
            continue;
        if (strcmp(event_real, joystick_real) == 0) {
            size_t entry_length = strnlen(entry->d_name, name_size);
            if (entry_length >= name_size) continue;
            memcpy(name, entry->d_name, entry_length + 1);
            found = 0;
            break;
        }
    }
    closedir(directory);
    return found;
}

static int h700_remove_public_links(const char *event_name, const char *joystick_name) {
    DIR *directory = opendir("/dev/input/by-path");
    if (!directory) return errno == ENOENT ? 0 : -1;
    int result = 0;
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
        if (strcmp(target_name, event_name) != 0 && (!joystick_name || strcmp(target_name, joystick_name) != 0))
            continue;
        if (unlink(path) < 0 && errno != ENOENT) result = -1;
    }
    closedir(directory);
    return result;
}

static int h700_isolate_source(struct evdev_source *source, int verbose) {
    int already_private = strcmp(source->path, H700_PRIVATE_SOURCE) == 0;
    if (!already_private && strncmp(source->path, "/dev/input/event", sizeof("/dev/input/event") - 1) != 0) {
        fprintf(stderr, "Refusing to relocate unexpected H700 source %s\n", source->path);
        return -1;
    }
    char event_name[32];
    if (h700_input_class_name(source->fd, "event", event_name, sizeof(event_name)) < 0) {
        fprintf(stderr, "Unable to identify H700 event source in sysfs\n");
        return -1;
    }
    char joystick_name[32];
    int has_joystick = h700_joystick_class_name(event_name, joystick_name, sizeof(joystick_name)) == 0;
    if (mkdir(H700_PRIVATE_DEVICE_DIR, 0755) < 0 && errno != EEXIST) {
        perror("mkdir " H700_PRIVATE_DEVICE_DIR);
        return -1;
    }
    if (!already_private && rename(source->path, H700_PRIVATE_SOURCE) < 0) {
        perror("relocate H700 evdev source");
        return -1;
    }
    snprintf(source->path, sizeof(source->path), "%s", H700_PRIVATE_SOURCE);
    if (has_joystick) {
        char joystick_path[PATH_MAX];
        int length = snprintf(joystick_path, sizeof(joystick_path), "/dev/input/%s", joystick_name);
        if (length <= 0 || (size_t) length >= sizeof(joystick_path)
            || (access(joystick_path, F_OK) == 0 && rename(joystick_path, H700_PRIVATE_JOYSTICK) < 0)) {
            perror("relocate H700 joystick source");
            return -1;
        }
    }
    if (h700_remove_public_links(event_name, has_joystick ? joystick_name : NULL) < 0) {
        perror("remove H700 public input links");
        return -1;
    }
    if (verbose) {
        fprintf(stderr, "Isolated H700 source under %s\n", H700_PRIVATE_DEVICE_DIR);
    }
    return 0;
}

static int h700_probe(void) {
    struct evdev_source source;
    if (h700_open_source(&source, 0, 0) < 0) return 0;
    evdev_source_close(&source);
    return 1;
}

static int h700_initialise(
    void **context, struct gamepad *gamepad, struct axis_state *lx, struct axis_state *ly, struct axis_state *rx,
    struct axis_state *ry, const struct device_options *options
) {
    (void) lx;
    (void) ly;
    (void) rx;
    (void) ry;
    struct h700_state *state = calloc(1, sizeof(*state));
    if (!state) return -1;
    const struct h700_layout *layout = find_layout(options->backend_id);
    if (!layout) {
        free(state);
        return -1;
    }
    state->desc = layout->desc;
    state->has_sticks = layout->has_sticks;
    state->source.fd = -1;
    if (h700_open_source(&state->source, 1, options->verbose) < 0) {
        free(state);
        return -1;
    }
    if (h700_isolate_source(&state->source, options->verbose) < 0) {
        evdev_source_close(&state->source);
        free(state);
        return -1;
    }
    const struct rumble_evdev_config rumble_config = {.source_fd = state->source.fd};
    if (device_rumble_initialise(&state->rumble, rumble_evdev_driver(), &rumble_config, options->rumble_strength) < 0) {
        evdev_source_close(&state->source);
        free(state);
        return -1;
    }
    state->gamepad = gamepad;
    *context = state;
    return 0;
}

static int h700_poll(void *context) {
    struct h700_state *state = context;
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
                unsigned short mapped_code = h700_map_key(event->code, state->has_sticks);
                if (mapped_code != KEY_RESERVED) {
                    gamepad_emit_key(state->gamepad, mapped_code, event->value);
                    dirty = 1;
                }
            } else if (event->type == EV_ABS) {
                unsigned short mapped_code = h700_map_abs(event->code, state->has_sticks);
                if (mapped_code < ABS_CNT) {
                    int value = mapped_code != ABS_HAT0X && mapped_code != ABS_HAT0Y
                                    ? normalise_stick_axis(&state->source, event->code, event->value)
                                : event->value < 0 ? -1
                                : event->value > 0 ? 1
                                                   : 0;
                    gamepad_emit_abs(state->gamepad, mapped_code, value);
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

static void h700_close(void *context) {
    struct h700_state *state = context;
    if (!state) return;
    device_rumble_close(&state->rumble);
    evdev_source_close(&state->source);
    free(state);
}

static void h700_set_rumble_strength(void *context, unsigned int strength_percent) {
    struct h700_state *state = context;
    if (state) device_rumble_set_strength(&state->rumble, strength_percent);
}

#define H700_BACKEND(_symbol, _id, _name, _desc)                                                                       \
    const struct device_backend _symbol = {                                                                            \
        .id = (_id),                                                                                                   \
        .name = (_name),                                                                                               \
        .probe_priority = 10,                                                                                          \
        .probe = h700_probe,                                                                                           \
        .gamepad = &(_desc),                                                                                           \
        .has_analogue_calibration = 0,                                                                                 \
        .ops =                                                                                                         \
            {.name = "h700-evdev",                                                                                     \
             .initialise = h700_initialise,                                                                            \
             .poll = h700_poll,                                                                                        \
             .refresh = NULL,                                                                                          \
             .set_rumble_strength = h700_set_rumble_strength,                                                          \
             .close = h700_close},                                                                                     \
        .poll_interval_us = 1000,                                                                                      \
    }

H700_BACKEND(RGSP_PROFILE, "rgsp", "ANBERNIC RG SP", RGSP_GAMEPAD);
H700_BACKEND(RG28XX_H_PROFILE, "rg28xx-h", "ANBERNIC RG28XX H", RG28XX_H_GAMEPAD);
H700_BACKEND(RG34XX_H_PROFILE, "rg34xx-h", "ANBERNIC RG34XX H", RG34XX_H_GAMEPAD);
H700_BACKEND(RG34XX_SP_PROFILE, "rg34xx-sp", "ANBERNIC RG34XX SP", RG34XX_SP_GAMEPAD);
H700_BACKEND(RG35XX_2024_PROFILE, "rg35xx-2024", "ANBERNIC RG35XX 2024", RG35XX_2024_GAMEPAD);
H700_BACKEND(RG35XX_H_PROFILE, "rg35xx-h", "ANBERNIC RG35XX H", RG35XX_H_GAMEPAD);
H700_BACKEND(RG35XX_PLUS_PROFILE, "rg35xx-plus", "ANBERNIC RG35XX PLUS", RG35XX_PLUS_GAMEPAD);
H700_BACKEND(RG35XX_PRO_PROFILE, "rg35xx-pro", "ANBERNIC RG35XX PRO", RG35XX_PRO_GAMEPAD);
H700_BACKEND(RG35XX_SP_PROFILE, "rg35xx-sp", "ANBERNIC RG35XX SP", RG35XX_SP_GAMEPAD);
H700_BACKEND(RG40XX_H_PROFILE, "rg40xx-h", "ANBERNIC RG40XX H", RG40XX_H_GAMEPAD);
H700_BACKEND(RG40XX_V_PROFILE, "rg40xx-v", "ANBERNIC RG40XX V", RG40XX_V_GAMEPAD);
H700_BACKEND(RGCUBEXX_H_PROFILE, "rgcubexx-h", "ANBERNIC RGCUBEXX H", RGCUBEXX_H_GAMEPAD);

#undef H700_BACKEND
