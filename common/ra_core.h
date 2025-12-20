#pragma once

struct ra_core_name {
    const char *core;
    const char *name;
};

struct ext_core_name {
    const char *core;
    const char *name;
};

extern const struct ra_core_name ra_core_names[];

extern const struct ext_core_name ext_core_names[];
