#define _POSIX_C_SOURCE 200809L

#include "device_rumble.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <time.h>
#include <stdlib.h>

static inline void set_motor(struct device_rumble_state *st, int on) {
    if (!st->initialised || !st->driver || !st->driver_ctx) {
        return;
    }
    if (st->motor_on == on) {
        return;
    }
    if (st->driver->set(st->driver_ctx, on) < 0) {
        perror("rumble set");
        return;
    }
    st->motor_on = on;
}

static inline unsigned int effect_magnitude(const struct ff_effect *eff) {
    if (!eff || eff->type != FF_RUMBLE) {
        return 0;
    }
    unsigned int strong = eff->u.rumble.strong_magnitude;
    unsigned int weak = eff->u.rumble.weak_magnitude;
    return (strong > weak) ? strong : weak;
}

static void stop_rumble(struct device_rumble_state *st) {
    st->active_id = -1;
    st->has_stop_time = 0;
    st->target_magnitude = 0;
    st->pulse_accumulator = 0;
    set_motor(st, 0);
}

static void update_motor_level(struct device_rumble_state *st) {
    unsigned int magnitude = st->target_magnitude;
    if (magnitude == 0) {
        set_motor(st, 0);
        return;
    }
    if (magnitude >= UINT16_MAX) {
        set_motor(st, 1);
        return;
    }

    st->pulse_accumulator += magnitude;
    if (st->pulse_accumulator >= UINT16_MAX) {
        st->pulse_accumulator -= UINT16_MAX;
        set_motor(st, 1);
    } else {
        set_motor(st, 0);
    }
}

static inline void timespec_add_ms(struct timespec *ts, unsigned int ms) {
    ts->tv_sec += ms / 1000u;
    ts->tv_nsec += (long) (ms % 1000u) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000L;
    }
}

static inline int timespec_ge(const struct timespec *a, const struct timespec *b) {
    if (a->tv_sec != b->tv_sec) {
        return a->tv_sec > b->tv_sec;
    }
    return a->tv_nsec >= b->tv_nsec;
}

static void maybe_stop_on_timeout(struct device_rumble_state *st) {
    if (!st->has_stop_time || st->active_id < 0) {
        return;
    }
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
        return;
    }
    if (timespec_ge(&now, &st->stop_time)) {
        stop_rumble(st);
    }
}

static void handle_play_event(struct device_rumble_state *st, int effect_id, int value) {
    if (value == 0) {
        stop_rumble(st);
        return;
    }
    if (effect_id < 0 || effect_id >= device_rumble_effect_slots) {
        return;
    }
    if (!st->slots[effect_id].used || st->slots[effect_id].effect.type != FF_RUMBLE) {
        return;
    }

    const struct ff_effect *eff = &st->slots[effect_id].effect;
    unsigned int mag = effect_magnitude(eff);
    mag = (mag * st->strength_percent + 50u) / 100u;
    st->active_id = effect_id;
    st->has_stop_time = 0;
    st->stop_time = (struct timespec) {0, 0};
    if (eff->replay.length > 0) {
        if (clock_gettime(CLOCK_MONOTONIC, &st->stop_time) == 0) {
            timespec_add_ms(&st->stop_time, eff->replay.length);
            st->has_stop_time = 1;
        } else {
            st->has_stop_time = 0;
        }
    }
    st->target_magnitude = mag;
    st->pulse_accumulator = 0;
    update_motor_level(st);
}

static void handle_upload(struct device_rumble_state *st, int fd, uint32_t request_id) {
    struct uinput_ff_upload upload;
    memset(&upload, 0, sizeof(upload));
    upload.request_id = request_id;
    upload.retval = 0;

    if (ioctl(fd, UI_BEGIN_FF_UPLOAD, &upload) < 0) {
        perror("UI_BEGIN_FF_UPLOAD");
        return;
    }

    int id = upload.effect.id;
    if (upload.effect.type != FF_RUMBLE || id < 0 || id >= device_rumble_effect_slots) {
        upload.retval = -EINVAL;
    } else {
        st->slots[id].effect = upload.effect;
        st->slots[id].used = 1;
        if (st->active_id == id && effect_magnitude(&upload.effect) == 0) {
            stop_rumble(st);
        }
    }

    if (ioctl(fd, UI_END_FF_UPLOAD, &upload) < 0) {
        perror("UI_END_FF_UPLOAD");
    }
}

static void handle_erase(struct device_rumble_state *st, int fd, uint32_t request_id) {
    struct uinput_ff_erase erase;
    memset(&erase, 0, sizeof(erase));
    erase.request_id = request_id;
    erase.retval = 0;

    if (ioctl(fd, UI_BEGIN_FF_ERASE, &erase) < 0) {
        perror("UI_BEGIN_FF_ERASE");
        return;
    }

    int id = (int) erase.effect_id;
    if (id < 0 || id >= device_rumble_effect_slots) {
        erase.retval = -EINVAL;
    } else {
        st->slots[id].used = 0;
        if (st->active_id == id) {
            stop_rumble(st);
        }
    }

    if (ioctl(fd, UI_END_FF_ERASE, &erase) < 0) {
        perror("UI_END_FF_ERASE");
    }
}

int device_rumble_initialise(
    struct device_rumble_state *st, const struct device_rumble_driver *driver, const void *driver_config,
    unsigned int strength_percent
) {
    if (!st || !driver) {
        return -1;
    }
    memset(st, 0, sizeof(*st));
    st->active_id = -1;
    st->driver = driver;
    st->strength_percent = strength_percent > 100u ? 100u : strength_percent;

    if (!driver->initialise || !driver->set || !driver->close || driver->ctx_size == 0) {
        return -1;
    }

    st->driver_ctx = calloc(1, driver->ctx_size);
    if (!st->driver_ctx) {
        return -1;
    }

    if (driver->initialise(st->driver_ctx, driver_config) < 0) {
        free(st->driver_ctx);
        st->driver_ctx = NULL;
        st->driver = NULL;
        return -1;
    }

    st->initialised = 1;
    return 0;
}

void device_rumble_close(struct device_rumble_state *st) {
    if (!st) {
        return;
    }
    if (st->driver && st->driver_ctx && st->driver->close) {
        st->driver->close(st->driver_ctx);
    }
    free(st->driver_ctx);
    memset(st, 0, sizeof(*st));
}

int device_rumble_poll(struct device_rumble_state *st, struct gamepad *gp) {
    if (!st || !gp) {
        return 0;
    }
    int fd = gamepad_get_fd(gp);
    if (fd < 0) {
        return 0;
    }

    struct input_event ev;
    for (;;) {
        int r = gamepad_read_event(gp, &ev);
        if (r < 0) {
            return 0;
        }
        if (r == 0) {
            break;
        }

        if (ev.type == EV_UINPUT) {
            if (ev.code == UI_FF_UPLOAD) {
                handle_upload(st, fd, (uint32_t) ev.value);
            } else if (ev.code == UI_FF_ERASE) {
                handle_erase(st, fd, (uint32_t) ev.value);
            }
        } else if (ev.type == EV_FF) {
            handle_play_event(st, (int) ev.code, (int) ev.value);
        }
    }

    maybe_stop_on_timeout(st);
    if (st->active_id >= 0) {
        update_motor_level(st);
    }
    return 1;
}

void device_rumble_set_strength(struct device_rumble_state *st, unsigned int strength_percent) {
    if (!st) return;
    unsigned int strength = strength_percent > 100u ? 100u : strength_percent;
    if (st->strength_percent == strength) return;
    stop_rumble(st);
    st->strength_percent = strength;
}
