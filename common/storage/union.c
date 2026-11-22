#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <common/runtime/init.h>
#include <common/config/config.h>
#include <common/base/options.h>
#include <common/storage/fileio.h>
#include <common/base/strutil.h>
#include <common/platform/device.h>
#include <common/storage/union.h>

#define UNION_SET_INIT_BIT 8
#define UNION_SET_INIT_CAP 512
#define UNION_SET_LOAD_NUM 3
#define UNION_SET_LOAD_DEN 4

#define DENTS_BUF_SIZE (64 * 1024)

static const char *union_mounts[3];

typedef struct {
    char **buckets;
    size_t capacity;
    size_t count;
} union_set;

typedef struct {
    char **names;
    char **paths;
    int count;
    int capacity;
} union_entry_list;

typedef struct {
    const char *mount;
    const char *rel_path;
    union_set dir_set;
    union_entry_list dir_list;
    union_set file_set;
    union_entry_list file_list;
} scan_worker_t;

typedef struct {
    scan_worker_t *w;
    int **counts;
    int count_limit;
} deep_arg_t;

typedef struct {
    unsigned long long d_ino __attribute__((unused));
    long long d_off __attribute__((unused));
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
} linux_dirent64;

static char tstone;
#define TOMBSTONE (&tstone)

static void union_set_init(union_set *s) {
    s->capacity = (size_t) 1 << UNION_SET_INIT_BIT;
    s->count = 0;
    s->buckets = calloc(s->capacity, sizeof(char *));
}

static void union_set_free(union_set *s) {
    if (!s->buckets) return;
    free(s->buckets);

    s->buckets = NULL;
    s->count = 0;
    s->capacity = 0;
}

static int union_entry_list_init(union_entry_list *l) {
    l->count = 0;
    l->capacity = UNION_SET_INIT_CAP;

    l->names = malloc((size_t) l->capacity * sizeof(char *));
    l->paths = malloc((size_t) l->capacity * sizeof(char *));

    return l->names && l->paths;
}

static void union_entry_list_free_arrays(union_entry_list *l) {
    if (!l) return;

    free(l->names);
    free(l->paths);

    l->names = NULL;
    l->paths = NULL;

    l->count = 0;
    l->capacity = 0;
}

static int union_entry_list_push(union_entry_list *l, char *name, char *path) {
    if (!l->names || !l->paths) return 0;

    if (l->count >= l->capacity) {
        const int new_cap = l->capacity * 2;

        char **new_names = malloc((size_t) new_cap * sizeof(char *));
        char **new_paths = malloc((size_t) new_cap * sizeof(char *));
        if (!new_names || !new_paths) {
            free(new_names);
            free(new_paths);
            return 0;
        }

        memcpy(new_names, l->names, (size_t) l->count * sizeof(char *));
        memcpy(new_paths, l->paths, (size_t) l->count * sizeof(char *));

        free(l->names);
        free(l->paths);

        l->names = new_names;
        l->paths = new_paths;
        l->capacity = new_cap;
    }

    l->names[l->count] = name;
    l->paths[l->count] = path;
    l->count++;
    return 1;
}

// Lowercased version of FNV1a
static size_t union_set_hash(const char *key, const size_t capacity) {
    uint32_t h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *) key; *p; p++)
        h = (h ^ (unsigned char) tolower(*p)) * 16777619u;
    return (size_t) h & (capacity - 1);
}

/*
 * R 1  - name already present (duplicate, so skip it)
 * R 0  - name was absent and has been inserted
 * R -1 - alloc fail
 */
static int union_set_insert(union_set *s, const char *name) {
    if (!s->buckets) return -1;

    if (s->count * UNION_SET_LOAD_DEN >= s->capacity * UNION_SET_LOAD_NUM) {
        const size_t new_cap = s->capacity * 2;
        char **new_buckets = calloc(new_cap, sizeof(char *));

        if (!new_buckets) return -1;

        for (size_t i = 0; i < s->capacity; i++) {
            char *e = s->buckets[i];
            if (!e || e == TOMBSTONE) continue;

            size_t j = union_set_hash(e, new_cap);
            while (new_buckets[j])
                j = (j + 1) & (new_cap - 1);

            new_buckets[j] = e;
        }

        free(s->buckets);

        s->buckets = new_buckets;
        s->capacity = new_cap;
    }

    size_t i = union_set_hash(name, s->capacity);
    char **available = NULL;

    for (size_t probed = 0; probed < s->capacity && s->buckets[i]; probed++) {
        if (s->buckets[i] == TOMBSTONE) {
            if (!available) available = &s->buckets[i];
        } else if (strcasecmp(s->buckets[i], name) == 0) {
            return 1;
        }
        i = (i + 1) & (s->capacity - 1);
    }

    *(available ? available : &s->buckets[i]) = (char *) name;
    s->count++;
    return 0;
}

static void union_set_remove(union_set *s, const char *name) {
    if (!s->buckets) return;

    size_t i = union_set_hash(name, s->capacity);
    for (size_t probed = 0; probed < s->capacity && s->buckets[i]; probed++) {
        if (s->buckets[i] != TOMBSTONE && strcasecmp(s->buckets[i], name) == 0) {
            s->buckets[i] = TOMBSTONE;
            s->count--;
            return;
        }
        i = (i + 1) & (s->capacity - 1);
    }
}

static void union_init_mounts(void) {
    union_mounts[0] = device.storage.usb.mount;
    union_mounts[1] = device.storage.sdcard.mount;
    union_mounts[2] = device.storage.rom.mount;
}

static void union_join_path(char *out, const char *a, const char *b);

static int count_visible_children(const int fd, const char *name, char **buffer, const int limit) {
    if (!*buffer) *buffer = malloc(DENTS_BUF_SIZE);
    if (!*buffer) return -1;

    const int child_fd = openat(fd, name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (child_fd < 0) return 0;

    int count = 0;
    while (count < limit) {
        const long bytes = syscall(SYS_getdents64, child_fd, *buffer, DENTS_BUF_SIZE);
        if (bytes == 0) break;
        if (bytes < 0) {
            if (errno == EINTR) continue;
            count = -1;
            break;
        }
        for (long pos = 0; pos < bytes && count < limit;) {
            const linux_dirent64 *entry = (linux_dirent64 *) (*buffer + pos);
            pos += entry->d_reclen;
            const char *child = entry->d_name;
            if (child[0] == '.' && (!config.visual.hidden || !strcmp(child, ".") || !strcmp(child, ".."))) continue;
            const int is_dir = entry->d_type == DT_DIR;
            if (!is_dir && entry->d_type != DT_REG) continue;
            if (config.visual.hidden || !should_skip(child, is_dir)) count++;
        }
    }
    close(child_fd);
    return count;
}

static void scan_mount(
    const char *mount, const char *rel_path, union_set *dir_set, union_entry_list *dir_list, union_set *file_set,
    union_entry_list *file_list, int **counts_out, const int count_limit
) {
    char scan_path[PATH_MAX];
    union_join_path(scan_path, mount, rel_path);

    const int fd = open(scan_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) return;

    char *buf = malloc(DENTS_BUF_SIZE);
    if (!buf) {
        close(fd);
        return;
    }

    // readdir names cannot contain slashes. Reuse the normalized parent prefix.
    size_t prefix_len = strlen(scan_path);
    if (prefix_len < sizeof(scan_path) - 1 && scan_path[prefix_len - 1] != '/') scan_path[prefix_len++] = '/';
    char *child_buf = NULL;
    int count_capacity = 0;
    for (;;) {
        const long dent_read = syscall(SYS_getdents64, fd, buf, DENTS_BUF_SIZE);
        if (dent_read <= 0) break;

        for (long pos = 0; pos < dent_read;) {
            const linux_dirent64 *d = (linux_dirent64 *) (buf + pos);
            pos += d->d_reclen;

            const char *name = d->d_name;

            if (name[0] == '.') {
                if (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')) continue;
                if (!config.visual.hidden) continue;
            }

            int is_dir;
            if (d->d_type == DT_DIR) {
                is_dir = 1;
            } else if (d->d_type == DT_REG) {
                is_dir = 0;
            } else {
                continue;
            }

            if (!config.visual.hidden && should_skip(name, is_dir)) continue;

            union_set *set = is_dir ? dir_set : file_set;
            union_entry_list *list = is_dir ? dir_list : file_list;

            char *name_copy = strdup(name);
            if (!name_copy) goto done;

            const int ret = union_set_insert(set, name_copy);
            if (ret < 0) {
                free(name_copy);
                goto done;
            }

            if (ret > 0) {
                free(name_copy);
                continue;
            }

            const size_t name_len = strnlen(name, sizeof(scan_path) - prefix_len - 1);
            char *path_copy = malloc(prefix_len + name_len + 1);
            if (!path_copy) {
                union_set_remove(set, name_copy);
                free(name_copy);
                goto done;
            }

            memcpy(path_copy, scan_path, prefix_len);
            memcpy(path_copy + prefix_len, name, name_len);
            path_copy[prefix_len + name_len] = '\0';

            if (!union_entry_list_push(list, name_copy, path_copy)) {
                union_set_remove(set, name_copy);
                free(name_copy);
                free(path_copy);
                goto done;
            }

            if (is_dir && counts_out) {
                const int new_idx = list->count - 1;

                if (new_idx >= count_capacity) {
                    int *tmp = realloc(*counts_out, (size_t) list->capacity * sizeof(int));
                    if (!tmp) {
                        free(*counts_out);
                        *counts_out = NULL;
                        counts_out = NULL;
                        continue;
                    }
                    *counts_out = tmp;
                    count_capacity = list->capacity;
                }

                (*counts_out)[new_idx] = count_visible_children(fd, name, &child_buf, count_limit);
            }
        }
    }

done:
    free(child_buf);
    free(buf);
    close(fd);
}

static void *union_scan_thread(void *arg) {
    const deep_arg_t *da = arg;
    scan_worker_t *w = da->w;

    if (!w->mount || !*w->mount) return NULL;

    scan_mount(
        w->mount, w->rel_path, &w->dir_set, &w->dir_list, &w->file_set, &w->file_list, da->counts, da->count_limit
    );
    return NULL;
}

static int union_is_path_prefix(const char *path, const char *prefix) {
    if (!path || !prefix || !*prefix) return 0;

    const size_t len = strlen(prefix);
    if (strncmp(path, prefix, len) != 0) return 0;

    return path[len] == '\0' || path[len] == '/';
}

static void union_join_path(char *out, const char *a, const char *b) {
    out[0] = '\0';
    if (*a) snprintf(out, 4096, "%s", a);

    if (*b) {
        const size_t len = strlen(out);
        snprintf(out + len, 4096 - len, "%s%s", len > 0 && out[len - 1] == '/' ? "" : "/", b);
    }

    remove_double_slashes(out);
}

void union_get_root_mount(char *out, const size_t out_size) {
    union_init_mounts();

    for (size_t i = 0; i < A_SIZE(union_mounts); i++) {
        if (union_mounts[i] && *union_mounts[i] && dir_exist((char *) union_mounts[i])) {
            snprintf(out, out_size, "%s", union_mounts[i]);
            return;
        }
    }

    snprintf(out, out_size, "%s", device.storage.rom.mount);
}

void union_get_relative_path(const char *path, char *out, const size_t out_size) {
    const char *sub = path ? path : "";
    union_init_mounts();

    for (size_t i = 0; i < A_SIZE(union_mounts); i++) {
        if (union_mounts[i] && union_is_path_prefix(sub, union_mounts[i])) {
            sub += strlen(union_mounts[i]);
            break;
        }
    }

    while (*sub == '/')
        sub++;
    snprintf(out, out_size, "%s", sub);
}

void union_get_mount_path(const char *path, char *out, const size_t out_size) {
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

int union_collect(
    const char *base_dir, char ***dir_names, char ***dir_paths, int *dir_count, char ***file_names, char ***file_paths,
    int *file_count, int **dir_item_counts
) {
    return union_collect_limited(
        base_dir, dir_names, dir_paths, dir_count, file_names, file_paths, file_count, dir_item_counts, INT_MAX
    );
}

int union_collect_limited(
    const char *base_dir, char ***dir_names, char ***dir_paths, int *dir_count, char ***file_names, char ***file_paths,
    int *file_count, int **dir_item_counts, const int count_limit
) {
    char rel_path[PATH_MAX];
    const size_t n = A_SIZE(union_mounts);

    *dir_names = NULL;
    *dir_paths = NULL;

    *file_names = NULL;
    *file_paths = NULL;

    *dir_count = 0;
    *file_count = 0;

    if (dir_item_counts) *dir_item_counts = NULL;

    union_init_mounts();
    union_get_relative_path(base_dir, rel_path, sizeof(rel_path));

    scan_worker_t workers[A_SIZE(union_mounts)];
    pthread_t threads[A_SIZE(union_mounts)];
    int started[A_SIZE(union_mounts)] = {0};
    deep_arg_t args[A_SIZE(union_mounts)];

    int *mount_counts[A_SIZE(union_mounts)] = {0};

    for (size_t i = 0; i < n; i++) {
        scan_worker_t *w = &workers[i];

        w->mount = union_mounts[i];
        w->rel_path = rel_path;

        union_set_init(&w->dir_set);
        union_set_init(&w->file_set);

        union_entry_list_init(&w->dir_list);
        union_entry_list_init(&w->file_list);

        args[i].w = w;
        args[i].count_limit = count_limit;
        args[i].counts = dir_item_counts ? &mount_counts[i] : NULL;

        started[i] = pthread_create(&threads[i], NULL, union_scan_thread, &args[i]) == 0;
        if (!started[i]) union_scan_thread(&args[i]);
    }

    for (size_t i = 0; i < n; i++)
        if (started[i]) pthread_join(threads[i], NULL);

    // The first contributing mount already has a deduplicated list in priority order.
    size_t first = 0;
    while (first + 1 < n && !workers[first].dir_list.count && !workers[first].file_list.count) {
        union_entry_list_free_arrays(&workers[first].dir_list);
        union_entry_list_free_arrays(&workers[first].file_list);
        union_set_free(&workers[first].dir_set);
        union_set_free(&workers[first].file_set);
        first++;
    }

    union_set global_dir_set = workers[first].dir_set;
    union_set global_file_set = workers[first].file_set;
    union_entry_list global_dir_list = workers[first].dir_list;
    union_entry_list global_file_list = workers[first].file_list;
    int *merged_counts = mount_counts[first];
    size_t total_dirs = (size_t) global_dir_list.count;
    for (size_t i = first + 1; i < n; i++)
        total_dirs += (size_t) workers[i].dir_list.count;
    if (dir_item_counts && total_dirs > (size_t) global_dir_list.count) {
        const int have_counts = merged_counts != NULL;
        int *expanded = realloc(merged_counts, total_dirs * sizeof(int));
        if (!expanded)
            free(merged_counts);
        else if (!have_counts) {
            for (int j = 0; j < global_dir_list.count; j++)
                expanded[j] = -1;
        }
        merged_counts = expanded;
    }

    for (size_t i = first + 1; i < n; i++) {
        scan_worker_t *w = &workers[i];

        for (int j = 0; j < w->dir_list.count; j++) {
            char *name = w->dir_list.names[j];
            char *path = w->dir_list.paths[j];

            if (union_set_insert(&global_dir_set, name) == 0) {
                if (!union_entry_list_push(&global_dir_list, name, path)) {
                    union_set_remove(&global_dir_set, name);
                    free(name);
                    free(path);
                    continue;
                }

                if (merged_counts) {
                    const int idx = global_dir_list.count - 1;

                    merged_counts[idx] = mount_counts[i] ? mount_counts[i][j] : -1;
                }
            } else {
                free(name);
                free(path);
            }
        }

        for (int j = 0; j < w->file_list.count; j++) {
            char *name = w->file_list.names[j];
            char *path = w->file_list.paths[j];

            if (union_set_insert(&global_file_set, name) == 0) {
                if (!union_entry_list_push(&global_file_list, name, path)) {
                    union_set_remove(&global_file_set, name);
                    free(name);
                    free(path);
                }
            } else {
                free(name);
                free(path);
            }
        }

        union_entry_list_free_arrays(&w->dir_list);
        union_entry_list_free_arrays(&w->file_list);

        free(mount_counts[i]);

        union_set_free(&w->dir_set);
        union_set_free(&w->file_set);
    }

    union_set_free(&global_dir_set);
    union_set_free(&global_file_set);

    *dir_names = global_dir_list.names;
    *dir_paths = global_dir_list.paths;

    *dir_count = global_dir_list.count;

    *file_names = global_file_list.names;
    *file_paths = global_file_list.paths;
    *file_count = global_file_list.count;

    if (dir_item_counts) *dir_item_counts = merged_counts;

    return *dir_count + *file_count;
}

int union_get_directory_item_count(const char *base_dir, const char *name, const int count_type) {
    char rel_path[PATH_MAX];
    char target_rel[PATH_MAX];

    union_init_mounts();

    union_get_relative_path(base_dir, rel_path, sizeof(rel_path));

    if (name && *name) {
        if (rel_path[0]) {
            snprintf(target_rel, sizeof(target_rel), "%s/%s", rel_path, name);
        } else {
            snprintf(target_rel, sizeof(target_rel), "%s", name);
        }
    } else {
        snprintf(target_rel, sizeof(target_rel), "%s", rel_path);
    }

    char *buf = malloc(DENTS_BUF_SIZE);
    if (!buf) return 0;

    union_set seen;
    union_set_init(&seen);
    char **owned = NULL;
    size_t owned_count = 0;
    size_t owned_capacity = 0;

    int found = 0;

    for (size_t i = 0; i < A_SIZE(union_mounts); i++) {
        if (!union_mounts[i] || !*union_mounts[i]) continue;

        char scan_path[PATH_MAX];
        snprintf(scan_path, sizeof(scan_path), "%s/%s", union_mounts[i], target_rel);
        remove_double_slashes(scan_path);

        const int fd = open(scan_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0) continue;

        for (;;) {
            const long dent_read = syscall(SYS_getdents64, fd, buf, DENTS_BUF_SIZE);
            if (dent_read <= 0) break;

            for (long pos = 0; pos < dent_read;) {
                const linux_dirent64 *d = (linux_dirent64 *) (buf + pos);
                pos += d->d_reclen;

                const char *entry = d->d_name;
                if (entry[0] == '.') {
                    if (entry[1] == '\0' || (entry[1] == '.' && entry[2] == '\0')) continue;
                    if (!config.visual.hidden) continue;
                }

                int is_dir;
                if (d->d_type == DT_DIR) {
                    is_dir = 1;
                } else if (d->d_type == DT_REG) {
                    is_dir = 0;
                } else {
                    char full_path[PATH_MAX];
                    snprintf(full_path, sizeof(full_path), "%s/%s", scan_path, entry);

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

                if (!config.visual.hidden && should_skip(entry, is_dir)) continue;

                int count_quality = 0;
                if (count_type == count_dirs && is_dir) count_quality = 1;
                if (count_type == count_files && !is_dir) count_quality = 1;
                if (count_type == count_both) count_quality = 1;

                if (!count_quality) continue;

                if (owned_count == owned_capacity) {
                    const size_t capacity = owned_capacity ? owned_capacity * 2 : UNION_SET_INIT_CAP;
                    char **grown = realloc(owned, capacity * sizeof(*grown));
                    if (!grown) {
                        close(fd);
                        goto done;
                    }
                    owned = grown;
                    owned_capacity = capacity;
                }

                char *copy = strdup(entry);
                if (!copy) {
                    close(fd);
                    goto done;
                }

                const int ret = union_set_insert(&seen, copy);
                if (ret < 0) {
                    free(copy);
                    close(fd);
                    goto done;
                }

                if (ret > 0) {
                    free(copy);
                    continue;
                }

                owned[owned_count++] = copy;
                found++;
            }
        }

        close(fd);
    }

done:
    for (size_t i = 0; i < owned_count; ++i)
        free(owned[i]);
    free(owned);
    union_set_free(&seen);
    free(buf);
    return found;
}

void union_get_roms_root(char *out, const size_t out_size) {
    union_init_mounts();

    for (size_t i = 0; i < A_SIZE(union_mounts); i++) {
        if (!union_mounts[i] || !*union_mounts[i]) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", union_mounts[i], MAIN_ROM_DIR);

        remove_double_slashes(path);
        if (dir_exist(path)) {
            snprintf(out, out_size, "%s", path);
            return;
        }
    }

    snprintf(out, out_size, "%s/%s", device.storage.rom.mount, MAIN_ROM_DIR);
}

char *union_get_title_root(char *path, char *out, const size_t out_size) {
    if (!out || out_size == 0) return NULL;
    out[0] = '\0';

    if (!path || !*path) return NULL;

    if (at_base(path, MAIN_ROM_DIR)) {
        snprintf(out, out_size, "%s", path);
        return out;
    }

    union_get_roms_root(out, out_size);
    return out;
}

int union_resolve_to_real(const char *union_path, char *out, const size_t out_size) {
    if (!union_path || !out || out_size == 0) return 0;

    out[0] = '\0';
    union_init_mounts();

    const char *rel;

    if (strncmp(union_path, "/mnt/union/", 11) == 0) {
        rel = union_path + 11;
    } else if (strncmp(union_path, "/mnt/union", 10) == 0) {
        rel = union_path + 10;
        while (*rel == '/')
            rel++;
    } else if (strncmp(union_path, "/mnt/", 5) == 0) {
        rel = union_path + 5;
        const char *slash = strchr(rel, '/');
        if (slash) {
            rel = slash + 1;
        } else {
            snprintf(out, out_size, "%s", union_path);
            return file_exist(out) || dir_exist(out);
        }
    } else {
        snprintf(out, out_size, "%s", union_path);
        return file_exist(out) || dir_exist(out);
    }

    for (size_t i = 0; i < A_SIZE(union_mounts); i++) {
        if (!union_mounts[i] || !*union_mounts[i]) continue;

        char test_path[PATH_MAX];
        snprintf(test_path, sizeof(test_path), "%s/%s", union_mounts[i], rel);
        remove_double_slashes(test_path);

        if (file_exist(test_path) || dir_exist(test_path)) {
            snprintf(out, out_size, "%s", test_path);
            return 1;
        }
    }

    if (union_mounts[0] && *union_mounts[0]) {
        snprintf(out, out_size, "%s/%s", union_mounts[0], rel);
        remove_double_slashes(out);
    }

    return 0;
}

int union_rewrite_file_paths(const char *file) {
    if (!file_exist(file)) return 0;

    char *data = read_all_char_from(file);
    if (!data || !*data) {
        free(data);
        return 0;
    }

    char *cursor = data;
    int modified = 0;

    while ((cursor = strstr(cursor, "/mnt/union"))) {
        char resolved[PATH_MAX];

        char temp[PATH_MAX];
        snprintf(temp, sizeof(temp), "%s", cursor);

        char *end = strpbrk(temp, "\n\r|");
        if (end) *end = '\0';

        if (union_resolve_to_real(temp, resolved, sizeof(resolved))) {
            char *new_data = str_replace(data, temp, resolved);
            free(data);
            data = new_data;
            cursor = data;
            modified = 1;
        } else {
            cursor += 10;
        }
    }

    if (modified) {
        char tmp[PATH_MAX];
        if (snprintf(tmp, sizeof(tmp), "%s.tmp", file) < (int) sizeof(tmp)) {
            FILE *fp = fopen(tmp, "w");
            if (fp) {
                const int ok =
                    fwrite(data, 1, strlen(data), fp) == strlen(data) && fflush(fp) == 0 && fsync(fileno(fp)) == 0;
                fclose(fp);
                if (!ok || file_rename(tmp, file) != 0) remove(tmp);
            }
        }
    }

    free(data);
    return modified;
}
