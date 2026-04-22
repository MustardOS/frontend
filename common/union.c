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
#include "config.h"
#include "options.h"
#include "device.h"
#include "collection.h"
#include "union.h"

#define UNION_SET_INIT_BIT 8
#define UNION_SET_INIT_CAP 512
#define UNION_SET_LOAD_NUM 4
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
    int ok;
} scan_worker_t;

typedef struct {
    scan_worker_t *w;
    int **counts;
} deep_arg_t;

struct linux_dirent64 {
    unsigned long long d_ino;
    long long d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};

static char _tombstone;
#define TOMBSTONE (&_tombstone)

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
    if (l->count >= l->capacity) {
        int new_cap = l->capacity * 2;
        char **new_names = realloc(l->names, (size_t) new_cap * sizeof(char *));
        char **new_paths = realloc(l->paths, (size_t) new_cap * sizeof(char *));
        if (!new_names || !new_paths) {
            free(new_names);
            free(new_paths);
            return 0;
        }

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

    s->buckets[i] = (char *) name;
    s->count++;
    return 0;
}

static void union_init_mounts(void) {
    union_mounts[0] = device.STORAGE.USB.MOUNT;
    union_mounts[1] = device.STORAGE.SDCARD.MOUNT;
    union_mounts[2] = device.STORAGE.ROM.MOUNT;
}

static void union_join_path(char *out, const char *a, const char *b);

static void scan_mount(const char *mount, const char *rel_path,
                       union_set *dir_set, union_entry_list *dir_list,
                       union_set *file_set, union_entry_list *file_list,
                       int **counts_out) {
    char scan_path[PATH_MAX];
    union_join_path(scan_path, mount, rel_path);

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

            if (name[0] == '.') {
                if (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')) continue;
                if (!config.VISUAL.HIDDEN) continue;
            }

            int is_dir;
            if (d->d_type == DT_DIR) {
                is_dir = 1;
            } else if (d->d_type == DT_REG) {
                is_dir = 0;
            } else {
                continue;
            }

            if (!config.VISUAL.HIDDEN && should_skip(name, is_dir)) continue;

            union_set *set = is_dir ? dir_set : file_set;
            union_entry_list *list = is_dir ? dir_list : file_list;

            char *name_copy = strdup(name);
            if (!name_copy) goto done;

            int ret = union_set_insert(set, name_copy);
            if (ret < 0) {
                free(name_copy);
                goto done;
            }

            if (ret > 0) {
                free(name_copy);
                continue;
            }

            char full_path[PATH_MAX];
            union_join_path(full_path, scan_path, name);

            char *path_copy = strdup(full_path);
            if (!path_copy) {
                free(name_copy);
                goto done;
            }

            if (!union_entry_list_push(list, name_copy, path_copy)) {
                free(name_copy);
                free(path_copy);
                goto done;
            }

            if (is_dir && counts_out) {
                int new_idx = list->count - 1;

                int *tmp = realloc(*counts_out, (size_t) list->count * sizeof(int));
                if (tmp) {
                    *counts_out = tmp;

                    int child_fd = open(full_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
                    if (child_fd < 0) {
                        (*counts_out)[new_idx] = 0;
                    } else {
                        char *child_buf = malloc(DENTS_BUF_SIZE);
                        int child_count = 0;

                        if (child_buf) {
                            for (;;) {
                                long cr = syscall(SYS_getdents64, child_fd, child_buf, DENTS_BUF_SIZE);
                                if (cr <= 0) break;

                                for (long cp = 0; cp < cr;) {
                                    struct linux_dirent64 *cd = (struct linux_dirent64 *) (child_buf + cp);
                                    cp += cd->d_reclen;

                                    const char *cn = cd->d_name;
                                    if (cn[0] == '.') {
                                        if (cn[1] == '\0' || (cn[1] == '.' && cn[2] == '\0')) continue;
                                        if (!config.VISUAL.HIDDEN) continue;
                                    }

                                    int cis_dir = (cd->d_type == DT_DIR) ? 1 : (cd->d_type == DT_REG) ? 0 : -1;
                                    if (cis_dir < 0) continue;

                                    if (config.VISUAL.HIDDEN || !should_skip(cn, cis_dir)) child_count++;
                                }
                            }

                            free(child_buf);
                        }

                        close(child_fd);
                        (*counts_out)[new_idx] = child_count;
                    }
                }
            }
        }
    }

    done:
    free(buf);
    close(fd);
}

static void *union_scan_thread(void *arg) {
    deep_arg_t *da = (deep_arg_t *) arg;
    scan_worker_t *w = da->w;

    if (!w->mount || !*w->mount) pthread_exit(NULL);

    scan_mount(w->mount, w->rel_path, &w->dir_set, &w->dir_list, &w->file_set, &w->file_list, da->counts);
    pthread_exit(NULL);
}

static int union_is_path_prefix(const char *path, const char *prefix) {
    if (!path || !prefix || !*prefix) return 0;

    size_t len = strlen(prefix);
    if (strncmp(path, prefix, len) != 0) return 0;

    return path[len] == '\0' || path[len] == '/';
}

static void union_join_path(char *out, const char *a, const char *b) {
    out[0] = '\0';
    if (*a) snprintf(out, 4096, "%s", a);

    if (*b) {
        size_t len = strlen(out);
        snprintf(out + len, 4096 - len, "%s%s", (len > 0 && out[len - 1] == '/') ? "" : "/", b);
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

int union_collect(const char *base_dir, char ***dir_names, char ***dir_paths, int *dir_count,
                  char ***file_names, char ***file_paths, int *file_count, int **dir_item_counts) {
    char rel_path[PATH_MAX];
    size_t n = A_SIZE(union_mounts);

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
    deep_arg_t args[A_SIZE(union_mounts)];

    int *mount_counts[A_SIZE(union_mounts)];
    memset(mount_counts, 0, sizeof(mount_counts));

    for (size_t i = 0; i < n; i++) {
        scan_worker_t *w = &workers[i];

        w->mount = union_mounts[i];
        w->rel_path = rel_path;

        union_set_init(&w->dir_set);
        union_set_init(&w->file_set);

        union_entry_list_init(&w->dir_list);
        union_entry_list_init(&w->file_list);

        args[i].w = w;
        args[i].counts = dir_item_counts ? &mount_counts[i] : NULL;

        pthread_create(&threads[i], NULL, union_scan_thread, &args[i]);
    }

    for (size_t i = 0; i < n; i++) pthread_join(threads[i], NULL);

    union_set global_dir_set, global_file_set;
    union_entry_list global_dir_list, global_file_list;

    union_set_init(&global_dir_set);
    union_set_init(&global_file_set);

    union_entry_list_init(&global_dir_list);
    union_entry_list_init(&global_file_list);

    int *merged_counts = NULL;
    int merged_cap = 0;

    if (dir_item_counts) {
        merged_counts = malloc(sizeof(int));
        if (merged_counts) merged_cap = 1;
    }

    for (size_t i = 0; i < n; i++) {
        scan_worker_t *w = &workers[i];

        for (int j = 0; j < w->dir_list.count; j++) {
            char *name = w->dir_list.names[j];
            char *path = w->dir_list.paths[j];

            if (union_set_insert(&global_dir_set, name) == 0) {
                union_entry_list_push(&global_dir_list, name, path);

                if (merged_counts) {
                    int idx = global_dir_list.count - 1;

                    if (idx >= merged_cap) {
                        int new_cap = merged_cap * 2;
                        int *tmp = realloc(merged_counts, (size_t) new_cap * sizeof(int));
                        if (tmp) {
                            merged_counts = tmp;
                            merged_cap = new_cap;
                        }
                    }

                    if (idx < merged_cap) {
                        int cnt = -1;
                        if (mount_counts[i] && j < w->dir_list.count) cnt = mount_counts[i][j];
                        merged_counts[idx] = cnt;
                    }
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
                union_entry_list_push(&global_file_list, name, path);
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

    if (dir_item_counts) {
        if (merged_counts && *dir_count > 0) {
            int *tmp = realloc(merged_counts, (size_t) (*dir_count) * sizeof(int));
            *dir_item_counts = tmp ? tmp : merged_counts;
        } else {
            free(merged_counts);
            *dir_item_counts = NULL;
        }
    }

    return *dir_count + *file_count;
}

int union_get_directory_item_count(const char *base_dir, const char *name, int count_type) {
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

    int found = 0;

    for (size_t i = 0; i < A_SIZE(union_mounts); i++) {
        if (!union_mounts[i] || !*union_mounts[i]) continue;

        char scan_path[PATH_MAX];
        snprintf(scan_path, sizeof(scan_path), "%s/%s", union_mounts[i], target_rel);
        remove_double_slashes(scan_path);

        int fd = open(scan_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0) continue;

        for (;;) {
            long dent_read = syscall(SYS_getdents64, fd, buf, DENTS_BUF_SIZE);
            if (dent_read <= 0) break;

            for (long pos = 0; pos < dent_read;) {
                struct linux_dirent64 *d = (struct linux_dirent64 *) (buf + pos);
                pos += d->d_reclen;

                const char *entry = d->d_name;
                if (entry[0] == '.') {
                    if (entry[1] == '\0' || (entry[1] == '.' && entry[2] == '\0')) continue;
                    if (!config.VISUAL.HIDDEN) continue;
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

                if (!config.VISUAL.HIDDEN && should_skip(entry, is_dir)) continue;

                int count_quality = 0;
                if (count_type == COUNT_DIRS && is_dir) count_quality = 1;
                if (count_type == COUNT_FILES && !is_dir) count_quality = 1;
                if (count_type == COUNT_BOTH) count_quality = 1;

                if (!count_quality) continue;

                char *copy = strdup(entry);
                if (!copy) goto done;

                int ret = union_set_insert(&seen, copy);
                if (ret < 0) {
                    free(copy);
                    goto done;
                }

                if (ret > 0) {
                    free(copy);
                    continue;
                }

                found++;
            }
        }

        close(fd);
    }

    done:
    union_set_free(&seen);
    free(buf);
    return found;
}

void union_get_roms_root(char *out, size_t out_size) {
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

    snprintf(out, out_size, "%s/%s", device.STORAGE.ROM.MOUNT, MAIN_ROM_DIR);
}

char *union_get_title_root(char *path, char *out, size_t out_size) {
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

int union_resolve_to_real(const char *union_path, char *out, size_t out_size) {
    if (!union_path || !out || out_size == 0) return 0;

    out[0] = '\0';
    union_init_mounts();

    const char *rel;

    if (strncmp(union_path, "/mnt/union/", 11) == 0) {
        rel = union_path + 11;
    } else if (strncmp(union_path, "/mnt/union", 10) == 0) {
        rel = union_path + 10;
        while (*rel == '/') rel++;
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
        FILE *fp = fopen(file, "w");
        if (fp) {
            fwrite(data, 1, strlen(data), fp);
            fflush(fp);
            fsync(fileno(fp));
            fclose(fp);
        }
    }

    free(data);
    return modified;
}
