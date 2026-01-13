#pragma once

#include <stddef.h>

typedef struct inotify_proc inotify_status;

extern inotify_status *ino_proc;

inotify_status *inotify_create(void);

int inotify_track(inotify_status *proc, const char *dir, const char *name, int *exists_out);

void inotify_check(inotify_status *proc);
