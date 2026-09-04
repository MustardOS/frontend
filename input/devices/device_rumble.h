#pragma once

#include <time.h>
#include <stddef.h>
#include <linux/input.h>
#include <linux/uinput.h>

#include "../common/uinput.h"

enum { device_rumble_effect_slots = 4 };

struct device_rumble_driver {
    const char *name;
    size_t ctx_size;

    int (*initialise)(void *ctx, const void *config);

    int (*set)(void *ctx, int on);

    void (*close)(void *ctx);
};

struct device_rumble_slot {
    int used;
    struct ff_effect effect;
};

struct device_rumble_state {
    struct device_rumble_slot slots[device_rumble_effect_slots];
    int active_id;
    int has_stop_time;
    struct timespec stop_time;
    const struct device_rumble_driver *driver;
    void *driver_ctx;
    unsigned int strength_percent;
    unsigned int target_magnitude;
    unsigned int pulse_accumulator;
    int motor_on;
    int initialised;
};

int device_rumble_initialise(
    struct device_rumble_state *st, const struct device_rumble_driver *driver, const void *driver_config,
    unsigned int strength_percent
);

void device_rumble_close(struct device_rumble_state *st);

int device_rumble_poll(struct device_rumble_state *st, struct gamepad *gp);

void device_rumble_set_strength(struct device_rumble_state *st, unsigned int strength_percent);

const struct device_rumble_driver *rumble_a133_driver(void);
