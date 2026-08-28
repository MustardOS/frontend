#pragma once

#include <stddef.h>
#include <stdio.h>

#include "device.h"

const struct device_backend *device_registry_detect(void);
const struct device_backend *device_registry_find(const char *id);
const struct device_backend *const *device_registry_all(size_t *count);
void device_registry_print(FILE *stream);
