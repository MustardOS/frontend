#pragma once

#include <stddef.h>
#include <common/runtime/init.h>

int is_network_connected(void);

int get_network_signal_percent(void);

enum network_reachability {
    network_reachability_unknown = 0,
    network_reachability_checking,
    network_reachability_online,
    network_reachability_sign_in,
    network_reachability_timeout,
    network_reachability_unavailable,
};

int get_network_reachability(void);

int get_network_ipv4_address(char *output, size_t output_size);

int get_any_ipv4_address(char *output, size_t output_size);

int is_bluetooth_connected(void);

int resolution_check(const char *theme_path);

struct screen_dimension get_device_dimensions(void);

int brightness_to_percent(int val);

int volume_to_percent(int val);

char *get_version(int verify);

char *get_build(void);

char *get_storage_label(const char *path, const char *primary, const char *secondary, const char *external);

const char *resolve_info_path(const char *rel);
