#pragma once

#include <signal.h>
#include <stddef.h>

struct gamepad;
struct gamepad_desc;
struct axis_state;

struct device_options {
    const char *backend_id;
    int verbose;
    unsigned int rumble_strength;
};

struct device_ops {
    const char *name;

    int (*initialise)(
        void **ctx, struct gamepad *gp, struct axis_state *lx, struct axis_state *ly, struct axis_state *rx,
        struct axis_state *ry, const struct device_options *options
    );

    int (*poll)(void *ctx);

    void (*refresh)(void *ctx);

    void (*set_rumble_strength)(void *ctx, unsigned int strength_percent);

    void (*close)(void *ctx);
};

struct device_backend {
    const char *id;
    const char *name;
    int probe_priority;
    int (*probe)(void);
    const struct gamepad_desc *gamepad;
    int has_analogue_calibration;
    struct device_ops ops;
    unsigned int poll_interval_us;
};
