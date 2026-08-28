#define _DEFAULT_SOURCE

#include "evdev.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifndef BUS_VIRTUAL
#define BUS_VIRTUAL 0x06
#endif

static int event_filename(const char *name) {
    return name && strncmp(name, "event", 5) == 0;
}

static int inspect_fd(int fd, struct evdev_source *source, const char *path, const char *requested_name) {
    struct input_id id = {0};
    char name[128] = {0};
    if (ioctl(fd, EVIOCGID, &id) < 0 || ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0 || id.bustype == BUS_VIRTUAL) {
        return -1;
    }
    if (requested_name && *requested_name && strcmp(name, requested_name) != 0) {
        return -1;
    }

    memset(source, 0, sizeof(*source));
    source->fd = fd;
    source->id = id;
    snprintf(source->path, sizeof(source->path), "%s", path);
    snprintf(source->name, sizeof(source->name), "%s", name);
    for (unsigned int code = 0; code < ABS_CNT; ++code) {
        if (ioctl(fd, EVIOCGABS(code), &source->abs_info[code]) == 0) {
            source->has_abs[code] = 1;
        }
    }
    return 0;
}

static int open_candidate(struct evdev_source *source, const char *path, const char *requested_name) {
    int fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    }
    if (fd < 0) {
        return -1;
    }
    if (inspect_fd(fd, source, path, requested_name) < 0) {
        close(fd);
        return -1;
    }
    return 0;
}

int evdev_source_open(
    struct evdev_source *source, const char *requested_path, const char *requested_name, int grab, int verbose
) {
    if (!source) {
        return -1;
    }
    memset(source, 0, sizeof(*source));
    source->fd = -1;

    if (requested_path && *requested_path) {
        if (open_candidate(source, requested_path, requested_name) < 0) {
            fprintf(stderr, "Unable to open evdev source %s\n", requested_path);
            return -1;
        }
    } else {
        DIR *directory = opendir("/dev/input");
        if (!directory) {
            return -1;
        }
        struct dirent *entry;
        while ((entry = readdir(directory)) != NULL) {
            if (!event_filename(entry->d_name)) {
                continue;
            }
            char path[128];
            int length = snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
            if (length <= 0 || (size_t) length >= sizeof(path)) {
                continue;
            }
            if (open_candidate(source, path, requested_name) == 0) {
                break;
            }
        }
        closedir(directory);
        if (source->fd < 0) {
            return -1;
        }
    }

    if (grab && ioctl(source->fd, EVIOCGRAB, 1) < 0) {
        perror("EVIOCGRAB evdev source");
        evdev_source_close(source);
        return -1;
    }
    if (verbose) {
        fprintf(
            stderr, "Using evdev source %s (%s, %04x:%04x:%04x)\n", source->path, source->name, source->id.bustype,
            source->id.vendor, source->id.product
        );
    }
    return 0;
}

int evdev_source_probe(const char *requested_path, const char *requested_name) {
    struct evdev_source source;
    if (evdev_source_open(&source, requested_path, requested_name, 0, 0) < 0) {
        return 0;
    }
    evdev_source_close(&source);
    return 1;
}

int evdev_source_read(struct evdev_source *source, struct input_event *events, size_t capacity) {
    if (!source || source->fd < 0 || !events || capacity == 0) {
        return -1;
    }
    ssize_t count;
    do {
        count = read(source->fd, events, capacity * sizeof(*events));
    } while (count < 0 && errno == EINTR);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return 0;
    }
    if (count < 0) {
        perror("read evdev source");
        return -1;
    }
    if (count == 0 || count % (ssize_t) sizeof(*events) != 0) {
        return count == 0 ? 0 : -1;
    }
    return (int) (count / (ssize_t) sizeof(*events));
}

void evdev_source_close(struct evdev_source *source) {
    if (!source) {
        return;
    }
    if (source->fd >= 0) {
        ioctl(source->fd, EVIOCGRAB, 0);
        close(source->fd);
    }
    source->fd = -1;
}
