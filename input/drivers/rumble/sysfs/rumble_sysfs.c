#define _POSIX_C_SOURCE 200809L

#include "rumble_sysfs.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

struct rumble_sysfs_hardware {
    int fd;
    char on_value[24];
    char off_value[24];
};

static int write_value(struct rumble_sysfs_hardware *hardware, const char *value) {
    size_t length = strlen(value);
    if (lseek(hardware->fd, 0, SEEK_SET) < 0) return -1;
    ssize_t written;
    do {
        written = write(hardware->fd, value, length);
    } while (written < 0 && errno == EINTR);
    return written == (ssize_t) length ? 0 : -1;
}

static int rumble_sysfs_initialise(void *context, const void *driver_config) {
    struct rumble_sysfs_hardware *hardware = context;
    const struct rumble_sysfs_config *config = driver_config;
    if (!hardware || !config || !config->path || !config->on_value || !config->off_value) return -1;

    hardware->fd = -1;
    if (strlen(config->on_value) >= sizeof(hardware->on_value)
        || strlen(config->off_value) >= sizeof(hardware->off_value)) {
        return -1;
    }
    strcpy(hardware->on_value, config->on_value);
    strcpy(hardware->off_value, config->off_value);
    hardware->fd = open(config->path, O_WRONLY | O_CLOEXEC);
    if (hardware->fd < 0 || write_value(hardware, hardware->off_value) < 0) {
        if (hardware->fd >= 0) close(hardware->fd);
        hardware->fd = -1;
        return -1;
    }
    return 0;
}

static int rumble_sysfs_set(void *context, int on) {
    struct rumble_sysfs_hardware *hardware = context;
    if (!hardware || hardware->fd < 0) return -1;
    return write_value(hardware, on ? hardware->on_value : hardware->off_value);
}

static void rumble_sysfs_close(void *context) {
    struct rumble_sysfs_hardware *hardware = context;
    if (!hardware || hardware->fd < 0) return;
    write_value(hardware, hardware->off_value);
    close(hardware->fd);
    hardware->fd = -1;
}

const struct device_rumble_driver *rumble_sysfs_driver(void) {
    static const struct device_rumble_driver driver = {
        .name = "sysfs",
        .ctx_size = sizeof(struct rumble_sysfs_hardware),
        .initialise = rumble_sysfs_initialise,
        .set = rumble_sysfs_set,
        .close = rumble_sysfs_close,
    };
    return &driver;
}
