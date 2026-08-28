#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "rumble_a133.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "../../../devices/device_rumble.h"

static int rumble_a133_driver_initialise(void *context, const void *config) {
    (void) config;
    return rumble_a133_initialise(context);
}
#include "../../gpio/gpio.h"

static int write_motor(int fd, int on) {
    char value = on ? '1' : '0';
    if (lseek(fd, 0, SEEK_SET) < 0) return -1;
    ssize_t written;
    do {
        written = write(fd, &value, 1);
    } while (written < 0 && errno == EINTR);
    return written == 1 ? 0 : -1;
}

int rumble_a133_initialise(struct rumble_a133_hw *hw) {
    if (!hw) {
        return -1;
    }
    *hw = (struct rumble_a133_hw) {.value_fd = -1};
    if (gpio_export(RUMBLE_A133_GPIO_PIN) < 0 || gpio_set_direction(RUMBLE_A133_GPIO_PIN, 1) < 0) {
        return -1;
    }
    char path[64];
    int length = snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", RUMBLE_A133_GPIO_PIN);
    if (length <= 0 || (size_t) length >= sizeof(path)) return -1;
    hw->value_fd = open(path, O_WRONLY | O_CLOEXEC);
    if (hw->value_fd < 0 || write_motor(hw->value_fd, 0) < 0) {
        if (hw->value_fd >= 0) close(hw->value_fd);
        hw->value_fd = -1;
        return -1;
    }
    hw->initialised = 1;
    return 0;
}

int rumble_a133_set(struct rumble_a133_hw *hw, int on) {
    if (!hw || !hw->initialised) {
        return -1;
    }
    if (write_motor(hw->value_fd, on) < 0) {
        perror("rumble_a133 write");
        return -1;
    }
    return 0;
}

void rumble_a133_close(struct rumble_a133_hw *hw) {
    if (!hw) {
        return;
    }
    if (hw->initialised) {
        write_motor(hw->value_fd, 0);
    }
    if (hw->value_fd >= 0) close(hw->value_fd);
    *hw = (struct rumble_a133_hw) {.value_fd = -1};
}

const struct device_rumble_driver *rumble_a133_driver(void) {
    static const struct device_rumble_driver drv = {
        .name = "a133-gpio",
        .ctx_size = sizeof(struct rumble_a133_hw),
        .initialise = rumble_a133_driver_initialise,
        .set = (int (*)(void *, int)) rumble_a133_set,
        .close = (void (*)(void *)) rumble_a133_close,
    };
    return &drv;
}
