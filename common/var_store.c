#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>
#include "var_store.h"

#define VS_MAX_WALK_DEPTH 16

#define MAX_LEN_SZ 4096

static const struct {
    var_ns_t outer;
    var_ns_t inner;
    const char *prefix;
} vs_alias[] = {
    {vs_ns_global, vs_ns_system, "system/"},
};

static uint32_t next_pow2(uint32_t v) {
    if (v < 8) return 8;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return v + 1;
}

static size_t vs_file_size(const uint32_t capacity) {
    return sizeof(var_header_t) + (size_t) capacity * sizeof(var_slot_t);
}

void vs_copy_out(char *dst, const size_t dst_sz, const char *src, const size_t src_sz) {
    size_t n = 0;
    while (n < src_sz && n + 1 < dst_sz && src[n] != '\0') {
        dst[n] = src[n];
        n++;
    }
    dst[n] = '\0';
}

int vs_valid_key(const char *key) {
    if (key == NULL || key[0] == '\0') return 0;
    if (key[0] == '/') return 0;

    const size_t len = strlen(key);
    if (len >= VS_KEY_MAX) return 0;

    for (size_t i = 0; i < len; i++) {
        const char c = key[i];
        const int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'
                       || c == '.' || c == '/' || c == '-';
        if (!ok) return 0;
        if (c == '/' && key[i + 1] == '/') return 0;
    }

    for (size_t i = 0; i + 1 < len; i++) {
        if (key[i] != '.' || key[i + 1] != '.') continue;
        if (i != 0 && key[i - 1] != '/') continue;
        if (key[i + 2] != '\0' && key[i + 2] != '/') continue;
        return 0;
    }

    return 1;
}

uint32_t vs_hash(const var_ns_t ns, const char *key) {
    uint32_t h = 2166136261u ^ (uint32_t) ns;
    for (const unsigned char *p = (const unsigned char *) key; *p; p++) {
        h ^= *p;
        h *= 16777619u;
    }
    return h;
}

int vs_canon(const var_ns_t ns, const char *key, var_ns_t *out_ns, char *out_key, const size_t out_sz) {
    if (key == NULL || out_ns == NULL || out_key == NULL || out_sz == 0) return vs_err_inval;
    if (ns >= vs_ns_count) return vs_err_inval;
    if (!vs_valid_key(key)) return vs_err_inval;

    const char *tail = key;
    var_ns_t target = ns;

    for (size_t i = 0; i < sizeof(vs_alias) / sizeof(vs_alias[0]); i++) {
        if (ns != vs_alias[i].outer) continue;

        const size_t plen = strlen(vs_alias[i].prefix);
        if (strncmp(key, vs_alias[i].prefix, plen) != 0 || key[plen] == '\0') continue;

        tail = key + plen;
        target = vs_alias[i].inner;
        break;
    }

    const size_t len = strlen(tail);
    if (len + 1 > out_sz) return vs_err_inval;

    memcpy(out_key, tail, len + 1);
    *out_ns = target;

    return vs_ok;
}

static int vs_header_sane(const var_store_t *vs) {
    const var_header_t *h = vs->hdr;

    if (h->magic != VS_MAGIC || h->version != VS_VERSION) return 0;
    if (h->capacity < 8 || h->capacity != next_pow2(h->capacity)) return 0;
    if (vs_file_size(h->capacity) > vs->map_size) return 0;

    return 1;
}

int vs_open(
    var_store_t *vs, const char *path, const int create_if_missing, const uint32_t capacity_hint, const int writable
) {
    if (vs == NULL || path == NULL) return vs_err_inval;
    memset(vs, 0, sizeof(*vs));
    vs->fd = -1;

    const int flags = (writable ? O_RDWR : O_RDONLY) | O_CLOEXEC;
    int fd = open(path, flags);
    int created = 0;

    if (fd < 0) {
        if (errno != ENOENT || !create_if_missing || !writable) return vs_err_io;

        const uint32_t capacity = next_pow2(capacity_hint ? capacity_hint : VS_DEF_CAP);

        fd = open(path, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd < 0) {
            fd = open(path, flags);
            if (fd < 0) return vs_err_io;
        } else {
            if (flock(fd, LOCK_EX) != 0 || ftruncate(fd, (off_t) vs_file_size(capacity)) != 0) {
                close(fd);
                unlink(path);
                return vs_err_io;
            }
            created = 1;
        }
    }

    if (!created && flock(fd, LOCK_SH) != 0) {
        close(fd);
        return vs_err_lock;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || (size_t) st.st_size < sizeof(var_header_t)) {
        flock(fd, LOCK_UN);
        close(fd);
        return vs_err_io;
    }

    const int prot = PROT_READ | (writable ? PROT_WRITE : 0);
    void *map = mmap(NULL, (size_t) st.st_size, prot, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        flock(fd, LOCK_UN);
        close(fd);
        return vs_err_io;
    }

    vs->fd = fd;
    vs->map = map;
    vs->map_size = (size_t) st.st_size;
    vs->hdr = (var_header_t *) map;
    vs->slots = (var_slot_t *) ((char *) map + sizeof(var_header_t));
    vs->writable = writable;

    if (created) {
        memset(vs->hdr, 0, sizeof(var_header_t));
        vs->hdr->magic = VS_MAGIC;
        vs->hdr->version = VS_VERSION;
        vs->hdr->capacity = (uint32_t) ((vs->map_size - sizeof(var_header_t)) / sizeof(var_slot_t));
    }

    const int sane = vs_header_sane(vs);
    flock(fd, LOCK_UN);

    if (!sane) {
        vs_close(vs);
        return vs_err_io;
    }

    return vs_ok;
}

void vs_close(var_store_t *vs) {
    if (vs == NULL) return;
    if (vs->map && vs->map != MAP_FAILED) munmap(vs->map, vs->map_size);
    if (vs->fd >= 0) close(vs->fd);
    memset(vs, 0, sizeof(*vs));
    vs->fd = -1;
}

static long find_slot(const var_store_t *vs, const var_ns_t ns, const char *key, const uint32_t hash) {
    const uint32_t cap = vs->hdr->capacity;
    uint32_t idx = hash & cap - 1;

    for (uint32_t i = 0; i < cap; i++) {
        const var_slot_t *s = &vs->slots[idx];
        if (!s->occupied && !s->tombstone) return -1;
        if (s->occupied && !s->tombstone && s->ns == (uint8_t) ns && s->hash == hash
            && strncmp(s->key, key, VS_KEY_MAX) == 0) {
            return idx;
        }
        idx = idx + 1 & cap - 1;
    }

    return -1;
}

static long find_slot_for_insert(const var_store_t *vs, const var_ns_t ns, const char *key, const uint32_t hash) {
    const uint32_t cap = vs->hdr->capacity;
    uint32_t idx = hash & cap - 1;
    long first_free = -1;

    for (uint32_t i = 0; i < cap; i++) {
        const var_slot_t *s = &vs->slots[idx];
        if (s->occupied && !s->tombstone && s->ns == (uint8_t) ns && s->hash == hash
            && strncmp(s->key, key, VS_KEY_MAX) == 0) {
            return idx;
        }
        if (first_free < 0 && (!s->occupied || s->tombstone)) first_free = (long) idx;
        if (!s->occupied && !s->tombstone) break;
        idx = idx + 1 & cap - 1;
    }

    return first_free;
}

static int slot_is(const var_slot_t *s, const var_ns_t ns, const char *key) {
    return s->occupied && !s->tombstone && s->ns == (uint8_t) ns && strncmp(s->key, key, VS_KEY_MAX) == 0;
}

static void slot_claim(const var_store_t *vs, var_slot_t *s, const var_ns_t ns, const char *key, const uint32_t hash) {
    if (slot_is(s, ns, key)) return;

    memset(s, 0, sizeof(*s));
    snprintf(s->key, VS_KEY_MAX, "%s", key);
    s->ns = (uint8_t) ns;
    s->hash = hash;
    s->occupied = 1;
    vs->hdr->count++;
}

int vs_get_locked(const var_store_t *vs, const var_ns_t ns, const char *key, char *out, const size_t out_sz) {
    if (vs == NULL || out == NULL || out_sz == 0) return vs_err_inval;
    out[0] = '\0';

    char ckey[VS_KEY_MAX];
    var_ns_t cns;
    if (vs_canon(ns, key, &cns, ckey, sizeof(ckey)) != vs_ok) return vs_err_inval;

    const long idx = find_slot(vs, cns, ckey, vs_hash(cns, ckey));
    if (idx < 0) return vs_notfound;

    vs_copy_out(out, out_sz, vs->slots[idx].value, VS_VAL_MAX);
    return vs_ok;
}

int vs_lock_shared(const var_store_t *vs) {
    if (vs == NULL) return vs_err_inval;
    return flock(vs->fd, LOCK_SH) == 0 ? vs_ok : vs_err_lock;
}

void vs_unlock(const var_store_t *vs) {
    if (vs != NULL) flock(vs->fd, LOCK_UN);
}

int vs_get(const var_store_t *vs, const var_ns_t ns, const char *key, char *out, const size_t out_sz) {
    if (vs == NULL || out == NULL || out_sz == 0) return vs_err_inval;
    out[0] = '\0';

    if (vs_lock_shared(vs) != vs_ok) return vs_err_lock;
    const int rc = vs_get_locked(vs, ns, key, out, out_sz);
    vs_unlock(vs);

    return rc;
}

static int
vs_set_locked(const var_store_t *vs, const var_ns_t ns, const char *key, const char *value, const int dirty) {
    const uint32_t hash = vs_hash(ns, key);
    const long idx = find_slot_for_insert(vs, ns, key, hash);
    if (idx < 0) return vs_err_full;

    var_slot_t *s = &vs->slots[idx];
    slot_claim(vs, s, ns, key, hash);
    s->tombstone = 0;
    snprintf(s->value, VS_VAL_MAX, "%s", value);
    s->dirty = (uint8_t) dirty;

    return vs_ok;
}

int vs_set(const var_store_t *vs, const var_ns_t ns, const char *key, const char *value) {
    if (vs == NULL || value == NULL) return vs_err_inval;
    if (!vs->writable) return vs_err_inval;
    if (strlen(value) >= VS_VAL_MAX) return vs_err_inval;

    char ckey[VS_KEY_MAX];
    var_ns_t cns;
    if (vs_canon(ns, key, &cns, ckey, sizeof(ckey)) != vs_ok) return vs_err_inval;

    if (flock(vs->fd, LOCK_EX) != 0) return vs_err_lock;
    const int rc = vs_set_locked(vs, cns, ckey, value, 1);
    flock(vs->fd, LOCK_UN);

    return rc;
}

static int atomic_write_file(const char *path, const char *value) {
    char tmp[PATH_MAX];
    const int n = snprintf(tmp, sizeof(tmp), "%s.tmp.%d", path, (int) getpid());
    if (n < 0 || (size_t) n >= sizeof(tmp)) return -1;

    const int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) return -1;

    const size_t len = strlen(value);
    const ssize_t written = len ? write(fd, value, len) : 0;
    const int cerr = close(fd);

    if (written < 0 || (size_t) written != len || cerr != 0) {
        unlink(tmp);
        return -1;
    }

    if (rename(tmp, path) != 0) {
        unlink(tmp);
        return -1;
    }

    return 0;
}

static int parent_dir_of(const char *path, char *out) {
    const char *slash = strrchr(path, '/');
    if (!slash) return -1;

    const size_t len = (size_t) (slash - path);
    if (len == 0 || len + 1 > MAX_LEN_SZ) return -1;

    memcpy(out, path, len);
    out[len] = '\0';
    return 0;
}

static int slot_path(const var_dirs_t *dirs, const var_ns_t ns, const char *key, char *out) {
    if (ns >= vs_ns_count || !dirs->base[ns]) return -1;

    const int n = snprintf(out, MAX_LEN_SZ, "%s/%s", dirs->base[ns], key);
    return n < 0 || (size_t) n >= MAX_LEN_SZ ? -1 : 0;
}

int vs_write(const var_dirs_t *dirs, const var_ns_t ns, const char *key, const char *value) {
    if (dirs == NULL || value == NULL) return vs_err_inval;
    if (strlen(value) >= VS_VAL_MAX) return vs_err_inval;

    char ckey[VS_KEY_MAX];
    var_ns_t cns;
    if (vs_canon(ns, key, &cns, ckey, sizeof(ckey)) != vs_ok) return vs_err_inval;

    char path[PATH_MAX], dir[PATH_MAX];
    if (slot_path(dirs, cns, ckey, path) != 0) return vs_err_inval;

    struct stat dst;
    if (parent_dir_of(path, dir) != 0 || stat(dir, &dst) != 0 || !S_ISDIR(dst.st_mode)) {
        return vs_err_io;
    }

    return atomic_write_file(path, value) == 0 ? vs_ok : vs_err_io;
}

int vs_store(const var_store_t *vs, const var_dirs_t *dirs, const var_ns_t ns, const char *key, const char *value) {
    if (vs == NULL || dirs == NULL || value == NULL) return vs_err_inval;
    if (!vs->writable) return vs_err_inval;

    char ckey[VS_KEY_MAX];
    var_ns_t cns;
    if (vs_canon(ns, key, &cns, ckey, sizeof(ckey)) != vs_ok) return vs_err_inval;

    if (flock(vs->fd, LOCK_EX) != 0) return vs_err_lock;

    int rc = vs_write(dirs, cns, ckey, value);
    if (rc == vs_ok) rc = vs_set_locked(vs, cns, ckey, value, 0);

    flock(vs->fd, LOCK_UN);
    return rc;
}

static int vs_del_locked(const var_store_t *vs, const var_ns_t ns, const char *key) {
    const long idx = find_slot(vs, ns, key, vs_hash(ns, key));
    if (idx < 0) return vs_notfound;

    var_slot_t *s = &vs->slots[idx];
    s->tombstone = 1;
    s->dirty = 0;
    if (vs->hdr->count > 0) vs->hdr->count--;

    return vs_ok;
}

int vs_del(const var_store_t *vs, const var_ns_t ns, const char *key) {
    if (vs == NULL) return vs_err_inval;
    if (!vs->writable) return vs_err_inval;

    char ckey[VS_KEY_MAX];
    var_ns_t cns;
    if (vs_canon(ns, key, &cns, ckey, sizeof(ckey)) != vs_ok) return vs_err_inval;

    if (flock(vs->fd, LOCK_EX) != 0) return vs_err_lock;
    const int rc = vs_del_locked(vs, cns, ckey);
    flock(vs->fd, LOCK_UN);

    return rc;
}

static int read_first_line(const char *path, char *out) {
    const int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;

    char buf[VS_VAL_MAX];
    const ssize_t n = read(fd, buf, sizeof(buf));
    close(fd);
    if (n < 0) return -1;

    const char *nl = memchr(buf, '\n', (size_t) n);
    size_t len = nl ? (size_t) (nl - buf) : (size_t) n;
    if (len >= MAX_LEN_SZ) return -1;
    if (len > 0 && buf[len - 1] == '\r') len--;

    memcpy(out, buf, len);
    out[len] = '\0';
    return 0;
}

typedef struct {
    var_store_t *vs;
    const var_dirs_t *dirs;
    var_ns_t ns;
    int indexed;
    int skipped;
    char path[PATH_MAX];
    char key[VS_KEY_MAX];
} vs_walk_t;

static void build_insert(vs_walk_t *w, const char *key, const char *value) {
    const var_store_t *vs = w->vs;
    const uint32_t hash = vs_hash(w->ns, key);
    const long idx = find_slot_for_insert(vs, w->ns, key, hash);
    if (idx < 0) {
        w->skipped++;
        return;
    }

    var_slot_t *s = &vs->slots[idx];
    if (slot_is(s, w->ns, key) && s->dirty) {
        w->indexed++;
        return;
    }

    slot_claim(vs, s, w->ns, key, hash);
    s->tombstone = 0;
    snprintf(s->value, VS_VAL_MAX, "%s", value);
    s->dirty = 0;
    w->indexed++;
}

static int is_other_base(const vs_walk_t *w, const char *path) {
    for (int ns = 0; ns < vs_ns_count; ns++) {
        if (ns == (int) w->ns || !w->dirs->base[ns]) continue;
        if (strcmp(w->dirs->base[ns], path) == 0) return 1;
    }

    return 0;
}

static void walk_dir(vs_walk_t *w, const size_t path_len, const size_t key_len, const int depth) {
    if (depth > VS_MAX_WALK_DEPTH) return;

    DIR *d = opendir(w->path);
    if (!d) return;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *name = ent->d_name;
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) continue;

        const size_t nlen = strlen(name);
        if (path_len + 1 + nlen >= sizeof(w->path) || key_len + (key_len ? 1 : 0) + nlen >= sizeof(w->key)) {
            w->skipped++;
            continue;
        }

        w->path[path_len] = '/';
        memcpy(w->path + path_len + 1, name, nlen + 1);
        const size_t child_path_len = path_len + 1 + nlen;

        size_t child_key_len = key_len;
        if (key_len) w->key[child_key_len++] = '/';
        memcpy(w->key + child_key_len, name, nlen + 1);
        child_key_len += nlen;

        struct stat st;
        if (lstat(w->path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (!is_other_base(w, w->path)) walk_dir(w, child_path_len, child_key_len, depth + 1);
        } else if (S_ISREG(st.st_mode)) {
            char value[VS_VAL_MAX];
            if (!vs_valid_key(w->key) || read_first_line(w->path, value) != 0) {
                w->skipped++;
                continue;
            }
            build_insert(w, w->key, value);
        }
    }

    closedir(d);
    w->path[path_len] = '\0';
    w->key[key_len] = '\0';
}

static void vs_prune(const var_store_t *vs, const var_dirs_t *dirs, const int *ns_live) {
    const uint32_t cap = vs->hdr->capacity;

    for (uint32_t i = 0; i < cap; i++) {
        var_slot_t *s = &vs->slots[i];
        if (!s->occupied || s->tombstone || s->dirty) continue;
        if (s->ns >= vs_ns_count || !ns_live[s->ns]) continue;

        char key[VS_KEY_MAX], path[PATH_MAX];
        vs_copy_out(key, sizeof(key), s->key, VS_KEY_MAX);
        if (slot_path(dirs, (var_ns_t) s->ns, key, path) != 0) continue;

        if (access(path, F_OK) != 0 && errno == ENOENT) {
            s->tombstone = 1;
            if (vs->hdr->count > 0) vs->hdr->count--;
        }
    }
}

int vs_build(var_store_t *vs, const var_dirs_t *dirs) {
    if (vs == NULL || dirs == NULL) return vs_err_inval;
    if (!vs->writable) return vs_err_inval;

    if (flock(vs->fd, LOCK_EX) != 0) return vs_err_lock;

    vs_walk_t w = {.vs = vs, .dirs = dirs};
    int ns_live[vs_ns_count] = {0};

    for (int ns = 0; ns < vs_ns_count; ns++) {
        if (!dirs->base[ns]) continue;

        struct stat st;
        if (stat(dirs->base[ns], &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        const int n = snprintf(w.path, sizeof(w.path), "%s", dirs->base[ns]);
        if (n < 0 || (size_t) n >= sizeof(w.path)) continue;

        ns_live[ns] = 1;
        w.ns = (var_ns_t) ns;
        w.key[0] = '\0';
        walk_dir(&w, (size_t) n, 0, 0);
    }

    vs_prune(vs, dirs, ns_live);

    vs->hdr->built_unix = (uint32_t) time(NULL);
    flock(vs->fd, LOCK_UN);

    return w.skipped > 0 ? -w.skipped : w.indexed;
}

int vs_flush(const var_store_t *vs, const var_dirs_t *dirs) {
    if (vs == NULL || dirs == NULL) return vs_err_inval;
    if (!vs->writable) return vs_err_inval;

    if (flock(vs->fd, LOCK_EX) != 0) return vs_err_lock;

    int failed = 0;
    const uint32_t cap = vs->hdr->capacity;

    for (uint32_t i = 0; i < cap; i++) {
        var_slot_t *s = &vs->slots[i];
        if (!s->occupied || s->tombstone || !s->dirty) continue;

        char key[VS_KEY_MAX], value[VS_VAL_MAX];
        char path[PATH_MAX], dir[PATH_MAX];

        vs_copy_out(key, sizeof(key), s->key, VS_KEY_MAX);
        vs_copy_out(value, sizeof(value), s->value, VS_VAL_MAX);

        if (slot_path(dirs, (var_ns_t) s->ns, key, path) != 0) {
            failed++;
            continue;
        }

        struct stat dst;
        if (parent_dir_of(path, dir) != 0 || stat(dir, &dst) != 0 || !S_ISDIR(dst.st_mode)) {
            failed++;
            continue;
        }

        if (atomic_write_file(path, value) != 0) {
            failed++;
            continue;
        }

        s->dirty = 0;
    }

    flock(vs->fd, LOCK_UN);
    return failed;
}

static const char *env_or(const char *name, const char *fallback) {
    const char *v = getenv(name);
    return v && v[0] ? v : fallback;
}

const char *vs_cache_path(void) {
    static char buf[PATH_MAX];
    static int done = 0;

    if (!done) {
        snprintf(buf, sizeof(buf), "%s/varstore.bin", env_or("MUOS_RUN_DIR", "/run/muos"));
        done = 1;
    }

    return buf;
}

static const char *trim_slash(const char *dir, char *buf) {
    size_t len = strlen(dir);
    while (len > 1 && dir[len - 1] == '/')
        len--;

    if (len + 1 > MAX_LEN_SZ) return dir;

    memcpy(buf, dir, len);
    buf[len] = '\0';
    return buf;
}

void vs_default_dirs(var_dirs_t *dirs) {
    if (dirs == NULL) return;

    static char trimmed[vs_ns_count][PATH_MAX];

    dirs->base[vs_ns_global] = env_or("MUOS_CONF_GLOBAL", "/opt/muos/config");
    dirs->base[vs_ns_device] = env_or("MUOS_CONF_DEVICE", "/opt/muos/device/config");
    dirs->base[vs_ns_kiosk] = env_or("MUOS_CONF_KIOSK", "/opt/muos/kiosk");
    dirs->base[vs_ns_system] = env_or("MUOS_CONF_SYSTEM", "/opt/muos/config/system");

    for (int ns = 0; ns < vs_ns_count; ns++)
        dirs->base[ns] = trim_slash(dirs->base[ns], trimmed[ns]);
}

void vs_invalidate_path(const char *path) {
    if (path == NULL || path[0] == '\0') return;

    var_dirs_t dirs;
    vs_default_dirs(&dirs);

    int best = -1;
    size_t best_len = 0;

    for (int ns = 0; ns < vs_ns_count; ns++) {
        if (!dirs.base[ns]) continue;

        const size_t len = strlen(dirs.base[ns]);
        if (len <= best_len) continue;
        if (strncmp(path, dirs.base[ns], len) != 0 || path[len] != '/') continue;

        best = ns;
        best_len = len;
    }

    if (best < 0) return;

    var_store_t vs;
    if (vs_open(&vs, vs_cache_path(), 0, 0, 1) != vs_ok) return;

    vs_del(&vs, (var_ns_t) best, path + best_len + 1);
    vs_close(&vs);
}
