#pragma once

#include "../../../devices/device_rumble.h"

struct rumble_evdev_config {
    int source_fd;
};

const struct device_rumble_driver *rumble_evdev_driver(void);
