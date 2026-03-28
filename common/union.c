#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include "common.h"
#include "device.h"
#include "collection.h"
#include "union.h"

#define UNION_ROM_NAME "ROMS"

#define UNION_EMPTY_THREADS 8
#define UNION_SET_INIT_BITS 11
#define UNION_SET_LOAD_NUM  3
#define UNION_SET_LOAD_DEN  4

#define UNION_LIST_INIT_CAP 512

#define DENTS_BUF_SIZE (64 * 1024)

static const char *union_mounts[3];

typedef struct {
    char **buckets;
    size_t capacity;
    size_t count;
} union_set;

static char _tombstone;
#define TOMBSTONE (&_tombstone)

static void union_set_init(union_set *s) {
    s->capacity = (size_t) 1 << UNION_SET_INIT_BITS;
    s->count = 0;
    s->buckets = calloc(s->capacity, sizeof(char *));
}

static void union_set_free(union_set *s) {
    if (!s->buckets) return;
    for (size_t i = 0; i < s->capacity; i++) {
        if (s->buckets[i] && s->buckets[i] != TOMBSTONE)
            free(s->buckets[i]);
    }
    free(s->buckets);
    s->buckets = NULL;
    s->count = 0;
    s->capacity = 0;
}

// Lowercased version of FNV1a
static size_t union_set_hash(const char *key, size_t capacity) {
    uint32_t h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *) key; *p; p++)
        h = (h ^ (unsigned char) tolower((unsigned char) *p)) * 16777619u;
    return (size_t) h & (capacity - 1);
}

/*
 * R 1  - name already present (duplicate, so skip it)
 * R 0  - name was absent and has been inserted
 * R -1 - alloc fail
 */
static int union_set_insert(union_set *s, const char *name) {
    if (s->count * UNION_SET_LOAD_DEN >= s->capacity * UNION_SET_LOAD_NUM) {
        size_t new_cap = s->capacity * 2;
        char **new_buckets = calloc(new_cap, sizeof(char *));

        if (!new_buckets) return -1;

        for (size_t i = 0; i < s->capacity; i++) {
            char *e = s->buckets[i];
            if (!e || e == TOMBSTONE) continue;

            size_t j = union_set_hash(e, new_cap);
            while (new_buckets[j]) j = (j + 1) & (new_cap - 1);

            new_buckets[j] = e;
        }

        free(s->buckets);

        s->buckets = new_buckets;
        s->capacity = new_cap;
    }

    size_t i = union_set_hash(name, s->capacity);

    while (s->buckets[i] && s->buckets[i] != TOMBSTONE) {
        if (strcasecmp(s->buckets[i], name) == 0) return 1;
        i = (i + 1) & (s->capacity - 1);
    }

    s->buckets[i] = strdup(name);
    if (!s->buckets[i]) return -1;

    s->count++;
    return 0;
}

typedef struct {
    char **names;
    int count;
    int capacity;
} union_list;

static int union_list_init(union_list *l) {
    l->count = 0;
    l->capacity = UNION_LIST_INIT_CAP;
    l->names = malloc((size_t) l->capacity * sizeof(char *));
    return l->names ? 1 : 0;
}

static int union_list_push(union_list *l, char *name) {
    if (l->count >= l->capacity) {
        int new_cap = l->capacity * 2;
        char **tmp = realloc(l->names, (size_t) new_cap * sizeof(char *));
        if (!tmp) return 0;
        l->names = tmp;
        l->capacity = new_cap;
    }
    l->names[l->count++] = name;
    return 1;
}

struct linux_dirent64 {
    unsigned long long d_ino;
    long long d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};

typedef struct {
    const char *mount;
    const char *rel_path;
    union_set dir_set;
    union_list dir_list;
    union_set file_set;
    union_list file_list;
    int ok;
} scan_worker_t;

static void union_join_path(char *out, size_t out_size,
                            const char *a, const char *b, const char *c);

static void scan_mount(const char *mount, const char *rel_path,
                       union_set *dir_set, union_list *dir_list,
                       union_set *file_set, union_list *file_list) {
    char scan_path[PATH_MAX];
    union_join_path(scan_path, sizeof(scan_path), mount, rel_path, NULL);

    int fd = open(scan_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) return;

    char *buf = malloc(DENTS_BUF_SIZE);
    if (!buf) {
        close(fd);
        return;
    }

    for (;;) {
        long dent_read = syscall(SYS_getdents64, fd, buf, DENTS_BUF_SIZE);
        if (dent_read <= 0) break;

        for (long pos = 0; pos < dent_read;) {
            struct linux_dirent64 *d = (struct linux_dirent64 *) (buf + pos);
            pos += d->d_reclen;

            const char *name = d->d_name;

            // Skip '.' and '..'
            if (name[0] == '.') {
                if (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')) continue;
            }

            int is_dir;

            if (d->d_type == DT_DIR) {
                is_dir = 1;
            } else if (d->d_type == DT_REG) {
                is_dir = 0;
            } else if (d->d_type == DT_LNK || d->d_type == DT_UNKNOWN) {
                char full_path[PATH_MAX];
                union_join_path(full_path, sizeof(full_path), scan_path, NULL, name);

                struct stat st;
                if (stat(full_path, &st) != 0) continue;

                if (S_ISDIR(st.st_mode)) {
                    is_dir = 1;
                } else if (S_ISREG(st.st_mode)) {
                    is_dir = 0;
                } else {
                    continue;
                }
            } else {
                continue;
            }

            if (should_skip(name, is_dir)) continue;

            union_set *set = is_dir ? dir_set : file_set;
            union_list *list = is_dir ? dir_list : file_list;

            int ret = union_set_insert(set, name);
            if (ret < 0) goto done;
            if (ret > 0) continue;

            char *copy = strdup(name);
            if (!copy || !union_list_push(list, copy)) {
                free(copy);
                goto done;
            }
        }
    }

    done:
    free(buf);
    close(fd);
}

static void scan_worker_thread_fn(scan_worker_t *w) {
    if (!w->mount || !*w->mount) return;
    scan_mount(w->mount, w->rel_path, &w->dir_set, &w->dir_list, &w->file_set, &w->file_list);
}

static void *scan_worker_thread(void *arg) {
    scan_worker_thread_fn((scan_worker_t *) arg);
    return NULL;
}

static void union_init_mounts(void) {
    union_mounts[0] = device.STORAGE.USB.MOUNT;
    union_mounts[1] = device.STORAGE.SDCARD.MOUNT;
    union_mounts[2] = device.STORAGE.ROM.MOUNT;
}

static int union_is_path_prefix(const char *path, const char *prefix) {
    if (!path || !prefix || !*prefix) return 0;

    size_t len = strlen(prefix);
    if (strncmp(path, prefix, len) != 0) return 0;

    return path[len] == '\0' || path[len] == '/';
}

static void union_join_path(char *out, size_t out_size, const char *a, const char *b, const char *c) {
    out[0] = '\0';
    if (a && *a) snprintf(out, out_size, "%s", a);

    if (b && *b) {
        size_t len = strlen(out);
        snprintf(out + len, out_size - len, "%s%s",
                 (len > 0 && out[len - 1] == '/') ? "" : "/", b);
    }

    if (c && *c) {
        size_t len = strlen(out);
        snprintf(out + len, out_size - len, "%s%s",
                 (len > 0 && out[len - 1] == '/') ? "" : "/", c);
    }

    remove_double_slashes(out);
}

void union_get_root_mount(char *out, size_t out_size) {
    union_init_mounts();

    for (size_t i = 0; i < A_SIZE(union_mounts); i++) {
        if (union_mounts[i] && *union_mounts[i] && dir_exist((char *) union_mounts[i])) {
            snprintf(out, out_size, "%s", union_mounts[i]);
            return;
        }
    }

    snprintf(out, out_size, "%s", device.STORAGE.ROM.MOUNT);
}

void union_get_relative_path(const char *path, char *out, size_t out_size) {
    const char *sub = path ? path : "";
    union_init_mounts();

    for (size_t i = 0; i < A_SIZE(union_mounts); i++) {
        if (union_mounts[i] && union_is_path_prefix(sub, union_mounts[i])) {
            sub += strlen(union_mounts[i]);
            break;
        }
    }

    while (*sub == '/') sub++;
    snprintf(out, out_size, "%s", sub);
}

void union_get_mount_path(const char *path, char *out, size_t out_size) {
    union_init_mounts();

    for (size_t i = 0; i < A_SIZE(union_mounts); i++) {
        if (union_mounts[i] && union_is_path_prefix(path, union_mounts[i])) {
            snprintf(out, out_size, "%s", union_mounts[i]);
            return;
        }
    }

    union_get_root_mount(out, out_size);
}

int union_is_root(const char *path) {
    char rel_path[PATH_MAX];
    union_get_relative_path(path, rel_path, sizeof(rel_path));

    return rel_path[0] == '\0';
}

int union_collect(const char *base_dir, char ***dir_names, int *dir_count, char ***file_names, int *file_count) {
    char rel_path[PATH_MAX];
    scan_worker_t workers[A_SIZE(union_mounts)];
    pthread_t threads[A_SIZE(union_mounts)];
    size_t n = A_SIZE(union_mounts);

    *dir_names = NULL;
    *file_names = NULL;
    *dir_count = 0;
    *file_count = 0;

    union_init_mounts();
    union_get_relative_path(base_dir, rel_path, sizeof(rel_path));

    for (size_t i = 0; i < n; i++) {
        scan_worker_t *w = &workers[i];
        w->mount = union_mounts[i];
        w->rel_path = rel_path;
        w->ok = 1;

        union_set_init(&w->dir_set);
        union_set_init(&w->file_set);

        if (!union_list_init(&w->dir_list) || !union_list_init(&w->file_list)) w->ok = 0;
        pthread_create(&threads[i], NULL, scan_worker_thread, w);
    }

    for (size_t i = 0; i < n; i++) pthread_join(threads[i], NULL);

    union_set global_dir_set, global_file_set;
    union_list global_dir_list, global_file_list;

    union_set_init(&global_dir_set);
    union_set_init(&global_file_set);

    if (!union_list_init(&global_dir_list) || !union_list_init(&global_file_list)) goto cleanup;

    for (size_t i = 0; i < n; i++) {
        scan_worker_t *w = &workers[i];

        for (int j = 0; j < w->dir_list.count; j++) {
            char *name = w->dir_list.names[j];
            int ret = union_set_insert(&global_dir_set, name);

            if (ret != 0) {
                free(name);
            } else {
                char *copy = strdup(name);
                if (!copy || !union_list_push(&global_dir_list, copy)) free(copy);
            }
        }

        for (int j = 0; j < w->file_list.count; j++) {
            char *name = w->file_list.names[j];
            int ret = union_set_insert(&global_file_set, name);

            if (ret != 0) {
                free(name);
            } else {
                char *copy = strdup(name);
                if (!copy || !union_list_push(&global_file_list, copy)) free(copy);
            }
        }
    }

    cleanup:
    for (size_t i = 0; i < n; i++) {
        scan_worker_t *w = &workers[i];

        union_set_free(&w->dir_set);
        union_set_free(&w->file_set);

        free(w->dir_list.names);
        free(w->file_list.names);
    }

    union_set_free(&global_dir_set);
    union_set_free(&global_file_set);

    *dir_names = global_dir_list.names;
    *dir_count = global_dir_list.count;
    *file_names = global_file_list.names;
    *file_count = global_file_list.count;

    return *dir_count + *file_count;
}

int union_resolve_path(const char *base_dir, const char *name, int want_dir, char *out, size_t out_size) {
    char rel_path[PATH_MAX];
    struct stat st;

    union_init_mounts();
    union_get_relative_path(base_dir, rel_path, sizeof(rel_path));

    for (size_t i = 0; i < A_SIZE(union_mounts); i++) {
        if (!union_mounts[i] || !*union_mounts[i]) continue;
        union_join_path(out, out_size, union_mounts[i], rel_path, name);

        if (stat(out, &st) == 0) {
            if ((want_dir && S_ISDIR(st.st_mode)) || (!want_dir && S_ISREG(st.st_mode))) return 1;
        }
    }

    union_join_path(out, out_size, device.STORAGE.ROM.MOUNT, rel_path, name);
    return 0;
}

int union_get_directory_item_count(const char *base_dir, const char *name, int count_type) {
    char rel_path[PATH_MAX];
    char target_rel[PATH_MAX];

    union_init_mounts();

    union_get_relative_path(base_dir, rel_path, sizeof(rel_path));
    union_join_path(target_rel, sizeof(target_rel), rel_path, NULL, name);

    int found_dirs = 0;
    int found_files = 0;

    for (size_t i = 0; i < A_SIZE(union_mounts); i++) {
        if (!union_mounts[i] || !*union_mounts[i]) continue;

        char scan_path[PATH_MAX];
        union_join_path(scan_path, sizeof(scan_path), union_mounts[i], target_rel, NULL);

        int fd = open(scan_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0) continue;

        char buf[DENTS_BUF_SIZE];

        for (;;) {
            long dent_read = syscall(SYS_getdents64, fd, buf, sizeof(buf));
            if (dent_read <= 0) break;

            for (long pos = 0; pos < dent_read;) {
                struct linux_dirent64 *d = (struct linux_dirent64 *) (buf + pos);
                pos += d->d_reclen;

                const char *entry = d->d_name;
                if (entry[0] == '.') if (entry[1] == '\0' || (entry[1] == '.' && entry[2] == '\0')) continue;

                int is_dir;
                if (d->d_type == DT_DIR) {
                    is_dir = 1;
                } else if (d->d_type == DT_REG) {
                    is_dir = 0;
                } else {
                    char full_path[PATH_MAX];
                    union_join_path(full_path, sizeof(full_path), scan_path, NULL, entry);

                    struct stat st;
                    if (stat(full_path, &st) != 0) continue;

                    if (S_ISDIR(st.st_mode)) {
                        is_dir = 1;
                    } else if (S_ISREG(st.st_mode)) {
                        is_dir = 0;
                    } else {
                        continue;
                    }
                }

                if (should_skip(entry, is_dir)) continue;

                if (is_dir) {
                    found_dirs = 1;
                } else {
                    found_files = 1;
                }

                if (count_type == COUNT_DIRS && found_dirs) {
                    close(fd);
                    return 1;
                }

                if (count_type == COUNT_FILES && found_files) {
                    close(fd);
                    return 1;
                }

                if (count_type == COUNT_BOTH && (found_dirs || found_files)) {
                    close(fd);
                    return 1;
                }
            }
        }

        close(fd);
    }

    return 0;
}

void union_get_roms_root(char *out, size_t out_size) {
    union_init_mounts();

    for (size_t i = 0; i < A_SIZE(union_mounts); i++) {
        if (!union_mounts[i] || !*union_mounts[i]) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", union_mounts[i], UNION_ROM_NAME);

        remove_double_slashes(path);
        if (dir_exist(path)) {
            snprintf(out, out_size, "%s", path);
            return;
        }
    }

    snprintf(out, out_size, "%s/%s", device.STORAGE.ROM.MOUNT, UNION_ROM_NAME);
}

char *union_get_title_root(char *path, char *out, size_t out_size) {
    if (!out || out_size == 0) return NULL;
    out[0] = '\0';

    if (!path || !*path) return NULL;

    if (at_base(path, UNION_ROM_NAME)) {
        snprintf(out, out_size, "%s", path);
        return out;
    }

    union_get_roms_root(out, out_size);
    return out;
}

int union_resolve_to_real(const char *union_path, char *out, size_t out_size) {
    if (!union_path || !out || out_size == 0) return 0;

    out[0] = '\0';
    if (strncmp(union_path, "/mnt/union/", 11) != 0) {
        snprintf(out, out_size, "%s", union_path);
        return file_exist(out) || dir_exist(out);
    }

    const char *rel = union_path + 11;

    const char *mounts[] = {
            device.STORAGE.USB.MOUNT,
            device.STORAGE.SDCARD.MOUNT,
            device.STORAGE.ROM.MOUNT
    };

    char test[PATH_MAX];

    for (size_t i = 0; i < A_SIZE(mounts); i++) {
        if (!mounts[i] || mounts[i][0] == '\0') continue;

        snprintf(test, sizeof(test), "%s/%s", mounts[i], rel);
        remove_double_slashes(test);

        if (file_exist(test) || dir_exist(test)) {
            snprintf(out, out_size, "%s", test);
            return 1;
        }
    }

    return 0;
}

int8_t get_empty_threads() {
    return UNION_EMPTY_THREADS;
}

void *empty_check_worker(void *arg) {
    empty_check_args_t *a = arg;

    for (int i = a->start; i < a->end; i++) {
        a->keep[i] = (union_get_directory_item_count(a->base_dir, a->dir_names[i], COUNT_BOTH) != 0) ? 1 : 0;
    }

    return NULL;
}
