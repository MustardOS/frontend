#pragma once

#include "init.h"

int is_network_connected(void);

int is_bluetooth_connected(void);

int resolution_check(const char *theme_path);

struct screen_dimension get_device_dimensions(void);

int brightness_to_percent(int val);

int volume_to_percent(int val);

char *get_version(int verify);

char *get_build(void);

char *get_storage_label(const char *path);

const char *resolve_info_path(const char *rel);
