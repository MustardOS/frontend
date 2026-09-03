#include "uinput.h"

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct gamepad {
    int fd;
    size_t pending_count;
    struct input_event pending[64];
    gamepad_event_observer observer;
    void *observer_context;
    char sysname[64];
};

static int flush_events(struct gamepad *gp) {
    if (!gp || gp->pending_count == 0) {
        return 1;
    }

    size_t bytes = gp->pending_count * sizeof(gp->pending[0]);
    ssize_t written;
    do {
        written = write(gp->fd, gp->pending, bytes);
    } while (written < 0 && errno == EINTR);

    gp->pending_count = 0;
    if (written != (ssize_t) bytes) {
        if (written < 0) {
            perror("write uinput batch");
        } else {
            fprintf(stderr, "short write to uinput\n");
        }
        return 0;
    }
    return 1;
}

static int set_ioctl_bit(int fd, unsigned long request, unsigned long value, const char *label) {
    if (ioctl(fd, request, value) == 0) {
        return 1;
    }
    fprintf(stderr, "%s: %s\n", label, strerror(errno));
    return 0;
}

static int set_abs(int fd, const struct gamepad_abs_desc *axis, int supports_setup_ioctl) {
    struct uinput_abs_setup abs = {
        .code = axis->code,
        .absinfo = {
            .minimum = axis->min,
            .maximum = axis->max,
            .fuzz = axis->fuzz,
            .flat = axis->flat,
            .resolution = axis->resolution,
        },
    };
    if (!set_ioctl_bit(fd, UI_SET_ABSBIT, axis->code, "UI_SET_ABSBIT")) {
        return 0;
    }
    if (!supports_setup_ioctl) {
        return 1;
    }
    if (ioctl(fd, UI_ABS_SETUP, &abs) < 0) {
        perror("UI_ABS_SETUP");
        return 0;
    }
    return 1;
}

static int write_legacy_setup(int fd, const struct gamepad_desc *desc) {
    struct uinput_user_dev setup;
    memset(&setup, 0, sizeof(setup));
    setup.id = desc->id;
    setup.ff_effects_max = desc->ff_effects_max;
    snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "%s", desc->name);

    for (size_t i = 0; i < desc->axis_count; ++i) {
        const struct gamepad_abs_desc *axis = &desc->axes[i];
        setup.absmin[axis->code] = axis->min;
        setup.absmax[axis->code] = axis->max;
        setup.absfuzz[axis->code] = axis->fuzz;
        setup.absflat[axis->code] = axis->flat;
    }

    ssize_t written;
    do {
        written = write(fd, &setup, sizeof(setup));
    } while (written < 0 && errno == EINTR);
    if (written != (ssize_t) sizeof(setup)) {
        if (written < 0)
            perror("write uinput legacy setup");
        else
            fprintf(stderr, "short write of uinput legacy setup\n");
        return 0;
    }
    return 1;
}

struct gamepad *gamepad_initialise(const struct gamepad_desc *desc) {
    if (!desc || !desc->name) {
        return NULL;
    }

    int fd = open("/dev/uinput", O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("open /dev/uinput");
        return NULL;
    }

    int uinput_version = 0;
    int supports_setup_ioctl = ioctl(fd, UI_GET_VERSION, &uinput_version) == 0 && uinput_version >= 5;

    if (desc->key_count > 0) {
        if (!set_ioctl_bit(fd, UI_SET_EVBIT, EV_KEY, "UI_SET_EVBIT EV_KEY")) {
            close(fd);
            return NULL;
        }
        for (size_t i = 0; i < desc->key_count; ++i) {
            if (!set_ioctl_bit(fd, UI_SET_KEYBIT, desc->keys[i], "UI_SET_KEYBIT")) {
                close(fd);
                return NULL;
            }
        }
    }

    if (desc->axis_count > 0) {
        if (!set_ioctl_bit(fd, UI_SET_EVBIT, EV_ABS, "UI_SET_EVBIT EV_ABS")) {
            close(fd);
            return NULL;
        }
        for (size_t i = 0; i < desc->axis_count; ++i) {
            if (!set_abs(fd, &desc->axes[i], supports_setup_ioctl)) {
                close(fd);
                return NULL;
            }
        }
    }

    if (desc->switch_count > 0) {
        if (!set_ioctl_bit(fd, UI_SET_EVBIT, EV_SW, "UI_SET_EVBIT EV_SW")) {
            close(fd);
            return NULL;
        }
        for (size_t i = 0; i < desc->switch_count; ++i) {
            if (!set_ioctl_bit(fd, UI_SET_SWBIT, desc->switches[i], "UI_SET_SWBIT")) {
                close(fd);
                return NULL;
            }
        }
    }

    if (desc->ff_effects_max > 0 && desc->enable_ff_rumble) {
        if (!set_ioctl_bit(fd, UI_SET_EVBIT, EV_FF, "UI_SET_EVBIT EV_FF")) {
            close(fd);
            return NULL;
        }

        const unsigned short default_ff[] = {FF_RUMBLE};
        const unsigned short *ff_effects = desc->ff_effects ? desc->ff_effects : default_ff;
        size_t ff_count = (desc->ff_effects && desc->ff_effect_count > 0)
                              ? desc->ff_effect_count
                              : (sizeof(default_ff) / sizeof(default_ff[0]));

        for (size_t i = 0; i < ff_count; ++i) {
            if (!set_ioctl_bit(fd, UI_SET_FFBIT, ff_effects[i], "UI_SET_FFBIT")) {
                close(fd);
                return NULL;
            }
        }
    }

    if (supports_setup_ioctl) {
        struct uinput_setup setup;
        memset(&setup, 0, sizeof(setup));
        setup.id = desc->id;
        setup.ff_effects_max = desc->ff_effects_max;
        snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "%s", desc->name);

        if (ioctl(fd, UI_DEV_SETUP, &setup) < 0) {
            perror("UI_DEV_SETUP");
            close(fd);
            return NULL;
        }
    } else if (!write_legacy_setup(fd, desc)) {
        close(fd);
        return NULL;
    }
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        perror("UI_DEV_CREATE");
        close(fd);
        return NULL;
    }

    struct gamepad *gp = calloc(1, sizeof(*gp));
    if (!gp) {
        perror("calloc");
        ioctl(fd, UI_DEV_DESTROY);
        close(fd);
        return NULL;
    }
    gp->fd = fd;
#ifdef UI_GET_SYSNAME
    if (ioctl(fd, UI_GET_SYSNAME(sizeof(gp->sysname)), gp->sysname) < 0) {
        gp->sysname[0] = '\0';
    }
#endif
    return gp;
}

static void emit_event(struct gamepad *gp, unsigned short type, unsigned short code, int value) {
    if (!gp) {
        return;
    }
    if (gp->pending_count == sizeof(gp->pending) / sizeof(gp->pending[0]) && !flush_events(gp)) {
        return;
    }

    struct input_event *ev = &gp->pending[gp->pending_count++];
    *ev = (struct input_event) {
        .type = type,
        .code = code,
        .value = value,
    };
    if (gp->observer) {
        gp->observer(ev, gp->observer_context);
    }
}

void gamepad_emit_key(struct gamepad *gp, unsigned short code, int value) {
    emit_event(gp, EV_KEY, code, value);
}

void gamepad_emit_abs(struct gamepad *gp, unsigned short code, int value) {
    emit_event(gp, EV_ABS, code, value);
}

void gamepad_emit_sw(struct gamepad *gp, unsigned short code, int value) {
    emit_event(gp, EV_SW, code, value);
}

void gamepad_sync(struct gamepad *gp) {
    emit_event(gp, EV_SYN, SYN_REPORT, 0);
    flush_events(gp);
}

void gamepad_destroy(struct gamepad *gp) {
    if (!gp) {
        return;
    }
    ioctl(gp->fd, UI_DEV_DESTROY);
    close(gp->fd);
    free(gp);
}

int gamepad_get_fd(struct gamepad *gp) {
    if (!gp) {
        return -1;
    }
    return gp->fd;
}

int gamepad_read_event(struct gamepad *gp, struct input_event *ev) {
    if (!gp || !ev) {
        return -1;
    }
    ssize_t r = read(gp->fd, ev, sizeof(*ev));
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        perror("read uinput");
        return -1;
    }
    if (r == 0) {
        return 0;
    }
    if (r != (ssize_t) sizeof(*ev)) {
        fprintf(stderr, "short read from uinput\n");
        return -1;
    }
    return 1;
}

void gamepad_set_event_observer(struct gamepad *gp, gamepad_event_observer observer, void *context) {
    if (!gp) {
        return;
    }
    gp->observer = observer;
    gp->observer_context = context;
}

int gamepad_get_event_path(struct gamepad *gp, char *path, size_t path_size) {
    if (!gp || !path || path_size == 0 || !gp->sysname[0]) {
        return -1;
    }

    char sysfs_path[128];
    if (snprintf(sysfs_path, sizeof(sysfs_path), "/sys/class/input/%s", gp->sysname) >= (int) sizeof(sysfs_path)) {
        return -1;
    }

    DIR *directory = opendir(sysfs_path);
    if (!directory) {
        return -1;
    }

    int result = -1;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }
        int length = snprintf(path, path_size, "/dev/input/%s", entry->d_name);
        if (length > 0 && (size_t) length < path_size) {
            result = 0;
        }
        break;
    }
    closedir(directory);
    return result;
}
