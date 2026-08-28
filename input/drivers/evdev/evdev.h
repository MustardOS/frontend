#pragma once

#include <stddef.h>
#include <linux/input.h>

struct evdev_source {
    int fd;
    char path[128];
    char name[128];
    struct input_id id;
    struct input_absinfo abs_info[ABS_CNT];
    int has_abs[ABS_CNT];
};

int evdev_source_open(
    struct evdev_source *source, const char *requested_path, const char *requested_name, int grab, int verbose
);
int evdev_source_probe(const char *requested_path, const char *requested_name);
int evdev_source_read(struct evdev_source *source, struct input_event *events, size_t capacity);
void evdev_source_close(struct evdev_source *source);
