#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <getopt.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

#include <common/base/totp.h>
#include <common/content/lookup.h>

#define MUWEB_VERSION "0.1.0"

#define MUWEB_MAX_CONNECTIONS 24
#define MUWEB_HEADER_LIMIT    (16 * 1024)
#define MUWEB_UPLOAD_LIMIT    (32 * 1024 * 1024)
#define MUWEB_IDLE_SECONDS    30
#define MUWEB_SEND_CHUNK      (64 * 1024)
#define MUWEB_SCAN_DEPTH      8

#define MUWEB_CONTENT_SECONDS 60
#define MUWEB_CONTENT_DEPTH   6

#define MUWEB_CONTENT_ROOTS 4

static const char *const catalogue_types[] = {
    "box",   "grid",         "preview",         "text",           "splash",        "manual",
    "video", "overlay/base", "overlay/battery", "overlay/bright", "overlay/volume"
};
#define CATALOGUE_TYPE_COUNT (sizeof(catalogue_types) / sizeof(catalogue_types[0]))

void *mux_malloc(const size_t size) {
    void *memory = malloc(size);
    if (!memory && size) {
        fprintf(stderr, "muweb: out of memory (%zu bytes)\n", size);
        abort();
    }
    return memory;
}

static volatile sig_atomic_t running = 1;

static char web_root[PATH_MAX];
static char catalogue_root[PATH_MAX];
static char pickles_root[PATH_MAX];
static char info_root[PATH_MAX];
static char content_roots[MUWEB_CONTENT_ROOTS][PATH_MAX];
static size_t content_root_count = 0;
static int read_only = 0;
static int verbose = 0;

static unsigned char code_secret[TOTP_SECRET_SIZE];
static int code_required = 0;

#define MUWEB_SESSION_SLOTS 4
#define MUWEB_SESSION_IDLE  1800
#define MUWEB_TOKEN_BYTES   16
#define MUWEB_TOKEN_TEXT    (MUWEB_TOKEN_BYTES * 2 + 1)

struct session {
    char token[MUWEB_TOKEN_TEXT];
    time_t expires;
};

static struct session sessions[MUWEB_SESSION_SLOTS];

static void signal_handler(const int signal_number) {
    (void) signal_number;
    running = 0;
}

static void log_verbose(const char *format, ...) {
    if (!verbose) return;
    va_list arguments;
    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    fputc('\n', stderr);
}

struct buffer {
    char *data;
    size_t length;
    size_t capacity;
};

static int buffer_reserve(struct buffer *buffer, const size_t extra) {
    if (buffer->length + extra + 1 <= buffer->capacity) return 1;

    size_t capacity = buffer->capacity ? buffer->capacity : 1024;
    while (capacity < buffer->length + extra + 1) {
        if (capacity > SIZE_MAX / 2) return 0;
        capacity *= 2;
    }

    char *data = realloc(buffer->data, capacity);
    if (!data) return 0;

    buffer->data = data;
    buffer->capacity = capacity;
    return 1;
}

static int buffer_append(struct buffer *buffer, const char *text, const size_t length) {
    if (!buffer_reserve(buffer, length)) return 0;
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return 1;
}

static int buffer_puts(struct buffer *buffer, const char *text) {
    return buffer_append(buffer, text, strlen(text));
}

static int buffer_printf(struct buffer *buffer, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    char stack[512];
    const int needed = vsnprintf(stack, sizeof(stack), format, arguments);
    va_end(arguments);

    if (needed < 0) return 0;
    if ((size_t) needed < sizeof(stack)) return buffer_append(buffer, stack, (size_t) needed);

    if (!buffer_reserve(buffer, (size_t) needed)) return 0;
    va_start(arguments, format);
    vsnprintf(buffer->data + buffer->length, (size_t) needed + 1, format, arguments);
    va_end(arguments);
    buffer->length += (size_t) needed;
    return 1;
}

static void buffer_free(struct buffer *buffer) {
    free(buffer->data);
    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
}

#define MUWEB_CACHE_SECONDS 15

struct cached {
    struct buffer body;
    time_t made;
};

static struct cached catalogue_cache;
static struct cached pickles_cache;
static struct cached content_cache;

static struct cached folder_cache;
static char folder_cached_name[PATH_MAX];

static int cache_fresh(const struct cached *cache) {
    return cache->body.length > 0 && time(NULL) - cache->made < MUWEB_CACHE_SECONDS;
}

static void cache_store(struct cached *cache, const struct buffer *body) {
    cache->body.length = 0;
    if (buffer_append(&cache->body, body->data ? body->data : "", body->length))
        cache->made = time(NULL);
    else
        cache->body.length = 0;
}

static void cache_drop(void) {
    catalogue_cache.body.length = 0;
    pickles_cache.body.length = 0;
    content_cache.body.length = 0;
    folder_cache.body.length = 0;
    folder_cached_name[0] = '\0';
}

static int json_string(struct buffer *buffer, const char *value) {
    if (!buffer_puts(buffer, "\"")) return 0;
    for (const unsigned char *p = (const unsigned char *) (value ? value : ""); *p; ++p) {
        switch (*p) {
            case '"':
                if (!buffer_puts(buffer, "\\\"")) return 0;
                break;
            case '\\':
                if (!buffer_puts(buffer, "\\\\")) return 0;
                break;
            case '\n':
                if (!buffer_puts(buffer, "\\n")) return 0;
                break;
            case '\r':
                if (!buffer_puts(buffer, "\\r")) return 0;
                break;
            case '\t':
                if (!buffer_puts(buffer, "\\t")) return 0;
                break;
            default:
                if (*p < 0x20) {
                    if (!buffer_printf(buffer, "\\u%04x", *p)) return 0;
                } else if (!buffer_append(buffer, (const char *) p, 1)) {
                    return 0;
                }
                break;
        }
    }
    return buffer_puts(buffer, "\"");
}

static int json_field(struct buffer *buffer, const char *key, const char *value) {
    return json_string(buffer, key) && buffer_puts(buffer, ":") && json_string(buffer, value);
}

static int json_number(struct buffer *buffer, const char *key, const long long value) {
    return json_string(buffer, key) && buffer_printf(buffer, ":%lld", value);
}

static int space_json(struct buffer *out, const char *key, const char *path) {
    struct statvfs info;
    if (!path || !path[0] || statvfs(path, &info) < 0) return 1;

    const unsigned long long unit = info.f_frsize ? info.f_frsize : info.f_bsize;
    const unsigned long long total = (unsigned long long) info.f_blocks * unit;
    const unsigned long long free_space = (unsigned long long) info.f_bavail * unit;

    const unsigned long long used = ((unsigned long long) info.f_blocks - info.f_bfree) * unit;

    return buffer_printf(out, "\"%s\":{\"total\":%llu,\"free\":%llu,\"used\":%llu},", key, total, free_space, used);
}

static int string_array_add(char ***items, size_t *count, size_t *capacity, const char *value) {
    if (*count == *capacity) {
        const size_t next = *capacity ? *capacity * 2 : 64;
        char **grown = realloc(*items, next * sizeof(char *));
        if (!grown) return 0;
        *items = grown;
        *capacity = next;
    }

    char *copy = strdup(value);
    if (!copy) return 0;

    (*items)[(*count)++] = copy;
    return 1;
}

static void string_array_free(char **items, const size_t count) {
    for (size_t i = 0; i < count; ++i)
        free(items[i]);
    free(items);
}

static int string_compare(const void *left, const void *right) {
    return strcmp(*(const char *const *) left, *(const char *const *) right);
}

static size_t string_array_dedupe(char **items, const size_t count) {
    if (count < 2) return count;

    qsort(items, count, sizeof(char *), string_compare);

    size_t unique = 1;
    for (size_t i = 1; i < count; ++i) {
        if (strcmp(items[i], items[unique - 1]) == 0) {
            free(items[i]);
            continue;
        }
        items[unique++] = items[i];
    }
    return unique;
}

static void strip_extension(const char *name) {
    char *dot = strrchr(name, '.');
    if (dot && dot != name) *dot = '\0';
}

static const char *file_extension(const char *name) {
    const char *dot = strrchr(name, '.');
    return dot && dot != name ? dot + 1 : "";
}

static int url_decode(const char *source, char *out, const size_t out_size) {
    size_t written = 0;
    for (const char *p = source; *p; ++p) {
        if (written + 1 >= out_size) return 0;

        if (*p == '%') {
            if (!isxdigit((unsigned char) p[1]) || !isxdigit((unsigned char) p[2])) return 0;
            const char hex[3] = {p[1], p[2], '\0'};
            const long value = strtol(hex, NULL, 16);
            if (value == 0) return 0;
            out[written++] = (char) value;
            p += 2;
        } else {
            out[written++] = *p;
        }
    }

    out[written] = '\0';
    return 1;
}

static int safe_relative_path(const char *relative) {
    if (!relative || !*relative || *relative == '/') return 0;
    if (strlen(relative) >= PATH_MAX - 1) return 0;

    const char *segment = relative;
    while (*segment) {
        const char *end = strchr(segment, '/');
        const size_t length = end ? (size_t) (end - segment) : strlen(segment);

        if (length == 0 || segment[0] == '.') return 0;
        for (size_t i = 0; i < length; ++i) {
            if (segment[i] == '\\' || (unsigned char) segment[i] < 0x20) return 0;
        }

        if (!end) break;
        segment = end + 1;
    }
    return 1;
}

static int join_path(char *out, const size_t out_size, const char *base, const char *relative) {
    const int written =
        relative && *relative ? snprintf(out, out_size, "%s/%s", base, relative) : snprintf(out, out_size, "%s", base);
    return written > 0 && (size_t) written < out_size;
}

static int resolve_within(const char *root, const char *relative, char *out, const size_t out_size) {
    if (relative && *relative && !safe_relative_path(relative)) return 0;

    char candidate[PATH_MAX];
    if (!join_path(candidate, sizeof(candidate), root, relative)) return 0;

    char resolved[PATH_MAX];
    if (!realpath(candidate, resolved)) return 0;

    char resolved_root[PATH_MAX];
    if (!realpath(root, resolved_root)) return 0;

    const size_t root_length = strlen(resolved_root);
    if (strncmp(resolved, resolved_root, root_length) != 0) return 0;
    if (resolved[root_length] != '\0' && resolved[root_length] != '/') return 0;

    const int written = snprintf(out, out_size, "%s", resolved);
    return written > 0 && (size_t) written < out_size;
}

static int make_directories(const char *path) {
    char working[PATH_MAX];
    const int written = snprintf(working, sizeof(working), "%s", path);
    if (written < 0 || (size_t) written >= sizeof(working)) return 0;

    for (char *p = working + 1; *p; ++p) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(working, 0755) < 0 && errno != EEXIST) return 0;
        *p = '/';
    }
    return mkdir(working, 0755) == 0 || errno == EEXIST;
}

static int
resolve_target(const char *root, const char *relative, const int create_parents, char *out, const size_t out_size) {
    if (!safe_relative_path(relative)) return 0;

    char parent_relative[PATH_MAX];
    const char *leaf = strrchr(relative, '/');

    if (leaf) {
        const size_t parent_length = (size_t) (leaf - relative);
        if (parent_length >= sizeof(parent_relative)) return 0;
        memcpy(parent_relative, relative, parent_length);
        parent_relative[parent_length] = '\0';
        leaf += 1;
    } else {
        parent_relative[0] = '\0';
        leaf = relative;
    }

    if (create_parents && parent_relative[0]) {
        char parent_path[PATH_MAX];
        if (!join_path(parent_path, sizeof(parent_path), root, parent_relative)) return 0;
        if (!make_directories(parent_path)) return 0;
    }

    char parent_resolved[PATH_MAX];
    if (!resolve_within(root, parent_relative[0] ? parent_relative : NULL, parent_resolved, sizeof(parent_resolved)))
        return 0;

    const int written = snprintf(out, out_size, "%s/%s", parent_resolved, leaf);
    return written > 0 && (size_t) written < out_size;
}

static void drop_checksum(const char *path) {
    char sum[PATH_MAX];
    if ((size_t) snprintf(sum, sizeof(sum), "%s.sum", path) < sizeof(sum)) unlink(sum);
}

static int write_file_atomic(const char *path, const char *data, const size_t length) {
    char temporary[PATH_MAX];
    const int written = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (written < 0 || (size_t) written >= sizeof(temporary)) return 0;

    const int descriptor = open(temporary, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0644);
    if (descriptor < 0) return 0;

    size_t offset = 0;
    while (offset < length) {
        const ssize_t chunk = write(descriptor, data + offset, length - offset);
        if (chunk <= 0) {
            if (errno == EINTR) continue;
            close(descriptor);
            unlink(temporary);
            return 0;
        }
        offset += (size_t) chunk;
    }

    if (fsync(descriptor) < 0 || close(descriptor) < 0) {
        unlink(temporary);
        return 0;
    }

    if (rename(temporary, path) < 0) {
        unlink(temporary);
        return 0;
    }
    return 1;
}

static int read_whole_file(const char *path, struct buffer *out) {
    const int descriptor = open(path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) return 0;

    char chunk[65536];
    for (;;) {
        const ssize_t got = read(descriptor, chunk, sizeof(chunk));
        if (got == 0) break;
        if (got < 0) {
            if (errno == EINTR) continue;
            close(descriptor);
            return 0;
        }
        if (out->length + (size_t) got > MUWEB_UPLOAD_LIMIT || !buffer_append(out, chunk, (size_t) got)) {
            close(descriptor);
            return 0;
        }
    }

    close(descriptor);
    return 1;
}

static const char *mime_for(const char *name) {
    static const struct {
        const char *extension;
        const char *type;
    } table[] = {
        {"html", "text/html; charset=utf-8"},
        {"css", "text/css; charset=utf-8"},
        {"js", "text/javascript; charset=utf-8"},
        {"json", "application/json"},
        {"svg", "image/svg+xml"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"webp", "image/webp"},
        {"qoi", "image/qoi"},
        {"tga", "image/x-targa"},
        {"gif", "image/gif"},
        {"bmp", "image/bmp"},
        {"pcx", "image/x-pcx"},
        {"txt", "text/plain; charset=utf-8"},
        {"pdf", "application/pdf"},
        {"mp4", "video/mp4"},
        {"webm", "video/webm"},
        {"ini", "text/plain; charset=utf-8"},
        {"state", "application/octet-stream"},
        {"srm", "application/octet-stream"}
    };

    const char *extension = file_extension(name);
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i) {
        if (strcasecmp(extension, table[i].extension) == 0) return table[i].type;
    }
    return "application/octet-stream";
}

static int upload_extension_allowed(const char *type, const char *extension) {
    static const char *const images[] = {"svg", "png", "jpg", "jpeg", "webp", "qoi", "tga", "gif", "bmp", "pcx", NULL};
    static const char *const documents[] = {"txt", NULL};
    static const char *const videos[] = {"mp4", NULL};

    const char *const *allowed = images;
    if (strcmp(type, "text") == 0 || strcmp(type, "manual") == 0)
        allowed = documents;
    else if (strcmp(type, "video") == 0)
        allowed = videos;

    for (size_t i = 0; allowed[i]; ++i) {
        if (strcasecmp(extension, allowed[i]) == 0) return 1;
    }
    return 0;
}

struct content_item {
    char stem[NAME_MAX + 1];
    char catalogue[NAME_MAX + 1];
    int folder;
};

struct content_folder {
    char name[NAME_MAX + 1];
    char catalogue[NAME_MAX + 1];
    struct content_item *items;
    size_t count;
    size_t capacity;
};

static struct content_folder *content_folders = NULL;
static size_t content_folder_count = 0;
static size_t content_folder_capacity = 0;
static time_t content_made = 0;

struct assign_entry {
    char key[NAME_MAX + 1];
    char catalogue[NAME_MAX + 1];
};

static struct assign_entry *assign_entries = NULL;
static size_t assign_count = 0;

static char **skip_patterns = NULL;
static size_t skip_pattern_count = 0;
static size_t skip_pattern_capacity = 0;

static void lowercase(char *text) {
    for (; *text; ++text)
        *text = (char) tolower((unsigned char) *text);
}

static void assign_key(const char *name, char *out, const size_t out_size) {
    size_t written = 0;

    for (const char *read = name; *read && written + 1 < out_size; ++read) {
        if (*read == ' ' || *read == '-' || *read == '_' || *read == '+') continue;
        out[written++] = (char) tolower((unsigned char) *read);
    }

    out[written] = '\0';
}

static const char *json_scan_string(const char *cursor, char *out, const size_t out_size) {
    while (*cursor && *cursor != '"') {
        if (*cursor == '}' || *cursor == ']') return NULL;
        ++cursor;
    }
    if (*cursor != '"') return NULL;
    ++cursor;

    size_t written = 0;
    while (*cursor && *cursor != '"') {
        char character = *cursor++;

        if (character == '\\' && *cursor) {
            const char escape = *cursor++;
            switch (escape) {
                case 'n':
                    character = '\n';
                    break;
                case 't':
                    character = '\t';
                    break;
                case 'r':
                    character = '\r';
                    break;
                case 'b':
                    character = '\b';
                    break;
                case 'f':
                    character = '\f';
                    break;
                case 'u': {
                    unsigned value = 0;
                    for (int digit = 0; digit < 4 && isxdigit((unsigned char) *cursor); ++digit) {
                        const char hex = *cursor++;
                        value = value * 16
                                + (unsigned) (isdigit((unsigned char) hex) ? hex - '0'
                                                                           : tolower((unsigned char) hex) - 'a' + 10);
                    }
                    character = value && value < 0x80 ? (char) value : '?';
                    break;
                }
                default:
                    character = escape;
                    break;
            }
        }

        if (written + 1 < out_size) out[written++] = character;
    }

    if (*cursor != '"') return NULL;
    out[written] = '\0';
    return cursor + 1;
}

static void assign_free(void) {
    free(assign_entries);
    assign_entries = NULL;
    assign_count = 0;
}

static void assign_load(void) {
    assign_free();
    if (!info_root[0]) return;

    char path[PATH_MAX];
    if ((size_t) snprintf(path, sizeof(path), "%s/assign/assign.json", info_root) >= sizeof(path)) return;

    struct buffer text = {0};
    if (!read_whole_file(path, &text) || !text.data) {
        buffer_free(&text);
        return;
    }

    const char *cursor = strchr(text.data, '{');
    size_t capacity = 0;

    while (cursor) {
        char key[NAME_MAX + 1];
        char value[NAME_MAX + 1];

        cursor = json_scan_string(cursor, key, sizeof(key));
        if (!cursor) break;
        cursor = json_scan_string(cursor, value, sizeof(value));
        if (!cursor) break;
        if (!key[0] || !value[0]) continue;

        if (assign_count == capacity) {
            const size_t next = capacity ? capacity * 2 : 32;
            struct assign_entry *grown = realloc(assign_entries, next * sizeof(*grown));
            if (!grown) break;
            assign_entries = grown;
            capacity = next;
        }

        assign_key(key, assign_entries[assign_count].key, sizeof(assign_entries[assign_count].key));
        snprintf(assign_entries[assign_count].catalogue, sizeof(assign_entries[assign_count].catalogue), "%s", value);
        assign_count += 1;
    }

    buffer_free(&text);
    log_verbose("assign.json: %zu directory names mapped", assign_count);
}

static const char *assign_lookup(const char *directory_name) {
    char key[NAME_MAX + 1];
    assign_key(directory_name, key, sizeof(key));

    for (size_t i = 0; i < assign_count; ++i)
        if (strcmp(assign_entries[i].key, key) == 0) return assign_entries[i].catalogue;

    return NULL;
}

static void skip_free(void) {
    string_array_free(skip_patterns, skip_pattern_count);
    skip_patterns = NULL;
    skip_pattern_count = 0;
    skip_pattern_capacity = 0;
}

static void skip_load(void) {
    skip_free();
    if (!info_root[0]) return;

    char path[PATH_MAX];
    if ((size_t) snprintf(path, sizeof(path), "%s/skip.ini", info_root) >= sizeof(path)) return;

    struct buffer text = {0};
    if (!read_whole_file(path, &text) || !text.data) {
        buffer_free(&text);
        return;
    }

    for (char *line = text.data, *next; line && *line; line = next) {
        next = strchr(line, '\n');
        if (next) *next++ = '\0';

        size_t length = strlen(line);
        while (length && (line[length - 1] == '\r' || line[length - 1] == ' '))
            line[--length] = '\0';
        if (!length || line[0] == '#') continue;

        lowercase(line);
        string_array_add(&skip_patterns, &skip_pattern_count, &skip_pattern_capacity, line);
    }

    buffer_free(&text);
    log_verbose("skip.ini: %zu patterns", skip_pattern_count);
}

static int should_skip(const char *name, const int is_directory) {
    char folded[NAME_MAX + 1];
    snprintf(folded, sizeof(folded), "%s", name);
    lowercase(folded);

    for (size_t i = 0; i < skip_pattern_count; ++i) {
        const char *pattern = skip_patterns[i];
        int directory_only = 0;

        if (*pattern == '/') {
            directory_only = 1;
            ++pattern;
            if (!*pattern) continue;
        }

        if (directory_only && !is_directory) continue;
        if (fnmatch(pattern, folded, 0) == 0) return 1;
    }

    return 0;
}

#define CATALOGUE_FOLDER "Folder"

static void content_free(void) {
    for (size_t i = 0; i < content_folder_count; ++i)
        free(content_folders[i].items);

    free(content_folders);
    content_folders = NULL;
    content_folder_count = 0;
    content_folder_capacity = 0;
    content_made = 0;
}

static struct content_folder *content_folder_for(const char *name) {
    for (size_t i = 0; i < content_folder_count; ++i)
        if (strcmp(content_folders[i].name, name) == 0) return &content_folders[i];

    if (content_folder_count == content_folder_capacity) {
        const size_t next = content_folder_capacity ? content_folder_capacity * 2 : 16;
        struct content_folder *grown = realloc(content_folders, next * sizeof(*grown));
        if (!grown) return NULL;
        content_folders = grown;
        content_folder_capacity = next;
    }

    struct content_folder *folder = &content_folders[content_folder_count];
    memset(folder, 0, sizeof(*folder));
    snprintf(folder->name, sizeof(folder->name), "%s", name);
    content_folder_count += 1;
    return folder;
}

static int
content_item_add(struct content_folder *folder, const char *stem, const char *catalogue, const int is_folder) {
    for (size_t i = 0; i < folder->count; ++i)
        if (folder->items[i].folder == is_folder && strcmp(folder->items[i].stem, stem) == 0) return 1;

    if (folder->count == folder->capacity) {
        const size_t next = folder->capacity ? folder->capacity * 2 : 32;
        struct content_item *grown = realloc(folder->items, next * sizeof(*grown));
        if (!grown) return 0;
        folder->items = grown;
        folder->capacity = next;
    }

    struct content_item *item = &folder->items[folder->count];
    memset(item, 0, sizeof(*item));
    snprintf(item->stem, sizeof(item->stem), "%s", stem);
    snprintf(item->catalogue, sizeof(item->catalogue), "%s", catalogue ? catalogue : "");
    item->folder = is_folder;
    folder->count += 1;
    return 1;
}

static void content_walk(
    const char *root, const char *path, struct content_folder *folder, const char *catalogue, const int depth
) {
    if (depth > MUWEB_CONTENT_DEPTH) return;

    DIR *directory = opendir(path);
    if (!directory) return;

    const struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;

        char child[PATH_MAX];
        if ((size_t) snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) >= sizeof(child)) continue;

        int is_directory = entry->d_type == DT_DIR;
        if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK) {
            struct stat info;
            if (stat(child, &info) < 0) continue;
            is_directory = S_ISDIR(info.st_mode);
        }

        if (should_skip(entry->d_name, is_directory)) continue;

        if (is_directory) {
            if (entry->d_type == DT_LNK) {
                char resolved[PATH_MAX];
                const size_t root_length = strlen(root);

                if (!realpath(child, resolved)) continue;
                if (strncmp(resolved, root, root_length) != 0
                    || (resolved[root_length] && resolved[root_length] != '/'))
                    continue;
            }

            content_item_add(folder, entry->d_name, CATALOGUE_FOLDER, 1);

            const char *assigned = assign_lookup(entry->d_name);
            content_walk(root, child, folder, assigned ? assigned : catalogue, depth + 1);
            continue;
        }

        char stem[NAME_MAX + 1];
        snprintf(stem, sizeof(stem), "%s", entry->d_name);
        strip_extension(stem);
        if (stem[0]) content_item_add(folder, stem, catalogue, 0);
    }

    closedir(directory);
}

static int content_item_compare(const void *left, const void *right) {
    const struct content_item *a = left;
    const struct content_item *b = right;

    if (a->folder != b->folder) return b->folder - a->folder;
    return strcasecmp(a->stem, b->stem);
}

static void content_index_build(void) {
    if (!content_root_count) return;
    if (content_made && time(NULL) - content_made < MUWEB_CONTENT_SECONDS) return;

    content_free();
    assign_load();
    skip_load();

    for (size_t root = 0; root < content_root_count; ++root) {
        DIR *directory = opendir(content_roots[root]);
        if (!directory) continue;

        const struct dirent *entry;
        while ((entry = readdir(directory))) {
            if (entry->d_name[0] == '.') continue;

            char child[PATH_MAX];
            if ((size_t) snprintf(child, sizeof(child), "%s/%s", content_roots[root], entry->d_name) >= sizeof(child))
                continue;

            int is_directory = entry->d_type == DT_DIR;
            if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK) {
                struct stat info;
                if (stat(child, &info) < 0) continue;
                is_directory = S_ISDIR(info.st_mode);
            }

            if (!is_directory || should_skip(entry->d_name, 1)) continue;

            if (entry->d_type == DT_LNK) {
                char resolved[PATH_MAX];
                const size_t root_length = strlen(content_roots[root]);

                if (!realpath(child, resolved)) continue;
                if (strncmp(resolved, content_roots[root], root_length) != 0
                    || (resolved[root_length] && resolved[root_length] != '/'))
                    continue;
            }

            struct content_folder *folder = content_folder_for(entry->d_name);
            if (!folder) continue;

            const char *assigned = assign_lookup(entry->d_name);
            if (assigned && !folder->catalogue[0])
                snprintf(folder->catalogue, sizeof(folder->catalogue), "%s", assigned);

            content_walk(content_roots[root], child, folder, assigned, 0);
        }

        closedir(directory);
    }

    size_t total = 0;
    for (size_t i = 0; i < content_folder_count; ++i) {
        qsort(content_folders[i].items, content_folders[i].count, sizeof(struct content_item), content_item_compare);
        total += content_folders[i].count;
    }

    content_made = time(NULL);
    log_verbose("content: %zu items across %zu folders", total, content_folder_count);
}

static int content_holds(const char *catalogue, const char *stem) {
    content_index_build();

    for (size_t i = 0; i < content_folder_count; ++i)
        for (size_t j = 0; j < content_folders[i].count; ++j)
            if (strcmp(content_folders[i].items[j].catalogue, catalogue) == 0
                && strcmp(content_folders[i].items[j].stem, stem) == 0)
                return 1;

    return 0;
}

static size_t content_count_for(const char *catalogue) {
    size_t count = 0;

    content_index_build();
    for (size_t i = 0; i < content_folder_count; ++i)
        for (size_t j = 0; j < content_folders[i].count; ++j)
            if (strcmp(content_folders[i].items[j].catalogue, catalogue) == 0) count += 1;

    return count;
}

struct catalogue_file {
    char relative[PATH_MAX];
    char stem[NAME_MAX + 1];
    long long bytes;
    long long modified;
};

struct catalogue_scan {
    struct catalogue_file *files;
    size_t count;
    size_t capacity;
    long long bytes;
};

static int
catalogue_scan_add(struct catalogue_scan *scan, const char *relative, const char *name, const struct stat *info) {
    if (scan->count == scan->capacity) {
        const size_t next = scan->capacity ? scan->capacity * 2 : 64;
        struct catalogue_file *grown = realloc(scan->files, next * sizeof(struct catalogue_file));
        if (!grown) return 0;
        scan->files = grown;
        scan->capacity = next;
    }

    struct catalogue_file *file = &scan->files[scan->count];
    if ((size_t) snprintf(file->relative, sizeof(file->relative), "%s", relative) >= sizeof(file->relative)) return 0;
    if ((size_t) snprintf(file->stem, sizeof(file->stem), "%s", name) >= sizeof(file->stem)) return 0;

    strip_extension(file->stem);
    file->bytes = (long long) info->st_size;
    file->modified = (long long) info->st_mtime;

    scan->bytes += file->bytes;
    scan->count += 1;
    return 1;
}

static void catalogue_scan_type(const char *type_path, struct catalogue_scan *scan, const int want_sizes) {
    static const struct stat unsized;

    DIR *directory = opendir(type_path);
    if (!directory) return;

    const struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;

        char child[PATH_MAX];
        if (!join_path(child, sizeof(child), type_path, entry->d_name)) continue;

        struct stat info;
        int kind = entry->d_type;

        if (kind == DT_UNKNOWN || want_sizes) {
            if (stat(child, &info) < 0) continue;
            kind = S_ISDIR(info.st_mode) ? DT_DIR : S_ISREG(info.st_mode) ? DT_REG : DT_UNKNOWN;
        }

        if (kind == DT_REG) {
            catalogue_scan_add(scan, entry->d_name, entry->d_name, want_sizes ? &info : &unsized);
            continue;
        }
        if (kind != DT_DIR) continue;

        DIR *nested = opendir(child);
        if (!nested) continue;

        const struct dirent *nested_entry;
        while ((nested_entry = readdir(nested))) {
            if (nested_entry->d_name[0] == '.') continue;

            struct stat nested_info;
            int nested_kind = nested_entry->d_type;

            if (nested_kind == DT_UNKNOWN || want_sizes) {
                char nested_path[PATH_MAX];
                if (!join_path(nested_path, sizeof(nested_path), child, nested_entry->d_name)) continue;
                if (stat(nested_path, &nested_info) < 0) continue;
                nested_kind = S_ISREG(nested_info.st_mode) ? DT_REG : DT_UNKNOWN;
            }
            if (nested_kind != DT_REG) continue;

            char relative[PATH_MAX];
            if ((size_t) snprintf(relative, sizeof(relative), "%s/%s", entry->d_name, nested_entry->d_name)
                >= sizeof(relative))
                continue;

            catalogue_scan_add(scan, relative, nested_entry->d_name, want_sizes ? &nested_info : &unsized);
        }
        closedir(nested);
    }
    closedir(directory);
}

static void catalogue_scan_free(struct catalogue_scan *scan) {
    free(scan->files);
    scan->files = NULL;
    scan->count = 0;
    scan->capacity = 0;
    scan->bytes = 0;
}

static int catalogue_types_json(struct buffer *out) {
    if (!buffer_puts(out, "\"types\":[")) return 0;
    for (size_t i = 0; i < CATALOGUE_TYPE_COUNT; ++i) {
        if (i && !buffer_puts(out, ",")) return 0;
        if (!json_string(out, catalogue_types[i])) return 0;
    }
    return buffer_puts(out, "]");
}

static int catalogue_index_json(struct buffer *out) {
    DIR *directory = opendir(catalogue_root);
    if (!directory) return 0;

    char **systems = NULL;
    size_t system_count = 0;
    size_t system_capacity = 0;

    const struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;

        char path[PATH_MAX];
        if (!join_path(path, sizeof(path), catalogue_root, entry->d_name)) continue;

        struct stat info;
        if (stat(path, &info) < 0 || !S_ISDIR(info.st_mode)) continue;
        string_array_add(&systems, &system_count, &system_capacity, entry->d_name);
    }
    closedir(directory);

    qsort(systems, system_count, sizeof(char *), string_compare);

    if (!buffer_puts(out, "{") || !space_json(out, "space", catalogue_root) || !catalogue_types_json(out)
        || !buffer_puts(out, ",\"systems\":[")) {
        string_array_free(systems, system_count);
        return 0;
    }

    for (size_t i = 0; i < system_count; ++i) {
        char **stems = NULL;
        size_t stem_count = 0;
        size_t stem_capacity = 0;

        if (i && !buffer_puts(out, ",")) break;
        if (!buffer_puts(out, "{") || !json_field(out, "name", systems[i]) || !buffer_puts(out, ",\"counts\":{")) break;

        for (size_t type = 0; type < CATALOGUE_TYPE_COUNT; ++type) {
            char type_path[PATH_MAX];
            struct catalogue_scan scan = {0};

            if (snprintf(type_path, sizeof(type_path), "%s/%s/%s", catalogue_root, systems[i], catalogue_types[type])
                < (int) sizeof(type_path))
                catalogue_scan_type(type_path, &scan, 0);

            for (size_t file = 0; file < scan.count; ++file)
                string_array_add(&stems, &stem_count, &stem_capacity, scan.files[file].stem);

            if (type && !buffer_puts(out, ",")) break;
            json_number(out, catalogue_types[type], (long long) scan.count);
            catalogue_scan_free(&scan);
        }

        stem_count = string_array_dedupe(stems, stem_count);
        string_array_free(stems, stem_count);

        if (!buffer_puts(out, "},") || !json_number(out, "entries", (long long) stem_count) || !buffer_puts(out, ",")
            || !json_number(out, "content", (long long) content_count_for(systems[i])) || !buffer_puts(out, "}"))
            break;
    }

    string_array_free(systems, system_count);
    return buffer_puts(out, "]}");
}

static int catalogue_system_json(struct buffer *out, const char *system) {
    char system_path[PATH_MAX];
    if (!resolve_within(catalogue_root, system, system_path, sizeof(system_path))) return 0;

    struct catalogue_scan scans[CATALOGUE_TYPE_COUNT] = {0};

    char **stems = NULL;
    size_t stem_count = 0;
    size_t stem_capacity = 0;

    for (size_t type = 0; type < CATALOGUE_TYPE_COUNT; ++type) {
        char type_relative[PATH_MAX];
        char type_path[PATH_MAX];

        if ((size_t) snprintf(type_relative, sizeof(type_relative), "%s/%s", system, catalogue_types[type])
            >= sizeof(type_relative))
            continue;
        if (!resolve_within(catalogue_root, type_relative, type_path, sizeof(type_path))) continue;

        catalogue_scan_type(type_path, &scans[type], 1);
        for (size_t file = 0; file < scans[type].count; ++file)
            string_array_add(&stems, &stem_count, &stem_capacity, scans[type].files[file].stem);
    }

    const size_t held_count = string_array_dedupe(stems, stem_count);
    const int assigned = content_root_count > 0;

    content_index_build();

    stem_count = held_count;
    for (size_t i = 0; i < content_folder_count && assigned; ++i)
        for (size_t j = 0; j < content_folders[i].count; ++j)
            if (strcmp(content_folders[i].items[j].catalogue, system) == 0)
                string_array_add(&stems, &stem_count, &stem_capacity, content_folders[i].items[j].stem);

    stem_count = string_array_dedupe(stems, stem_count);

    int ok = buffer_puts(out, "{") && json_field(out, "name", system) && buffer_puts(out, ",")
             && space_json(out, "space", catalogue_root)
             && buffer_printf(out, "\"assigned\":%s,", assigned ? "true" : "false") && catalogue_types_json(out)
             && buffer_puts(out, ",\"entries\":[");

    for (size_t i = 0; ok && i < stem_count; ++i) {
        long long bytes = 0;
        long long modified = 0;

        if (i && !((ok = buffer_puts(out, ",")))) break;
        ok = buffer_puts(out, "{") && json_field(out, "stem", stems[i]) && buffer_puts(out, ",\"files\":{");

        int first = 1;
        for (size_t type = 0; ok && type < CATALOGUE_TYPE_COUNT; ++type) {
            for (size_t file = 0; file < scans[type].count; ++file) {
                if (strcmp(scans[type].files[file].stem, stems[i]) != 0) continue;

                if (!first && !((ok = buffer_puts(out, ",")))) break;
                ok = json_string(out, catalogue_types[type]) && buffer_puts(out, ":")
                     && json_string(out, scans[type].files[file].relative);

                bytes += scans[type].files[file].bytes;
                if (scans[type].files[file].modified > modified) modified = scans[type].files[file].modified;
                first = 0;
                break;
            }
        }

        const int on_card = !assigned || content_holds(system, stems[i]);

        ok = ok && buffer_puts(out, "},") && json_number(out, "bytes", bytes) && buffer_puts(out, ",")
             && json_number(out, "modified", modified) && (on_card || buffer_puts(out, ",\"orphan\":true"))
             && buffer_puts(out, "}");
    }

    for (size_t type = 0; type < CATALOGUE_TYPE_COUNT; ++type)
        catalogue_scan_free(&scans[type]);
    string_array_free(stems, stem_count);

    return ok && buffer_puts(out, "]}");
}

struct name_entry {
    char key[NAME_MAX + 1];
    char value[256];
};

static struct name_entry *name_entries = NULL;
static size_t name_entry_count = 0;
static char name_source[PATH_MAX];

static struct name_entry *folder_names = NULL;
static size_t folder_name_count = 0;
static int folder_names_loaded = 0;

static size_t name_map_read(const char *path, struct name_entry **into) {
    struct buffer text = {0};
    if (!read_whole_file(path, &text) || !text.data) {
        buffer_free(&text);
        return 0;
    }

    const char *cursor = strchr(text.data, '{');
    size_t count = 0;
    size_t capacity = 0;

    while (cursor) {
        char key[NAME_MAX + 1];
        char value[256];

        cursor = json_scan_string(cursor, key, sizeof(key));
        if (!cursor) break;
        cursor = json_scan_string(cursor, value, sizeof(value));
        if (!cursor) break;
        if (!key[0] || !value[0]) continue;

        if (count == capacity) {
            const size_t next = capacity ? capacity * 2 : 32;
            struct name_entry *grown = realloc(*into, next * sizeof(**into));
            if (!grown) break;
            *into = grown;
            capacity = next;
        }

        snprintf((*into)[count].key, sizeof((*into)[count].key), "%s", key);
        lowercase((*into)[count].key);
        snprintf((*into)[count].value, sizeof((*into)[count].value), "%s", value);
        count += 1;
    }

    buffer_free(&text);
    return count;
}

static const char *name_map_get(const struct name_entry *entries, const size_t count, const char *folded) {
    for (size_t i = 0; i < count; ++i)
        if (strcmp(entries[i].key, folded) == 0) return entries[i].value;

    return NULL;
}

static void names_free(void) {
    free(name_entries);
    name_entries = NULL;
    name_entry_count = 0;
    name_source[0] = '\0';

    free(folder_names);
    folder_names = NULL;
    folder_name_count = 0;
    folder_names_loaded = 0;
}

static void names_load_for(const char *folder) {
    if (!info_root[0]) return;

    enum { root_room = PATH_MAX / 2, name_room = NAME_MAX };

    char path[PATH_MAX];
    if (strlen(info_root) >= root_room || strlen(folder) >= name_room) return;

    snprintf(path, sizeof(path), "%.*s/name/%.*s.json", (int) root_room, info_root, (int) name_room, folder);
    if (access(path, R_OK) != 0) snprintf(path, sizeof(path), "%.*s/name/global.json", (int) root_room, info_root);

    if (strcmp(name_source, path) == 0) return;

    free(name_entries);
    name_entries = NULL;
    name_entry_count = name_map_read(path, &name_entries);
    snprintf(name_source, sizeof(name_source), "%s", path);
}

static const char *friendly_content_name(const char *folder, const char *stem) {
    char folded[NAME_MAX + 1];
    snprintf(folded, sizeof(folded), "%s", stem);
    lowercase(folded);

    names_load_for(folder);

    const char *custom = name_map_get(name_entries, name_entry_count, folded);
    if (custom) return strcmp(custom, stem) == 0 ? NULL : custom;

    const char *known = lookup(stem);
    return known && strcmp(known, stem) != 0 ? known : NULL;
}

static const char *friendly_folder_name(const char *folder) {
    if (!info_root[0]) return NULL;

    if (!folder_names_loaded) {
        char path[PATH_MAX];
        folder_names_loaded = 1;

        if ((size_t) snprintf(path, sizeof(path), "%s/name/folder.json", info_root) < sizeof(path))
            folder_name_count = name_map_read(path, &folder_names);
    }

    char folded[NAME_MAX + 1];
    snprintf(folded, sizeof(folded), "%s", folder);
    lowercase(folded);

    const char *known = name_map_get(folder_names, folder_name_count, folded);
    return known && strcmp(known, folder) != 0 ? known : NULL;
}

struct catalogue_view {
    struct catalogue_scan scans[CATALOGUE_TYPE_COUNT];
    int loaded;
};

static void catalogue_view_load(struct catalogue_view *view, const char *system) {
    memset(view, 0, sizeof(*view));
    if (!system || !*system || !safe_relative_path(system)) return;

    for (size_t type = 0; type < CATALOGUE_TYPE_COUNT; ++type) {
        char relative[PATH_MAX];
        char path[PATH_MAX];

        if ((size_t) snprintf(relative, sizeof(relative), "%s/%s", system, catalogue_types[type]) >= sizeof(relative))
            continue;
        if (!resolve_within(catalogue_root, relative, path, sizeof(path))) continue;

        catalogue_scan_type(path, &view->scans[type], 1);
    }

    view->loaded = 1;
}

static void catalogue_view_free(struct catalogue_view *view) {
    for (size_t type = 0; type < CATALOGUE_TYPE_COUNT; ++type)
        catalogue_scan_free(&view->scans[type]);
    view->loaded = 0;
}

static int catalogue_files_json(
    struct buffer *out, const struct catalogue_view *view, const char *stem, long long *bytes, long long *modified
) {
    int ok = buffer_puts(out, "\"files\":{");
    int first = 1;

    for (size_t type = 0; ok && type < CATALOGUE_TYPE_COUNT; ++type) {
        for (size_t file = 0; file < view->scans[type].count; ++file) {
            if (strcmp(view->scans[type].files[file].stem, stem) != 0) continue;

            if (!first && !((ok = buffer_puts(out, ",")))) break;
            ok = json_string(out, catalogue_types[type]) && buffer_puts(out, ":")
                 && json_string(out, view->scans[type].files[file].relative);

            if (bytes) *bytes += view->scans[type].files[file].bytes;
            if (modified && view->scans[type].files[file].modified > *modified)
                *modified = view->scans[type].files[file].modified;
            first = 0;
            break;
        }
    }

    return ok && buffer_puts(out, "}");
}

static const char *catalogue_for_path(const char *relative) {
    const char *found = NULL;
    const char *segment = relative;

    while (segment && *segment) {
        const char *end = strchr(segment, '/');
        const size_t length = end ? (size_t) (end - segment) : strlen(segment);

        char name[NAME_MAX + 1];
        if (length < sizeof(name)) {
            memcpy(name, segment, length);
            name[length] = '\0';

            const char *assigned = assign_lookup(name);
            if (assigned) found = assigned;
        }

        if (!end) break;
        segment = end + 1;
    }

    return found;
}

static const char *path_leaf(const char *relative) {
    const char *slash = strrchr(relative, '/');
    return slash ? slash + 1 : relative;
}

struct child_list {
    char **names;
    size_t count;
    size_t capacity;
};

static void children_of(const char *relative, struct child_list *directories, struct child_list *files) {
    for (size_t root = 0; root < content_root_count; ++root) {
        char path[PATH_MAX];
        if (!resolve_within(content_roots[root], relative, path, sizeof(path))) continue;

        DIR *directory = opendir(path);
        if (!directory) continue;

        const struct dirent *entry;
        while ((entry = readdir(directory))) {
            if (entry->d_name[0] == '.') continue;

            char child[PATH_MAX];
            if ((size_t) snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) >= sizeof(child)) continue;

            int is_directory = entry->d_type == DT_DIR;
            if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK) {
                struct stat info;
                if (stat(child, &info) < 0) continue;
                is_directory = S_ISDIR(info.st_mode);
            }

            if (should_skip(entry->d_name, is_directory)) continue;

            if (is_directory && entry->d_type == DT_LNK) {
                char resolved[PATH_MAX];
                const size_t root_length = strlen(content_roots[root]);

                if (!realpath(child, resolved)) continue;
                if (strncmp(resolved, content_roots[root], root_length) != 0
                    || (resolved[root_length] && resolved[root_length] != '/'))
                    continue;
            }

            if (is_directory) {
                string_array_add(&directories->names, &directories->count, &directories->capacity, entry->d_name);
                continue;
            }

            char stem[NAME_MAX + 1];
            snprintf(stem, sizeof(stem), "%s", entry->d_name);
            strip_extension(stem);
            if (stem[0]) string_array_add(&files->names, &files->count, &files->capacity, stem);
        }

        closedir(directory);
    }

    directories->count = string_array_dedupe(directories->names, directories->count);
    files->count = string_array_dedupe(files->names, files->count);
}

static void child_list_free(struct child_list *list) {
    string_array_free(list->names, list->count);
    list->names = NULL;
    list->count = 0;
    list->capacity = 0;
}

static int content_index_json(struct buffer *out) {
    content_index_build();

    struct catalogue_view folders;
    catalogue_view_load(&folders, CATALOGUE_FOLDER);

    int ok = buffer_puts(out, "{") && space_json(out, "space", catalogue_root) && catalogue_types_json(out)
             && buffer_printf(out, ",\"folder\":\"%s\",\"folders\":[", CATALOGUE_FOLDER);

    for (size_t i = 0; ok && i < content_folder_count; ++i) {
        const struct content_folder *folder = &content_folders[i];
        long long bytes = 0;
        long long modified = 0;

        if (i && !((ok = buffer_puts(out, ",")))) break;

        ok = buffer_puts(out, "{") && json_field(out, "name", folder->name) && buffer_puts(out, ",");

        const char *friendly = friendly_folder_name(folder->name);
        if (ok && friendly) ok = json_field(out, "friendly", friendly) && buffer_puts(out, ",");
        if (ok && folder->catalogue[0]) ok = json_field(out, "catalogue", folder->catalogue) && buffer_puts(out, ",");

        size_t held = 0;
        for (size_t item = 0; item < folder->count; ++item)
            if (!folder->items[item].folder) held += 1;

        ok = ok && json_number(out, "content", (long long) held) && buffer_puts(out, ",")
             && catalogue_files_json(out, &folders, folder->name, &bytes, &modified) && buffer_puts(out, ",")
             && json_number(out, "bytes", bytes) && buffer_puts(out, ",") && json_number(out, "modified", modified)
             && buffer_puts(out, "}");
    }

    catalogue_view_free(&folders);
    return ok && buffer_puts(out, "]}");
}

#define OVERLAY_STEPS 10

static const char *const overlay_steps[] = {"overlay/battery", "overlay/bright", "overlay/volume"};
#define OVERLAY_STEP_COUNT (sizeof(overlay_steps) / sizeof(overlay_steps[0]))

static int overlay_is_stepped(const char *type) {
    for (size_t i = 0; i < OVERLAY_STEP_COUNT; ++i)
        if (strcmp(overlay_steps[i], type) == 0) return 1;

    return 0;
}

static int overlays_json(struct buffer *out, const struct catalogue_view *view) {
    if (!buffer_puts(out, ",\"overlays\":{")) return 0;

    for (size_t i = 0; i < OVERLAY_STEP_COUNT; ++i) {
        const char *key = overlay_steps[i];
        const char *leaf = strchr(key, '/') + 1;

        size_t index = CATALOGUE_TYPE_COUNT;
        for (size_t type = 0; type < CATALOGUE_TYPE_COUNT; ++type)
            if (strcmp(catalogue_types[type], key) == 0) index = type;

        if (i && !buffer_puts(out, ",")) return 0;
        if (!json_string(out, leaf) || !buffer_puts(out, ":{")) return 0;

        int first = 1;
        for (int step = 0; index < CATALOGUE_TYPE_COUNT && step < OVERLAY_STEPS; ++step) {
            char want[NAME_MAX + 1];
            snprintf(want, sizeof(want), "%s_%d", leaf, step);

            for (size_t file = 0; file < view->scans[index].count; ++file) {
                if (strcmp(view->scans[index].files[file].stem, want) != 0) continue;

                char label[16];
                snprintf(label, sizeof(label), "%d", step);

                if (!first && !buffer_puts(out, ",")) return 0;
                if (!json_field(out, label, view->scans[index].files[file].relative)) return 0;
                first = 0;
                break;
            }
        }

        if (!buffer_puts(out, "}")) return 0;
    }

    return buffer_puts(out, "}");
}

static int content_folder_json(struct buffer *out, const char *relative) {
    struct child_list directories = {0};
    struct child_list files = {0};

    int exists = 0;
    for (size_t root = 0; root < content_root_count && !exists; ++root) {
        char path[PATH_MAX];
        struct stat info;
        if (resolve_within(content_roots[root], relative, path, sizeof(path)) && stat(path, &info) == 0
            && S_ISDIR(info.st_mode))
            exists = 1;
    }
    if (!exists) return 0;

    content_index_build();
    children_of(relative, &directories, &files);

    const char *leaf = path_leaf(relative);
    const char *catalogue = catalogue_for_path(relative);

    struct catalogue_view folders;
    struct catalogue_view system;

    catalogue_view_load(&folders, CATALOGUE_FOLDER);
    catalogue_view_load(&system, catalogue);

    int ok = buffer_puts(out, "{") && space_json(out, "space", catalogue_root) && json_field(out, "name", leaf)
             && buffer_puts(out, ",") && json_field(out, "path", relative) && buffer_puts(out, ",")
             && catalogue_types_json(out) && buffer_printf(out, ",\"folder\":\"%s\",", CATALOGUE_FOLDER);

    const char *leaf_friendly = friendly_folder_name(leaf);
    if (ok && leaf_friendly) ok = json_field(out, "friendly", leaf_friendly) && buffer_puts(out, ",");

    if (ok && catalogue) ok = json_field(out, "catalogue", catalogue) && buffer_puts(out, ",");
    ok = ok && buffer_puts(out, "\"entries\":[");

    long long bytes = 0;
    long long modified = 0;

    ok = ok && buffer_puts(out, "{") && json_field(out, "stem", leaf) && buffer_puts(out, ",")
         && (!leaf_friendly || (json_field(out, "friendly", leaf_friendly) && buffer_puts(out, ",")))
         && json_field(out, "catalogue", CATALOGUE_FOLDER) && buffer_puts(out, ",\"folder\":true,\"self\":true,")
         && catalogue_files_json(out, &folders, leaf, &bytes, &modified) && buffer_puts(out, ",")
         && json_number(out, "bytes", bytes) && buffer_puts(out, ",") && json_number(out, "modified", modified)
         && buffer_puts(out, "}");

    for (size_t i = 0; ok && i < files.count; ++i) {
        bytes = 0;
        modified = 0;

        ok = buffer_puts(out, ",{") && json_field(out, "stem", files.names[i]) && buffer_puts(out, ",");

        const char *friendly = friendly_content_name(leaf, files.names[i]);
        if (ok && friendly) ok = json_field(out, "friendly", friendly) && buffer_puts(out, ",");
        if (ok && catalogue) ok = json_field(out, "catalogue", catalogue) && buffer_puts(out, ",");

        ok = ok && catalogue_files_json(out, catalogue ? &system : &folders, files.names[i], &bytes, &modified)
             && buffer_puts(out, ",") && json_number(out, "bytes", bytes) && buffer_puts(out, ",")
             && json_number(out, "modified", modified) && buffer_puts(out, "}");
    }

    for (size_t i = 0; ok && i < directories.count; ++i) {
        char within[PATH_MAX];
        if ((size_t) snprintf(within, sizeof(within), "%s/%s", relative, directories.names[i]) >= sizeof(within))
            continue;

        bytes = 0;
        modified = 0;

        const char *sub_friendly = friendly_folder_name(directories.names[i]);

        ok = buffer_puts(out, ",{") && json_field(out, "stem", directories.names[i]) && buffer_puts(out, ",")
             && (!sub_friendly || (json_field(out, "friendly", sub_friendly) && buffer_puts(out, ",")))
             && json_field(out, "path", within) && buffer_puts(out, ",")
             && json_field(out, "catalogue", CATALOGUE_FOLDER) && buffer_puts(out, ",\"folder\":true,")
             && catalogue_files_json(out, &folders, directories.names[i], &bytes, &modified) && buffer_puts(out, ",")
             && json_number(out, "bytes", bytes) && buffer_puts(out, ",") && json_number(out, "modified", modified)
             && buffer_puts(out, "}");
    }

    const int names_catalogue = catalogue && assign_lookup(leaf) != NULL;

    for (size_t type = 0; ok && names_catalogue && type < CATALOGUE_TYPE_COUNT; ++type) {
        if (overlay_is_stepped(catalogue_types[type])) continue;

        for (size_t file = 0; ok && file < system.scans[type].count; ++file) {
            const char *stem = system.scans[type].files[file].stem;

            if (strcmp(stem, "default") == 0) continue;
            int known = content_holds(catalogue, stem);

            for (size_t earlier = 0; !known && earlier < type; ++earlier)
                for (size_t seen = 0; !known && seen < system.scans[earlier].count; ++seen)
                    known = strcmp(system.scans[earlier].files[seen].stem, stem) == 0;

            for (size_t seen = 0; !known && seen < file; ++seen)
                known = strcmp(system.scans[type].files[seen].stem, stem) == 0;

            if (known) continue;

            bytes = 0;
            modified = 0;

            const char *gone_friendly = friendly_content_name(leaf, stem);

            ok = buffer_puts(out, ",{") && json_field(out, "stem", stem) && buffer_puts(out, ",")
                 && (!gone_friendly || (json_field(out, "friendly", gone_friendly) && buffer_puts(out, ",")))
                 && json_field(out, "catalogue", catalogue) && buffer_puts(out, ",\"orphan\":true,")
                 && catalogue_files_json(out, &system, stem, &bytes, &modified) && buffer_puts(out, ",")
                 && json_number(out, "bytes", bytes) && buffer_puts(out, ",") && json_number(out, "modified", modified)
                 && buffer_puts(out, "}");
        }
    }

    ok = ok && buffer_puts(out, "]");
    if (ok && catalogue) ok = overlays_json(out, &system);

    catalogue_view_free(&folders);
    catalogue_view_free(&system);
    child_list_free(&directories);
    child_list_free(&files);
    return ok && buffer_puts(out, "}");
}

static long long directory_bytes(const char *path, const int depth) {
    if (depth > MUWEB_SCAN_DEPTH) return 0;

    DIR *directory = opendir(path);
    if (!directory) return 0;

    long long bytes = 0;
    const struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;

        char child[PATH_MAX];
        if (!join_path(child, sizeof(child), path, entry->d_name)) continue;

        struct stat info;
        if (stat(child, &info) < 0) continue;

        if (S_ISDIR(info.st_mode))
            bytes += directory_bytes(child, depth + 1);
        else if (S_ISREG(info.st_mode))
            bytes += (long long) info.st_size;
    }
    closedir(directory);
    return bytes;
}

static int is_state_directory(const char *path, size_t *slot_count, char *preview, const size_t preview_size) {
    DIR *directory = opendir(path);
    if (!directory) return 0;

    size_t slots = 0;
    size_t shots = 0;
    if (preview && preview_size) preview[0] = '\0';

    const struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;

        if (strcasecmp(file_extension(entry->d_name), "state") == 0) {
            slots += 1;
            continue;
        }
        if (!preview || !preview_size || strcasecmp(file_extension(entry->d_name), "png") != 0) continue;

        char shot[PATH_MAX];
        struct stat info;
        if (!join_path(shot, sizeof(shot), path, entry->d_name)) continue;
        if (stat(shot, &info) < 0 || !S_ISREG(info.st_mode) || info.st_size == 0) continue;

        // Just ignore the use of rand here, srand might be better but it's just for previews...
        shots += 1;
        if ((size_t) rand() % shots == 0) snprintf(preview, preview_size, "%s", entry->d_name);
    }
    closedir(directory);

    if (slot_count) *slot_count = slots;
    return slots > 0;
}

static int
pickles_walk_states(struct buffer *out, const char *base, const char *relative, const int depth, int *first) {
    if (depth > MUWEB_SCAN_DEPTH) return 1;

    char path[PATH_MAX];
    if (!join_path(path, sizeof(path), base, relative)) return 1;

    DIR *directory = opendir(path);
    if (!directory) return 1;

    int ok = 1;
    const struct dirent *entry;
    while (ok && ((entry = readdir(directory)))) {
        if (entry->d_name[0] == '.') continue;

        char child[PATH_MAX];
        if (!join_path(child, sizeof(child), path, entry->d_name)) continue;

        struct stat info;
        if (stat(child, &info) < 0 || !S_ISDIR(info.st_mode)) continue;

        char child_relative[PATH_MAX];
        if ((size_t) snprintf(
                child_relative, sizeof(child_relative), "%s%s%s", relative, *relative ? "/" : "", entry->d_name
            )
            >= sizeof(child_relative))
            continue;

        size_t slots = 0;
        char preview[NAME_MAX + 1];
        if (is_state_directory(child, &slots, preview, sizeof(preview))) {
            const char *friendly = friendly_content_name(path_leaf(relative), entry->d_name);

            if (!*first) ok = buffer_puts(out, ",");
            ok = ok && buffer_puts(out, "{") && json_field(out, "path", child_relative) && buffer_puts(out, ",")
                 && json_field(out, "name", entry->d_name) && buffer_puts(out, ",")
                 && (!friendly || (json_field(out, "friendly", friendly) && buffer_puts(out, ",")))
                 && json_number(out, "slots", (long long) slots) && buffer_puts(out, ",")
                 && json_number(out, "bytes", directory_bytes(child, 0)) && buffer_puts(out, ",")
                 && json_number(out, "modified", info.st_mtime);
            if (ok && preview[0]) ok = buffer_puts(out, ",") && json_field(out, "preview", preview);
            ok = ok && buffer_puts(out, "}");
            *first = 0;
            continue;
        }

        ok = pickles_walk_states(out, base, child_relative, depth + 1, first);
    }
    closedir(directory);
    return ok;
}

#define SAVE_BACKUP_NONE (-1)
#define SAVE_SKIP        (-2)

// Fairly certain all of the usual suspects are captured, but I'm sure there are more!
static const char *const save_extensions[] = {"srm", "rtc", "sav", "mcd", "mcr", "mpk", "eep", "fla", "dsv",
                                              "brm", "bkr", "vmu", "vms", "gme", "pak", "sgm", "nv"};

static int is_save_extension(const char *name) {
    const char *extension = file_extension(name);
    if (!*extension) return 0;

    for (size_t i = 0; i < sizeof(save_extensions) / sizeof(save_extensions[0]); ++i) {
        if (strcasecmp(extension, save_extensions[i]) == 0) return 1;
    }
    return 0;
}

struct save_file {
    char relative[PATH_MAX];
    char base[PATH_MAX];
    char group[PATH_MAX];
    int backup;
    int save;
    long long bytes;
    long long modified;
};

struct save_scan {
    struct save_file *files;
    size_t count;
    size_t capacity;
};

static int classify_save(const char *name, char *base, const size_t base_size) {
    const size_t length = strlen(name);
    if (length >= 4 && strcasecmp(name + length - 4, ".sum") == 0) return SAVE_SKIP;
    if (strstr(name, ".pending.")) return SAVE_SKIP;

    const char *dot = strrchr(name, '.');
    if (dot && strncasecmp(dot, ".bk", 3) == 0) {
        const char *digits = dot + 3;
        if (*digits) {
            int index = 0;
            for (const char *p = digits; *p; ++p) {
                if (!isdigit((unsigned char) *p))
                    return snprintf(base, base_size, "%s", name) >= 0 ? SAVE_BACKUP_NONE : SAVE_SKIP;
                index = index * 10 + (*p - '0');
            }

            const size_t stem = (size_t) (dot - name);
            if (stem >= base_size) return SAVE_SKIP;
            memcpy(base, name, stem);
            base[stem] = '\0';
            return index;
        }
    }

    return snprintf(base, base_size, "%s", name) < (int) base_size ? SAVE_BACKUP_NONE : SAVE_SKIP;
}

static int save_scan_add(
    struct save_scan *scan, const char *relative, const char *base, const char *group, const int backup, const int save,
    const struct stat *info
) {
    if (scan->count == scan->capacity) {
        const size_t next = scan->capacity ? scan->capacity * 2 : 64;
        struct save_file *grown = realloc(scan->files, next * sizeof(struct save_file));
        if (!grown) return 0;
        scan->files = grown;
        scan->capacity = next;
    }

    struct save_file *file = &scan->files[scan->count];
    if ((size_t) snprintf(file->relative, sizeof(file->relative), "%s", relative) >= sizeof(file->relative)) return 0;
    if ((size_t) snprintf(file->base, sizeof(file->base), "%s", base) >= sizeof(file->base)) return 0;

    if ((size_t) snprintf(file->group, sizeof(file->group), "%s", group) >= sizeof(file->group)) return 0;

    file->backup = backup;
    file->save = save;
    file->bytes = (long long) info->st_size;
    file->modified = (long long) info->st_mtime;
    scan->count += 1;
    return 1;
}

static void save_scan_walk(struct save_scan *scan, const char *root, const char *relative, const int depth) {
    if (depth > MUWEB_SCAN_DEPTH) return;

    char path[PATH_MAX];
    if (!join_path(path, sizeof(path), root, relative)) return;

    DIR *directory = opendir(path);
    if (!directory) return;

    const struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;

        char child[PATH_MAX];
        if (!join_path(child, sizeof(child), path, entry->d_name)) continue;

        struct stat info;
        if (stat(child, &info) < 0) continue;

        char child_relative[PATH_MAX];
        if ((size_t) snprintf(
                child_relative, sizeof(child_relative), "%s%s%s", relative, *relative ? "/" : "", entry->d_name
            )
            >= sizeof(child_relative))
            continue;

        if (S_ISDIR(info.st_mode)) {
            save_scan_walk(scan, root, child_relative, depth + 1);
            continue;
        }

        if (!S_ISREG(info.st_mode) || info.st_size == 0) continue;

        char base_name[PATH_MAX];
        const int backup = classify_save(entry->d_name, base_name, sizeof(base_name));
        if (backup == SAVE_SKIP) continue;

        char base_relative[PATH_MAX];
        const size_t directory_length = strlen(child_relative) - strlen(entry->d_name);
        if ((size_t) snprintf(
                base_relative, sizeof(base_relative), "%.*s%s", (int) directory_length, child_relative, base_name
            )
            >= sizeof(base_relative))
            continue;

        char group[PATH_MAX];
        const char *slash = strchr(child_relative, '/');
        const size_t group_length = slash ? (size_t) (slash - child_relative) : strlen(child_relative);
        if (group_length >= sizeof(group)) continue;
        memcpy(group, child_relative, group_length);
        group[group_length] = '\0';

        save_scan_add(scan, child_relative, base_relative, group, backup, is_save_extension(base_name), &info);
    }
    closedir(directory);
}

static int save_index_json(struct buffer *out, const char *root) {
    struct save_scan scan = {0};
    save_scan_walk(&scan, root, "", 0);

    int ok = 1;
    int first = 1;

    for (size_t i = 0; ok && i < scan.count; ++i) {
        const struct save_file *save = &scan.files[i];
        if (save->backup != SAVE_BACKUP_NONE || !save->save) continue;

        long long total = save->bytes;
        const char *name = strrchr(save->relative, '/');
        name = name ? name + 1 : save->relative;

        char stem[PATH_MAX];
        char folder[PATH_MAX] = "";

        snprintf(stem, sizeof(stem), "%s", name);
        strip_extension(stem);

        if (name > save->relative) {
            const size_t length = (size_t) (name - save->relative) - 1;
            if (length < sizeof(folder)) {
                memcpy(folder, save->relative, length);
                folder[length] = '\0';
            }
        }

        const char *friendly = friendly_content_name(path_leaf(folder), stem);

        if (!first) ok = buffer_puts(out, ",");
        ok = ok && buffer_puts(out, "{") && json_field(out, "path", save->relative) && buffer_puts(out, ",")
             && json_field(out, "name", name) && buffer_puts(out, ",")
             && (!friendly || (json_field(out, "friendly", friendly) && buffer_puts(out, ",")))
             && json_number(out, "bytes", save->bytes) && buffer_puts(out, ",")
             && json_number(out, "modified", save->modified) && buffer_puts(out, ",\"backups\":[");

        int written = 0;

        for (int index = 0; ok && index < 32; ++index) {
            for (size_t j = 0; j < scan.count; ++j) {
                if (scan.files[j].backup != index) continue;
                if (strcmp(scan.files[j].base, save->relative) != 0) continue;

                total += scan.files[j].bytes;
                if (written && !((ok = buffer_puts(out, ",")))) break;
                ok = buffer_puts(out, "{") && json_number(out, "index", index) && buffer_puts(out, ",")
                     && json_number(out, "bytes", scan.files[j].bytes) && buffer_puts(out, ",")
                     && json_number(out, "modified", scan.files[j].modified) && buffer_puts(out, "}");
                written += 1;
                break;
            }
        }

        ok = ok && buffer_puts(out, "],") && json_number(out, "total", total) && buffer_puts(out, "}");
        first = 0;
    }

    ok = ok && buffer_puts(out, "],\"other\":[");
    first = 1;

    for (size_t i = 0; ok && i < scan.count; ++i) {
        if (scan.files[i].save) continue;

        int seen = 0;
        for (size_t j = 0; j < i; ++j) {
            if (!scan.files[j].save && strcmp(scan.files[j].group, scan.files[i].group) == 0) seen = 1;
        }
        if (seen) continue;

        long long bytes = 0;
        long long modified = 0;
        long long files = 0;

        for (size_t j = 0; j < scan.count; ++j) {
            if (scan.files[j].save || strcmp(scan.files[j].group, scan.files[i].group) != 0) continue;
            bytes += scan.files[j].bytes;
            if (scan.files[j].modified > modified) modified = scan.files[j].modified;
            files += 1;
        }

        if (!first) ok = buffer_puts(out, ",");
        ok = ok && buffer_puts(out, "{") && json_field(out, "group", scan.files[i].group) && buffer_puts(out, ",")
             && json_number(out, "files", files) && buffer_puts(out, ",") && json_number(out, "bytes", bytes)
             && buffer_puts(out, ",") && json_number(out, "modified", modified) && buffer_puts(out, ",\"items\":[");

        int written = 0;
        for (size_t j = 0; ok && j < scan.count && written < 200; ++j) {
            if (scan.files[j].save || strcmp(scan.files[j].group, scan.files[i].group) != 0) continue;

            const char *name = strrchr(scan.files[j].relative, '/');
            name = name ? name + 1 : scan.files[j].relative;

            if (written && !((ok = buffer_puts(out, ",")))) break;
            ok = buffer_puts(out, "{") && json_field(out, "path", scan.files[j].relative) && buffer_puts(out, ",")
                 && json_field(out, "name", name) && buffer_puts(out, ",")
                 && json_number(out, "bytes", scan.files[j].bytes) && buffer_puts(out, ",")
                 && json_number(out, "modified", scan.files[j].modified) && buffer_puts(out, "}");
            written += 1;
        }

        ok = ok && buffer_puts(out, "]}");
        first = 0;
    }

    free(scan.files);
    return ok;
}

static int pickles_index_json(struct buffer *out) {
    char state_base[PATH_MAX];
    char sram_base[PATH_MAX];
    int first = 1;

    if (!buffer_puts(out, "{") || !space_json(out, "space", pickles_root)) return 0;
    if (!buffer_puts(out, "\"content\":[")) return 0;
    if (join_path(state_base, sizeof(state_base), pickles_root, "state"))
        pickles_walk_states(out, state_base, "", 0, &first);

    if (!buffer_puts(out, "],\"sram\":[")) return 0;
    if (join_path(sram_base, sizeof(sram_base), pickles_root, "sram")) {
        if (!save_index_json(out, sram_base)) return 0;
    } else if (!buffer_puts(out, "],\"other\":[")) {
        return 0;
    }

    return buffer_puts(out, "]}");
}

static int
manifest_lookup(const char *manifest_path, const char *group, const char *key, char *out, const size_t out_size) {
    out[0] = '\0';

    FILE *file = fopen(manifest_path, "r");
    if (!file) return 0;

    char line[1024];
    int in_group = 0;
    int found = 0;

    while (fgets(line, sizeof(line), file)) {
        char *text = line;
        while (*text == ' ' || *text == '\t')
            ++text;

        size_t length = strlen(text);
        while (length && (text[length - 1] == '\n' || text[length - 1] == '\r' || text[length - 1] == ' '))
            text[--length] = '\0';

        if (*text == '[') {
            char *close = strchr(text, ']');
            if (!close) continue;
            *close = '\0';
            in_group = strcmp(text + 1, group) == 0;
            continue;
        }

        if (!in_group) continue;

        char *separator = strchr(text, '=');
        if (!separator) continue;
        *separator = '\0';

        if (strcmp(text, key) != 0) continue;
        snprintf(out, out_size, "%s", separator + 1);
        found = 1;
        break;
    }

    fclose(file);
    return found;
}

static int manifest_drop_group(const char *manifest_path, const char *group) {
    FILE *file = fopen(manifest_path, "r");
    if (!file) return 1;

    struct buffer kept = {0};
    char line[1024];
    int skipping = 0;
    int ok = 1;

    while (ok && fgets(line, sizeof(line), file)) {
        char probe[1024];
        snprintf(probe, sizeof(probe), "%s", line);

        char *text = probe;
        while (*text == ' ' || *text == '\t')
            ++text;

        size_t length = strlen(text);
        while (length && (text[length - 1] == '\n' || text[length - 1] == '\r' || text[length - 1] == ' '))
            text[--length] = '\0';

        if (*text == '[') {
            char *close = strchr(text, ']');
            skipping = close && (*close = '\0', strcmp(text + 1, group) == 0);
        }

        if (!skipping) ok = buffer_puts(&kept, line);
    }
    fclose(file);

    if (ok) ok = write_file_atomic(manifest_path, kept.data ? kept.data : "", kept.length);
    buffer_free(&kept);
    return ok;
}

static int pickles_content_json(struct buffer *out, const char *relative) {
    char content_relative[PATH_MAX];
    if ((size_t) snprintf(content_relative, sizeof(content_relative), "state/%s", relative) >= sizeof(content_relative))
        return 0;

    char content_path[PATH_MAX];
    if (!resolve_within(pickles_root, content_relative, content_path, sizeof(content_path))) return 0;

    char manifest_path[PATH_MAX];
    if (!join_path(manifest_path, sizeof(manifest_path), content_path, "states.ini")) return 0;

    DIR *directory = opendir(content_path);
    if (!directory) return 0;

    char **slots = NULL;
    size_t slot_count = 0;
    size_t slot_capacity = 0;

    const struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;
        if (strcasecmp(file_extension(entry->d_name), "state") != 0) continue;

        char stem[NAME_MAX + 1];
        snprintf(stem, sizeof(stem), "%s", entry->d_name);
        strip_extension(stem);
        string_array_add(&slots, &slot_count, &slot_capacity, stem);
    }
    closedir(directory);

    qsort(slots, slot_count, sizeof(char *), string_compare);

    int ok = buffer_puts(out, "{") && json_field(out, "path", relative) && buffer_puts(out, ",\"slots\":[");

    for (size_t i = 0; ok && i < slot_count; ++i) {
        char state_name[PATH_MAX];
        char preview_name[PATH_MAX];
        char state_path[PATH_MAX];
        char preview_path[PATH_MAX];
        char value[512];
        struct stat info;

        snprintf(state_name, sizeof(state_name), "%s.state", slots[i]);
        snprintf(preview_name, sizeof(preview_name), "%s.png", slots[i]);

        if (!join_path(state_path, sizeof(state_path), content_path, state_name)) continue;
        if (!join_path(preview_path, sizeof(preview_path), content_path, preview_name)) continue;
        if (stat(state_path, &info) < 0) continue;

        if (i && !((ok = buffer_puts(out, ",")))) break;

        ok = buffer_puts(out, "{") && json_field(out, "id", slots[i]) && buffer_puts(out, ",")
             && json_field(out, "state", state_name) && buffer_puts(out, ",") && json_number(out, "bytes", info.st_size)
             && buffer_puts(out, ",") && json_number(out, "modified", info.st_mtime);

        if (ok && access(preview_path, R_OK) == 0)
            ok = buffer_puts(out, ",") && json_field(out, "preview", preview_name);

        static const char *const manifest_keys[] = {"name", "crc", "core", "core_version"};
        for (size_t key = 0; ok && key < sizeof(manifest_keys) / sizeof(manifest_keys[0]); ++key) {
            if (!manifest_lookup(manifest_path, slots[i], manifest_keys[key], value, sizeof(value))) continue;
            ok = buffer_puts(out, ",") && json_field(out, manifest_keys[key], value);
        }

        if (ok && manifest_lookup(manifest_path, slots[i], "created", value, sizeof(value)))
            ok = buffer_puts(out, ",") && json_number(out, "created", strtoll(value, NULL, 10));

        ok = ok && buffer_puts(out, "}");
    }

    string_array_free(slots, slot_count);
    return ok && buffer_puts(out, "]}");
}

struct connection {
    int socket;
    time_t deadline;

    struct buffer request;
    size_t header_length;
    size_t content_length;

    struct buffer response;
    size_t sent;

    int body_descriptor;
    long long body_remaining;
    int head_only;
    int revalidate;
    const char *cache_header;
    char if_modified[64];
    char auth_code[16];
    char auth_session[MUWEB_TOKEN_TEXT];
};

static const char *status_text(const int status) {
    switch (status) {
        case 200:
            return "OK";
        case 204:
            return "No Content";
        case 206:
            return "Partial Content";
        case 304:
            return "Not Modified";
        case 400:
            return "Bad Request";
        case 401:
            return "Unauthorized";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 413:
            return "Payload Too Large";
        case 500:
            return "Internal Server Error";
        case 501:
            return "Not Implemented";
        default:
            return "Error";
    }
}

static void connection_reset_body(struct connection *connection) {
    if (connection->body_descriptor >= 0) close(connection->body_descriptor);
    connection->body_descriptor = -1;
    connection->body_remaining = 0;
}

static int response_headers(
    struct connection *connection, const int status, const char *content_type, const long long length, const char *extra
) {
    return buffer_printf(
        &connection->response,
        "HTTP/1.1 %d %s\r\n"
        "Server: muweb/%s\r\n"
        "Connection: close\r\n"
        "%s"
        "X-Content-Type-Options: nosniff\r\n"
        "Content-Security-Policy: default-src 'self'; connect-src 'self'; img-src 'self' blob:; "
        "media-src 'self'; style-src 'self'; script-src 'self'\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "%s"
        "\r\n",
        status, status_text(status), MUWEB_VERSION,
        connection->cache_header ? connection->cache_header : "Cache-Control: no-store\r\n", content_type, length,
        extra ? extra : ""
    );
}

static void send_text(struct connection *connection, const int status, const char *content_type, const char *body) {
    const size_t length = strlen(body);
    connection->response.length = 0;
    connection_reset_body(connection);

    response_headers(connection, status, content_type, (long long) length, NULL);
    if (!connection->head_only) buffer_append(&connection->response, body, length);
}

static void send_error(struct connection *connection, const int status, const char *message) {
    struct buffer body = {0};
    buffer_puts(&body, "{\"error\":");
    json_string(&body, message);
    buffer_puts(&body, "}");

    send_text(connection, status, "application/json", body.data ? body.data : "{}");
    buffer_free(&body);
}

static void send_json(struct connection *connection, const struct buffer *body) {
    send_text(connection, 200, "application/json", body->data ? body->data : "{}");
}

static int http_date(const time_t when, char *out, const size_t out_size) {
    struct tm parts;
    if (!gmtime_r(&when, &parts)) return 0;
    return strftime(out, out_size, "%a, %d %b %Y %H:%M:%S GMT", &parts) > 0;
}

static void send_file(struct connection *connection, const char *path, const char *range_header) {
    struct stat info;
    if (stat(path, &info) < 0 || !S_ISREG(info.st_mode)) {
        send_error(connection, 404, "Not found");
        return;
    }

    char modified[64] = "";
    if (connection->revalidate && http_date(info.st_mtime, modified, sizeof(modified))) {
        connection->cache_header = "Cache-Control: no-cache\r\n";

        if (connection->if_modified[0] && strcmp(connection->if_modified, modified) == 0) {
            connection->response.length = 0;
            connection_reset_body(connection);
            response_headers(connection, 304, "text/plain", 0, NULL);
            return;
        }
    }

    const int descriptor = open(path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        send_error(connection, 403, "Cannot read that file");
        return;
    }

    long long start = 0;
    long long end = (long long) info.st_size - 1;
    int partial = 0;

    if (range_header && strncasecmp(range_header, "bytes=", 6) == 0 && info.st_size > 0) {
        const char *specification = range_header + 6;
        if (*specification == '-') {
            const long long suffix = strtoll(specification + 1, NULL, 10);
            if (suffix > 0) {
                start = suffix >= (long long) info.st_size ? 0 : (long long) info.st_size - suffix;
                partial = 1;
            }
        } else {
            char *cursor = NULL;
            const long long requested = strtoll(specification, &cursor, 10);
            if (cursor && requested >= 0 && requested < (long long) info.st_size) {
                start = requested;
                if (*cursor == '-' && cursor[1]) {
                    const long long requested_end = strtoll(cursor + 1, NULL, 10);
                    if (requested_end >= start && requested_end < (long long) info.st_size) end = requested_end;
                }
                partial = 1;
            }
        }
    }

    if (partial && lseek(descriptor, start, SEEK_SET) < 0) {
        close(descriptor);
        send_error(connection, 500, "Cannot seek that file");
        return;
    }

    char extra[224] = "";
    if (modified[0]) snprintf(extra, sizeof(extra), "Last-Modified: %s\r\n", modified);

    const size_t used = strlen(extra);
    if (partial)
        snprintf(
            extra + used, sizeof(extra) - used, "Accept-Ranges: bytes\r\nContent-Range: bytes %lld-%lld/%lld\r\n",
            start, end, (long long) info.st_size
        );
    else
        snprintf(extra + used, sizeof(extra) - used, "Accept-Ranges: bytes\r\n");

    connection->response.length = 0;
    connection_reset_body(connection);
    response_headers(connection, partial ? 206 : 200, mime_for(path), end - start + 1, extra);

    if (connection->head_only) {
        close(descriptor);
        return;
    }

    connection->body_descriptor = descriptor;
    connection->body_remaining = end - start + 1;
}

static const char *header_value(const char *headers, const char *name, char *out, const size_t out_size) {
    const size_t name_length = strlen(name);
    const char *line = headers;

    while (line && *line) {
        if (strncasecmp(line, name, name_length) == 0 && line[name_length] == ':') {
            const char *value = line + name_length + 1;
            while (*value == ' ' || *value == '\t')
                ++value;

            const char *end = strstr(value, "\r\n");
            const size_t length = end ? (size_t) (end - value) : strlen(value);
            if (length >= out_size) return NULL;

            memcpy(out, value, length);
            out[length] = '\0';
            return out;
        }

        line = strstr(line, "\r\n");
        if (line) line += 2;
    }
    return NULL;
}

static int session_valid(const char *token) {
    if (!token || !*token) return 0;

    const time_t now = time(NULL);
    for (size_t i = 0; i < MUWEB_SESSION_SLOTS; ++i) {
        if (!sessions[i].token[0] || sessions[i].expires <= now) continue;
        if (strcmp(sessions[i].token, token) != 0) continue;

        sessions[i].expires = now + MUWEB_SESSION_IDLE;
        return 1;
    }
    return 0;
}

static int session_open(char *out, const size_t out_size) {
    unsigned char raw[MUWEB_TOKEN_BYTES];

    const int random = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (random < 0) return 0;

    const ssize_t got = read(random, raw, sizeof(raw));
    close(random);
    if (got != (ssize_t) sizeof(raw)) return 0;

    char token[MUWEB_TOKEN_TEXT];
    for (size_t i = 0; i < sizeof(raw); ++i)
        snprintf(token + i * 2, 3, "%02x", raw[i]);

    size_t slot = 0;
    for (size_t i = 1; i < MUWEB_SESSION_SLOTS; ++i) {
        if (sessions[i].expires < sessions[slot].expires) slot = i;
    }

    snprintf(sessions[slot].token, sizeof(sessions[slot].token), "%s", token);
    sessions[slot].expires = time(NULL) + MUWEB_SESSION_IDLE;

    return (size_t) snprintf(out, out_size, "%s", token) < out_size;
}

static void session_close(const char *token) {
    for (size_t i = 0; i < MUWEB_SESSION_SLOTS; ++i) {
        if (token && *token && strcmp(sessions[i].token, token) != 0) continue;
        sessions[i].token[0] = '\0';
        sessions[i].expires = 0;
    }
}

static int write_allowed(struct connection *connection) {
    if (read_only) {
        send_error(connection, 403, "The dashboard is running read only");
        return 0;
    }
    if (!code_required) return 1;

    if (session_valid(connection->auth_session)) return 1;
    if (connection->auth_code[0] && totp_matches(code_secret, connection->auth_code)) return 1;

    send_error(connection, 401, "Unlock the dashboard with the code shown on the device");
    return 0;
}

static void handle_catalogue_api(
    struct connection *connection, const char *method, const char *path, const char *body, const size_t body_length
) {
    if (!catalogue_root[0]) {
        send_error(connection, 404, "No catalogue is configured");
        return;
    }

    if (!*path) {
        if (strcmp(method, "GET") != 0) {
            send_error(connection, 405, "Method not allowed");
            return;
        }

        if (cache_fresh(&catalogue_cache)) {
            send_json(connection, &catalogue_cache.body);
            return;
        }

        struct buffer out = {0};
        if (catalogue_index_json(&out)) {
            cache_store(&catalogue_cache, &out);
            send_json(connection, &out);
        } else {
            send_error(connection, 500, "Could not read the catalogue");
        }
        buffer_free(&out);
        return;
    }

    char *rest = strchr(path, '/');
    if (rest) *rest++ = '\0';
    const char *system = path;

    if (!rest || !*rest) {
        if (strcmp(method, "GET") != 0) {
            send_error(connection, 405, "Method not allowed");
            return;
        }
        if (!safe_relative_path(system)) {
            send_error(connection, 400, "Invalid system name");
            return;
        }

        struct buffer out = {0};
        if (catalogue_system_json(&out, system))
            send_json(connection, &out);
        else
            send_error(connection, 404, "No such system");
        buffer_free(&out);
        return;
    }

    const char *type = NULL;
    const char *leaf = NULL;

    for (size_t i = 0; i < CATALOGUE_TYPE_COUNT; ++i) {
        const size_t length = strlen(catalogue_types[i]);
        if (strncmp(rest, catalogue_types[i], length) != 0 || rest[length] != '/') continue;
        if (type && strlen(type) >= length) continue;

        type = catalogue_types[i];
        leaf = rest + length + 1;
    }

    if (!type || !*leaf || !safe_relative_path(system) || !safe_relative_path(leaf)) {
        send_error(connection, 400, "Unknown catalogue type or name");
        return;
    }

    char relative[PATH_MAX];
    if ((size_t) snprintf(relative, sizeof(relative), "%s/%s/%s", system, type, leaf) >= sizeof(relative)) {
        send_error(connection, 400, "Path too long");
        return;
    }

    if (!write_allowed(connection)) return;

    if (strcmp(method, "POST") == 0) {
        if (!upload_extension_allowed(type, file_extension(leaf))) {
            send_error(connection, 400, "That file type is not accepted here");
            return;
        }

        char target[PATH_MAX];
        if (!resolve_target(catalogue_root, relative, 1, target, sizeof(target))) {
            send_error(connection, 400, "Invalid destination");
            return;
        }
        if (!write_file_atomic(target, body, body_length)) {
            send_error(connection, 500, "Could not write that file");
            return;
        }
        cache_drop();

        log_verbose("stored %s (%zu bytes)", relative, body_length);
        send_text(connection, 200, "application/json", "{\"ok\":true}");
        return;
    }

    if (strcmp(method, "DELETE") == 0) {
        char target[PATH_MAX];
        if (!resolve_within(catalogue_root, relative, target, sizeof(target))) {
            send_error(connection, 404, "Not found");
            return;
        }
        if (unlink(target) < 0) {
            send_error(connection, 500, "Could not remove that file");
            return;
        }
        cache_drop();

        log_verbose("removed %s", relative);
        send_text(connection, 200, "application/json", "{\"ok\":true}");
        return;
    }

    send_error(connection, 405, "Method not allowed");
}

static void handle_pickles_api(
    struct connection *connection, const char *method, const char *path, const char *body, const size_t body_length
) {
    if (!pickles_root[0]) {
        send_error(connection, 404, "No save tree is configured");
        return;
    }

    if (!*path) {
        if (strcmp(method, "GET") != 0) {
            send_error(connection, 405, "Method not allowed");
            return;
        }

        if (cache_fresh(&pickles_cache)) {
            send_json(connection, &pickles_cache.body);
            return;
        }

        struct buffer out = {0};
        if (pickles_index_json(&out)) {
            cache_store(&pickles_cache, &out);
            send_json(connection, &out);
        } else {
            send_error(connection, 500, "Could not read the save tree");
        }
        buffer_free(&out);
        return;
    }

    char *rest = strchr(path, '/');
    if (rest) *rest++ = '\0';
    const char *section = path;

    if (!rest || !*rest || !safe_relative_path(rest)) {
        send_error(connection, 400, "Invalid path");
        return;
    }

    if (strcmp(section, "state") == 0) {
        if (strcmp(method, "GET") == 0) {
            struct buffer out = {0};
            if (pickles_content_json(&out, rest))
                send_json(connection, &out);
            else
                send_error(connection, 404, "No saves for that content");
            buffer_free(&out);
            return;
        }

        if (strcmp(method, "DELETE") == 0) {
            if (!write_allowed(connection)) return;

            char *slot = strrchr(rest, '/');
            if (!slot) {
                send_error(connection, 400, "Expected a slot to remove");
                return;
            }
            *slot++ = '\0';

            char content_relative[PATH_MAX];
            char content_path[PATH_MAX];
            if ((size_t) snprintf(content_relative, sizeof(content_relative), "state/%s", rest)
                    >= sizeof(content_relative)
                || !resolve_within(pickles_root, content_relative, content_path, sizeof(content_path))) {
                send_error(connection, 404, "No saves for that content");
                return;
            }

            char state_path[PATH_MAX];
            char preview_path[PATH_MAX];
            char manifest_path[PATH_MAX];
            char name[NAME_MAX + 1];

            snprintf(name, sizeof(name), "%s.state", slot);
            if (!join_path(state_path, sizeof(state_path), content_path, name)) {
                send_error(connection, 400, "Path too long");
                return;
            }
            snprintf(name, sizeof(name), "%s.png", slot);
            join_path(preview_path, sizeof(preview_path), content_path, name);
            join_path(manifest_path, sizeof(manifest_path), content_path, "states.ini");

            if (unlink(state_path) < 0) {
                send_error(connection, 404, "No such slot");
                return;
            }
            unlink(preview_path);
            manifest_drop_group(manifest_path, slot);
            cache_drop();

            log_verbose("removed slot %s from %s", slot, rest);
            send_text(connection, 200, "application/json", "{\"ok\":true}");
            return;
        }

        send_error(connection, 405, "Method not allowed");
        return;
    }

    if (strcmp(section, "sram") == 0) {
        if (strcmp(method, "POST") != 0) {
            send_error(connection, 405, "Method not allowed");
            return;
        }
        if (!write_allowed(connection)) return;
        if (strcasecmp(file_extension(rest), "srm") != 0) {
            send_error(connection, 400, "Only .srm files can be restored");
            return;
        }

        char relative[PATH_MAX];
        char target[PATH_MAX];
        if ((size_t) snprintf(relative, sizeof(relative), "sram/%s", rest) >= sizeof(relative)
            || !resolve_target(pickles_root, relative, 1, target, sizeof(target))) {
            send_error(connection, 400, "Invalid destination");
            return;
        }
        if (!write_file_atomic(target, body, body_length)) {
            send_error(connection, 500, "Could not write that file");
            return;
        }
        drop_checksum(target);
        cache_drop();

        log_verbose("restored %s (%zu bytes)", relative, body_length);
        send_text(connection, 200, "application/json", "{\"ok\":true}");
        return;
    }

    if (strcmp(section, "backup") == 0) {
        if (strcmp(method, "POST") != 0) {
            send_error(connection, 405, "Method not allowed");
            return;
        }
        if (!write_allowed(connection)) return;

        char *save = strchr(rest, '/');
        if (!save) {
            send_error(connection, 400, "Expected a save to restore");
            return;
        }
        *save++ = '\0';

        char *end = NULL;
        const long index = strtol(rest, &end, 10);
        if (!end || *end || index < 0 || index > 31 || !*save || !safe_relative_path(save)) {
            send_error(connection, 400, "Invalid backup");
            return;
        }

        char live_relative[PATH_MAX];
        char live[PATH_MAX];
        char backup[PATH_MAX];
        if ((size_t) snprintf(live_relative, sizeof(live_relative), "sram/%s", save) >= sizeof(live_relative)
            || !resolve_within(pickles_root, live_relative, live, sizeof(live))
            || (size_t) snprintf(backup, sizeof(backup), "%s.bk%ld", live, index) >= sizeof(backup)) {
            send_error(connection, 404, "No such save");
            return;
        }

        struct buffer content = {0};
        if (!read_whole_file(backup, &content)) {
            buffer_free(&content);
            send_error(connection, 404, "No such backup");
            return;
        }

        const int stored = write_file_atomic(live, content.data ? content.data : "", content.length);
        const long long restored = (long long) content.length;
        buffer_free(&content);

        if (!stored) {
            send_error(connection, 500, "Could not write that save");
            return;
        }
        drop_checksum(live);
        cache_drop();

        log_verbose("restored %s from backup %ld", save, index);

        struct buffer out = {0};
        buffer_puts(&out, "{\"ok\":true,");
        json_number(&out, "bytes", restored);
        buffer_puts(&out, "}");
        send_json(connection, &out);
        buffer_free(&out);
        return;
    }

    send_error(connection, 404, "Not found");
}

static void handle_content_api(struct connection *connection, const char *method, char *path) {
    if (!catalogue_root[0] || !content_root_count) {
        send_error(connection, 404, "No content is configured");
        return;
    }

    if (strcmp(method, "GET") != 0) {
        send_error(connection, 405, "Method not allowed");
        return;
    }

    struct buffer out = {0};

    if (!*path) {
        if (cache_fresh(&content_cache)) {
            send_json(connection, &content_cache.body);
            return;
        }

        if (content_index_json(&out)) {
            cache_store(&content_cache, &out);
            send_json(connection, &out);
        } else {
            send_error(connection, 500, "Could not read the content");
        }
        buffer_free(&out);
        return;
    }

    const size_t length = strlen(path);
    if (length && path[length - 1] == '/') path[length - 1] = '\0';

    if (!safe_relative_path(path)) {
        send_error(connection, 400, "Invalid folder name");
        return;
    }

    if (cache_fresh(&folder_cache) && strcmp(folder_cached_name, path) == 0) {
        send_json(connection, &folder_cache.body);
        return;
    }

    if (content_folder_json(&out, path)) {
        cache_store(&folder_cache, &out);
        snprintf(folder_cached_name, sizeof(folder_cached_name), "%s", path);
        send_json(connection, &out);
    } else {
        send_error(connection, 404, "No such folder");
    }

    buffer_free(&out);
}

static void handle_media(struct connection *connection, const char *path, const char *range_header) {
    char *rest = strchr(path, '/');
    if (!rest) {
        send_error(connection, 404, "Not found");
        return;
    }
    *rest++ = '\0';

    const char *root = NULL;
    if (strcmp(path, "catalogue") == 0)
        root = catalogue_root;
    else if (strcmp(path, "pickles") == 0)
        root = pickles_root;

    if (!root) {
        send_error(connection, 404, "Not found");
        return;
    }

    char target[PATH_MAX];
    if (!resolve_within(root, rest, target, sizeof(target))) {
        send_error(connection, 404, "Not found");
        return;
    }
    send_file(connection, target, range_header);
}

static void handle_static(struct connection *connection, const char *path, const char *range_header) {
    const char *relative = !*path || strcmp(path, "/") == 0 ? "index.html" : path;

    char target[PATH_MAX];
    if (!resolve_within(web_root, relative, target, sizeof(target))) {
        send_error(connection, 404, "Not found");
        return;
    }

    struct stat info;
    if (stat(target, &info) == 0 && S_ISDIR(info.st_mode)) {
        send_error(connection, 404, "Not found");
        return;
    }
    send_file(connection, target, range_header);
}

static void handle_request(struct connection *connection) {
    const char *request = connection->request.data;
    const char *body = request + connection->header_length;
    const size_t body_length = connection->content_length;

    char method[16];
    char raw_target[2048];
    if (sscanf(request, "%15s %2047s", method, raw_target) != 2) {
        send_error(connection, 400, "Malformed request");
        return;
    }

    connection->head_only = strcmp(method, "HEAD") == 0;
    if (connection->head_only) snprintf(method, sizeof(method), "GET");

    char *query = strchr(raw_target, '?');
    if (query) *query = '\0';

    char target[2048];
    if (!url_decode(raw_target, target, sizeof(target)) || target[0] != '/') {
        send_error(connection, 400, "Malformed target");
        return;
    }

    char range_storage[128];
    const char *headers = strstr(request, "\r\n");
    const char *range_header =
        headers ? header_value(headers + 2, "Range", range_storage, sizeof(range_storage)) : NULL;

    connection->cache_header = "Cache-Control: no-store\r\n";
    connection->auth_code[0] = '\0';
    connection->auth_session[0] = '\0';
    connection->if_modified[0] = '\0';
    if (headers) {
        header_value(headers + 2, "X-muOS-Code", connection->auth_code, sizeof(connection->auth_code));
        header_value(headers + 2, "X-muOS-Session", connection->auth_session, sizeof(connection->auth_session));
        header_value(headers + 2, "If-Modified-Since", connection->if_modified, sizeof(connection->if_modified));
    }

    log_verbose("%s %s", method, target);

    if (strcmp(target, "/api/auth") == 0) {
        if (strcmp(method, "GET") != 0) {
            send_error(connection, 405, "Method not allowed");
            return;
        }

        struct buffer out = {0};
        buffer_puts(&out, "{");
        json_number(&out, "required", code_required);
        buffer_puts(&out, ",");
        json_number(&out, "readonly", read_only);
        buffer_puts(&out, ",");
        json_number(&out, "unlocked", !code_required || session_valid(connection->auth_session));
        buffer_puts(&out, ",");
        json_number(&out, "step", TOTP_STEP);
        buffer_puts(&out, ",");
        json_number(&out, "digits", TOTP_DIGITS);
        buffer_puts(&out, ",");
        json_number(&out, "remaining", code_required ? totp_remaining(time(NULL)) : 0);
        buffer_puts(&out, "}");
        send_json(connection, &out);
        buffer_free(&out);
        return;
    }

    if (strcmp(target, "/api/session") == 0) {
        if (read_only) {
            send_error(connection, 403, "The dashboard is running read only");
            return;
        }

        if (strcmp(method, "DELETE") == 0) {
            session_close(connection->auth_session);
            send_text(connection, 200, "application/json", "{\"ok\":true}");
            return;
        }

        if (strcmp(method, "POST") != 0) {
            send_error(connection, 405, "Method not allowed");
            return;
        }

        if (!code_required) {
            send_error(connection, 400, "Authentication is turned off");
            return;
        }
        if (!connection->auth_code[0] || !totp_matches(code_secret, connection->auth_code)) {
            send_error(connection, 401, "That code is wrong or has expired");
            return;
        }

        char token[MUWEB_TOKEN_TEXT];
        if (!session_open(token, sizeof(token))) {
            send_error(connection, 500, "Could not start a session");
            return;
        }

        log_verbose("dashboard unlocked");

        struct buffer out = {0};
        buffer_puts(&out, "{");
        json_field(&out, "token", token);
        buffer_puts(&out, ",");
        json_number(&out, "idle", MUWEB_SESSION_IDLE);
        buffer_puts(&out, "}");
        send_json(connection, &out);
        buffer_free(&out);
        return;
    }

    if (strncmp(target, "/api/content", 12) == 0 && (target[12] == '\0' || target[12] == '/')) {
        handle_content_api(connection, method, target + (target[12] == '/' ? 13 : 12));
        return;
    }

    if (strncmp(target, "/api/catalogue", 14) == 0 && (target[14] == '\0' || target[14] == '/')) {
        handle_catalogue_api(connection, method, target + (target[14] == '/' ? 15 : 14), body, body_length);
        return;
    }
    if (strncmp(target, "/api/pickles", 12) == 0 && (target[12] == '\0' || target[12] == '/')) {
        handle_pickles_api(connection, method, target + (target[12] == '/' ? 13 : 12), body, body_length);
        return;
    }
    if (strncmp(target, "/media/", 7) == 0) {
        if (strcmp(method, "GET") != 0) {
            send_error(connection, 405, "Method not allowed");
            return;
        }
        connection->revalidate = 1;
        handle_media(connection, target + 7, range_header);
        return;
    }

    if (strcmp(method, "GET") != 0) {
        send_error(connection, 405, "Method not allowed");
        return;
    }

    if (strncmp(target, "/state/", 7) != 0) connection->revalidate = 1;

    handle_static(connection, target + 1, range_header);
}

static void connection_close(struct connection *connection) {
    if (connection->socket >= 0) close(connection->socket);
    connection_reset_body(connection);
    buffer_free(&connection->request);
    buffer_free(&connection->response);
    connection->socket = -1;
}

static int connection_ready_to_dispatch(struct connection *connection) {
    if (!connection->header_length) {
        const char *blank = strstr(connection->request.data ? connection->request.data : "", "\r\n\r\n");
        if (!blank) return 0;

        connection->header_length = (size_t) (blank - connection->request.data) + 4;

        char storage[64];
        const char *headers = strstr(connection->request.data, "\r\n");
        const char *length = headers ? header_value(headers + 2, "Content-Length", storage, sizeof(storage)) : NULL;
        connection->content_length = length ? (size_t) strtoull(length, NULL, 10) : 0;

        if (connection->content_length > MUWEB_UPLOAD_LIMIT) {
            send_error(connection, 413, "That upload is too large");
            connection->header_length = 0;
            connection->content_length = 0;
            return 1;
        }
    }

    return connection->request.length >= connection->header_length + connection->content_length;
}

static int connection_receive(struct connection *connection) {
    char chunk[8192];
    const ssize_t received = recv(connection->socket, chunk, sizeof(chunk), 0);

    if (received < 0) return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ? 1 : 0;
    if (received == 0) return 0;

    if (!buffer_append(&connection->request, chunk, (size_t) received)) return 0;

    if (!connection->header_length && connection->request.length > MUWEB_HEADER_LIMIT) {
        send_error(connection, 413, "Request headers are too large");
        return 1;
    }
    if (connection->request.length > MUWEB_HEADER_LIMIT + MUWEB_UPLOAD_LIMIT) return 0;

    connection->deadline = time(NULL) + MUWEB_IDLE_SECONDS;
    return 1;
}

static int connection_send(struct connection *connection) {
    while (connection->sent < connection->response.length) {
        const ssize_t written = send(
            connection->socket, connection->response.data + connection->sent,
            connection->response.length - connection->sent, MSG_NOSIGNAL
        );

        if (written < 0) return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ? 1 : 0;
        connection->sent += (size_t) written;
    }

    if (connection->body_descriptor < 0) return connection->body_remaining > 0 ? 0 : -1;

    char chunk[MUWEB_SEND_CHUNK];
    while (connection->body_remaining > 0) {
        const size_t wanted = connection->body_remaining < (long long) sizeof(chunk)
                                  ? (size_t) connection->body_remaining
                                  : sizeof(chunk);
        const ssize_t read_bytes = read(connection->body_descriptor, chunk, wanted);
        if (read_bytes <= 0) return read_bytes < 0 && errno == EINTR ? 1 : 0;

        ssize_t offset = 0;
        while (offset < read_bytes) {
            const ssize_t written =
                send(connection->socket, chunk + offset, (size_t) (read_bytes - offset), MSG_NOSIGNAL);

            if (written < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    lseek(connection->body_descriptor, offset - read_bytes, SEEK_CUR);
                    return 1;
                }
                return 0;
            }
            offset += written;
        }
        connection->body_remaining -= read_bytes;
    }

    return -1;
}

static int listen_socket(const uint16_t port) {
    const int socket_descriptor = socket(AF_INET6, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (socket_descriptor < 0) return -1;

    const int enable = 1;
    const int disable = 0;
    setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    setsockopt(socket_descriptor, IPPROTO_IPV6, IPV6_V6ONLY, &disable, sizeof(disable));

    struct sockaddr_in6 address = {0};
    address.sin6_family = AF_INET6;
    address.sin6_addr = in6addr_any;
    address.sin6_port = htons(port);

    if (bind(socket_descriptor, (struct sockaddr *) &address, sizeof(address)) < 0
        || listen(socket_descriptor, 16) < 0) {
        close(socket_descriptor);
        return -1;
    }

    return socket_descriptor;
}

static void print_usage(FILE *stream) {
    fprintf(
        stream, "Usage: muweb [options]\n"
                "  -r, --root DIR         directory holding the Web Dashboard (required)\n"
                "  -p, --port PORT        port to listen on (default: 80)\n"
                "  -c, --catalogue DIR    catalogue root to browse and manage\n"
                "  -s, --pickles DIR      the Pickles save root to browse and manage\n"
                "  -i, --info DIR         the 'MUOS' info directory, for assign.json and skip.ini\n"
                "  -m, --content DIR      the ROMS directory to take the content roster from,\n"
                "                         repeat once per storage root (max 4)\n"
                "  -k, --secret FILE      device secret for the rolling code, minted if absent\n"
                "  -o, --readonly         serve everything but refuse uploads and deletions\n"
                "  -C, --show-code        print the code the device should display, then exit\n"
                "  -v, --verbose          log each request\n"
                "  -V, --version          print version\n"
                "  -h, --help             show this help\n"
    );
}

static int set_root(char *destination, const char *value, const char *label) {
    char resolved[PATH_MAX];
    if (!realpath(value, resolved)) {
        fprintf(stderr, "muweb: cannot resolve %s '%s': %s\n", label, value, strerror(errno));
        return 0;
    }

    struct stat info;
    if (stat(resolved, &info) < 0 || !S_ISDIR(info.st_mode)) {
        fprintf(stderr, "muweb: %s '%s' is not a directory\n", label, value);
        return 0;
    }

    snprintf(destination, PATH_MAX, "%s", resolved);
    return 1;
}

int main(const int argc, char **argv) {
    const char *root_argument = NULL;
    const char *catalogue_argument = NULL;
    const char *pickles_argument = NULL;
    const char *info_argument = NULL;
    const char *secret_argument = NULL;
    int show_code = 0;
    long port = 80;

    const struct option options[] = {
        {"root", required_argument, NULL, 'r'},
        {"port", required_argument, NULL, 'p'},
        {"catalogue", required_argument, NULL, 'c'},
        {"pickles", required_argument, NULL, 's'},
        {"info", required_argument, NULL, 'i'},
        {"content", required_argument, NULL, 'm'},
        {"secret", required_argument, NULL, 'k'},
        {"readonly", no_argument, NULL, 'o'},
        {"show-code", no_argument, NULL, 'C'},
        {"verbose", no_argument, NULL, 'v'},
        {"version", no_argument, NULL, 'V'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    int option;
    while ((option = getopt_long(argc, argv, "r:p:c:s:i:m:k:oCvVh", options, NULL)) != -1) {
        switch (option) {
            case 'r':
                root_argument = optarg;
                break;
            case 'c':
                catalogue_argument = optarg;
                break;
            case 's':
                pickles_argument = optarg;
                break;
            case 'i':
                info_argument = optarg;
                break;
            case 'm':
                if (content_root_count >= MUWEB_CONTENT_ROOTS) {
                    fprintf(stderr, "muweb: at most %d content roots\n", MUWEB_CONTENT_ROOTS);
                    return 2;
                }
                if (!set_root(content_roots[content_root_count], optarg, "content root")) return 2;
                content_root_count += 1;
                break;
            case 'k':
                secret_argument = optarg;
                break;
            case 'o':
                read_only = 1;
                break;
            case 'C':
                show_code = 1;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'V':
                printf("muweb %s\n", MUWEB_VERSION);
                return 0;
            case 'h':
                print_usage(stdout);
                return 0;
            case 'p': {
                char *end = NULL;
                errno = 0;
                port = strtol(optarg, &end, 10);
                if (errno || !end || *end || port < 1 || port > 65535) {
                    fprintf(stderr, "muweb: invalid port '%s'\n", optarg);
                    return 2;
                }
                break;
            }
            default:
                print_usage(stderr);
                return 2;
        }
    }

    if (optind != argc || (!root_argument && !show_code)) {
        print_usage(stderr);
        return 2;
    }

    if (secret_argument) {
        if (!totp_secret_load(secret_argument, code_secret)) {
            fprintf(stderr, "muweb: cannot load or create the secret '%s': %s\n", secret_argument, strerror(errno));
            return 2;
        }
        code_required = 1;
    }

    if (show_code) {
        if (!code_required) {
            fprintf(stderr, "muweb: --show-code needs --secret\n");
            return 2;
        }

        char code[16];
        totp_code(code_secret, totp_window(time(NULL)), code, sizeof(code));
        printf("%s\n", code);
        return 0;
    }

    if (!set_root(web_root, root_argument, "web root")) return 2;
    if (catalogue_argument && !set_root(catalogue_root, catalogue_argument, "catalogue root")) return 2;
    if (pickles_argument && !set_root(pickles_root, pickles_argument, "pickles root")) return 2;

    if (info_argument && !set_root(info_root, info_argument, "info directory")) return 2;

    srand((unsigned) (time(NULL) ^ (unsigned) getpid()));

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    const int listener = listen_socket((uint16_t) port);
    if (listener < 0) {
        fprintf(stderr, "muweb: cannot listen on port %ld: %s\n", port, strerror(errno));
        return 1;
    }

    struct connection connections[MUWEB_MAX_CONNECTIONS] = {0};
    for (size_t i = 0; i < MUWEB_MAX_CONNECTIONS; ++i) {
        connections[i].socket = -1;
        connections[i].body_descriptor = -1;
    }

    log_verbose(
        "muweb %s serving %s on port %ld%s%s", MUWEB_VERSION, web_root, port, read_only ? " (read only)" : "",
        code_required ? " (changes need the device code)" : ""
    );

    while (running) {
        struct pollfd descriptors[MUWEB_MAX_CONNECTIONS + 1];
        struct connection *slots[MUWEB_MAX_CONNECTIONS + 1];
        nfds_t count = 0;

        descriptors[count].fd = listener;
        descriptors[count].events = POLLIN;
        descriptors[count].revents = 0;
        slots[count] = NULL;
        count += 1;

        for (size_t i = 0; i < MUWEB_MAX_CONNECTIONS; ++i) {
            if (connections[i].socket < 0) continue;

            descriptors[count].fd = connections[i].socket;
            descriptors[count].events =
                connections[i].response.length || connections[i].body_remaining ? POLLOUT : POLLIN;
            descriptors[count].revents = 0;
            slots[count] = &connections[i];
            count += 1;
        }

        const int ready = poll(descriptors, count, 1000);
        if (ready < 0 && errno != EINTR) {
            fprintf(stderr, "muweb: poll failed: %s\n", strerror(errno));
            break;
        }

        const time_t now = time(NULL);

        if (ready > 0 && descriptors[0].revents & POLLIN) {
            const int accepted = accept(listener, NULL, NULL);
            if (accepted >= 0) {
                struct connection *slot = NULL;
                for (size_t i = 0; i < MUWEB_MAX_CONNECTIONS; ++i) {
                    if (connections[i].socket < 0) {
                        slot = &connections[i];
                        break;
                    }
                }

                if (!slot) {
                    close(accepted);
                } else {
                    memset(slot, 0, sizeof(*slot));
                    slot->socket = accepted;
                    slot->body_descriptor = -1;
                    slot->deadline = now + MUWEB_IDLE_SECONDS;
                    fcntl(accepted, F_SETFL, fcntl(accepted, F_GETFL, 0) | O_NONBLOCK);
                }
            }
        }

        for (nfds_t i = 1; i < count; ++i) {
            struct connection *connection = slots[i];
            if (!connection || connection->socket < 0) continue;

            if (descriptors[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                connection_close(connection);
                continue;
            }

            if (descriptors[i].revents & POLLIN) {
                if (!connection_receive(connection)) {
                    connection_close(connection);
                    continue;
                }
                if (!connection->response.length && connection_ready_to_dispatch(connection)) {
                    if (!connection->response.length) handle_request(connection);
                }
            }

            if (descriptors[i].revents & POLLOUT) {
                const int state = connection_send(connection);
                if (state <= 0) connection_close(connection);
                continue;
            }

            if (connection->socket >= 0 && now > connection->deadline) connection_close(connection);
        }
    }

    for (size_t i = 0; i < MUWEB_MAX_CONNECTIONS; ++i) {
        if (connections[i].socket >= 0) connection_close(&connections[i]);
    }

    buffer_free(&catalogue_cache.body);
    buffer_free(&pickles_cache.body);
    buffer_free(&content_cache.body);
    buffer_free(&folder_cache.body);

    content_free();
    assign_free();
    skip_free();
    names_free();

    close(listener);
    return 0;
}
