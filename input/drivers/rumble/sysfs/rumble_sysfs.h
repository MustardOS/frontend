#pragma once

#include "../../../devices/device_rumble.h"

struct rumble_sysfs_config {
    const char *path;
    const char *on_value;
    const char *off_value;
};

const struct device_rumble_driver *rumble_sysfs_driver(void);
