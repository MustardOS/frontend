#include "rumble_evdev.h"

#include <linux/input.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct rumble_evdev_hw {
    int fd;
    int effect_id;
};

static int rumble_evdev_initialise(void *context, const void *driver_config) {
    struct rumble_evdev_hw *hardware = context;
    const struct rumble_evdev_config *config = driver_config;
    if (!hardware || !config || config->source_fd < 0) return -1;

    hardware->fd = dup(config->source_fd);
    hardware->effect_id = -1;
    if (hardware->fd < 0) return -1;

    struct ff_effect effect;
    memset(&effect, 0, sizeof(effect));
    effect.type = FF_RUMBLE;
    effect.id = -1;
    effect.u.rumble.strong_magnitude = UINT16_MAX;
    effect.u.rumble.weak_magnitude = UINT16_MAX / 2u;
    if (ioctl(hardware->fd, EVIOCSFF, &effect) < 0) {
        close(hardware->fd);
        hardware->fd = -1;
        return -1;
    }
    hardware->effect_id = effect.id;
    return 0;
}

static int rumble_evdev_set(void *context, int on) {
    struct rumble_evdev_hw *hardware = context;
    if (!hardware || hardware->fd < 0 || hardware->effect_id < 0) return -1;
    struct input_event event;
    memset(&event, 0, sizeof(event));
    event.type = EV_FF;
    event.code = (unsigned short) hardware->effect_id;
    event.value = on ? 1 : 0;
    ssize_t count = write(hardware->fd, &event, sizeof(event));
    return count == (ssize_t) sizeof(event) ? 0 : -1;
}

static void rumble_evdev_close(void *context) {
    struct rumble_evdev_hw *hardware = context;
    if (!hardware || hardware->fd < 0) return;
    rumble_evdev_set(hardware, 0);
    if (hardware->effect_id >= 0) {
        ioctl(hardware->fd, EVIOCRMFF, hardware->effect_id);
    }
    close(hardware->fd);
    hardware->fd = -1;
    hardware->effect_id = -1;
}

const struct device_rumble_driver *rumble_evdev_driver(void) {
    static const struct device_rumble_driver driver = {
        .name = "evdev",
        .ctx_size = sizeof(struct rumble_evdev_hw),
        .initialise = rumble_evdev_initialise,
        .set = rumble_evdev_set,
        .close = rumble_evdev_close,
    };
    return &driver;
}
