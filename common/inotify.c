#include <sys/inotify.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "inotify.h"

// We'll probably do more in the future but this will do for now!
#define INOTIFY_MAX_TRACKED 4

inotify_status *ino_proc = NULL;

typedef struct {
    int wd;
    char *dir;
    char *name;
    int *exists_out;
    unsigned *changes_out;
} tracked_t;

struct inotify_proc {
    int fd;
    tracked_t tracked[INOTIFY_MAX_TRACKED];
    size_t count;
};

static int file_exists_joined(const char *dir, const char *name) {
    size_t dl = strlen(dir);
    size_t nl = strlen(name);
    int slash = (dl && dir[dl - 1] != '/');
    size_t len = dl + slash + nl + 1;

    char *p = malloc(len);
    if (!p) return 0;

    memcpy(p, dir, dl);
    if (slash) p[dl++] = '/';

    memcpy(p + dl, name, nl);
    p[dl + nl] = '\0';

    int ok = (access(p, F_OK) == 0);
    free(p);
    return ok;
}

inotify_status *inotify_create(void) {
    inotify_status *proc = calloc(1, sizeof(*proc));
    if (!proc) return NULL;

    proc->fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (proc->fd < 0) {
        free(proc);
        return NULL;
    }

    return proc;
}

static int ensure_dir_watch(inotify_status *proc, const char *dir, int *wd_out) {
    for (size_t i = 0; i < proc->count; i++) {
        if (proc->tracked[i].dir && strcmp(proc->tracked[i].dir, dir) == 0) {
            *wd_out = proc->tracked[i].wd;
            return 0;
        }
    }

    uint32_t mask = IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM |
                    IN_DELETE_SELF | IN_MOVE_SELF | IN_CLOSE_WRITE | IN_MODIFY;

    int wd = inotify_add_watch(proc->fd, dir, mask);
    if (wd < 0) return -1;

    *wd_out = wd;
    return 0;
}

int inotify_track(inotify_status *proc, const char *dir, const char *name, int *exists_out, unsigned *changes_out) {
    if (!proc || !dir || !name || !exists_out) return -1;
    if (proc->count >= INOTIFY_MAX_TRACKED) return -1;
    if (strchr(name, '/')) return -1;

    int wd;
    if (ensure_dir_watch(proc, dir, &wd) < 0) return -1;

    tracked_t *t = &proc->tracked[proc->count++];

    t->wd = wd;
    t->dir = strdup(dir);
    t->name = strdup(name);
    t->exists_out = exists_out;
    t->changes_out = changes_out;

    if (!t->dir || !t->name) return -1;

    *exists_out = file_exists_joined(dir, name);
    if (t->changes_out) *t->changes_out = 1;

    return 0;
}

void inotify_check(inotify_status *proc) {
    if (!proc || proc->fd < 0) return;

    // This is such bullshit, but apparently required for inotify on aarch64?!
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));

    for (;;) {
        ssize_t n = read(proc->fd, buf, sizeof(buf));
        if (n <= 0) {
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
            return;
        }

        size_t off = 0;
        while (off < (size_t) n) {
            const struct inotify_event *ev = (const struct inotify_event *) (buf + off);

            if (ev->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) {
                for (size_t i = 0; i < proc->count; i++) {
                    if (proc->tracked[i].wd == ev->wd) {
                        tracked_t *t = &proc->tracked[i];
                        *t->exists_out = file_exists_joined(t->dir, t->name);
                        if (t->changes_out) (*t->changes_out)++;
                    }
                }

                off += sizeof(struct inotify_event) + ev->len;
                continue;
            }

            if (ev->len && ev->name[0]) {
                for (size_t i = 0; i < proc->count; i++) {
                    tracked_t *t = &proc->tracked[i];

                    if (t->wd != ev->wd) continue;
                    if (strcmp(t->name, ev->name) != 0) continue;

                    if (ev->mask & (IN_CREATE | IN_MOVED_TO)) {
                        *t->exists_out = 1;
                        if (t->changes_out) (*t->changes_out)++;
                    } else if (ev->mask & (IN_DELETE | IN_MOVED_FROM)) {
                        *t->exists_out = 0;
                        if (t->changes_out) (*t->changes_out)++;
                    } else if (ev->mask & (IN_CLOSE_WRITE | IN_MODIFY)) {
                        if (t->changes_out) (*t->changes_out)++;
                    } else {
                        *t->exists_out = file_exists_joined(t->dir, t->name);
                        if (t->changes_out) (*t->changes_out)++;
                    }
                }
            }

            off += sizeof(struct inotify_event) + ev->len;
        }
    }
}
