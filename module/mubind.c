#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#define PATH_SIZE     4096
#define MAX_MOUNTS    512
#define MAX_KEYS      256
#define UNMOUNT_LIMIT 8

typedef struct {
    char device[32];
    char root[PATH_SIZE];
    char point[PATH_SIZE];
} mount_entry;

static mount_entry mounts[MAX_MOUNTS];
static int mount_count = 0;

static char keys[MAX_KEYS][64];
static int key_count = 0;

static int join_path(char *out, const size_t size, const char *base, const char *leaf) {
    const int length = snprintf(out, size, "%s/%s", base, leaf);
    return length > 0 && (size_t) length < size ? 0 : -1;
}

static int copy_text(char *out, const size_t size, const char *text) {
    const int length = snprintf(out, size, "%s", text);
    return length >= 0 && (size_t) length < size ? 0 : -1;
}

static void decode_path(char *path) {
    char *source = path;
    char *target = path;
    while (*source) {
        if (source[0] == '\\' && source[1] >= '0' && source[1] <= '7' && source[2] >= '0' && source[2] <= '7'
            && source[3] >= '0' && source[3] <= '7') {
            *target++ = (char) ((source[1] - '0') * 64 + (source[2] - '0') * 8 + (source[3] - '0'));
            source += 4;
        } else {
            *target++ = *source++;
        }
    }
    *target = '\0';
}

static void load_mounts(void) {
    mount_count = 0;

    FILE *file = fopen("/proc/self/mountinfo", "r");
    if (!file) return;

    char line[3 * PATH_SIZE];
    while (mount_count < MAX_MOUNTS && fgets(line, sizeof(line), file)) {
        char *state = NULL;
        char *field[5] = {0};
        for (int i = 0; i < 5; ++i) {
            field[i] = strtok_r(i ? NULL : line, " ", &state);
            if (!field[i]) break;
        }
        if (!field[4]) continue;

        mount_entry *entry = &mounts[mount_count];
        decode_path(field[3]);
        decode_path(field[4]);
        if (copy_text(entry->device, sizeof(entry->device), field[2]) != 0
            || copy_text(entry->root, sizeof(entry->root), field[3]) != 0
            || copy_text(entry->point, sizeof(entry->point), field[4]) != 0)
            continue;
        mount_count++;
    }

    fclose(file);
}

static int mount_layers(const char *path) {
    int layers = 0;
    for (int i = 0; i < mount_count; ++i) {
        if (strcmp(mounts[i].point, path) == 0) layers++;
    }
    return layers;
}

static const mount_entry *mount_top(const char *path) {
    for (int i = mount_count - 1; i >= 0; --i) {
        if (strcmp(mounts[i].point, path) == 0) return &mounts[i];
    }
    return NULL;
}

static const mount_entry *mount_containing(const char *path) {
    const mount_entry *best = NULL;
    size_t best_length = 0;

    for (int i = 0; i < mount_count; ++i) {
        const char *point = mounts[i].point;
        size_t length = strlen(point);

        int inside;
        if (strcmp(point, "/") == 0) {
            inside = path[0] == '/';
            length = 1;
        } else {
            inside = strncmp(path, point, length) == 0 && (path[length] == '\0' || path[length] == '/');
        }

        if (inside && length >= best_length) {
            best = &mounts[i];
            best_length = length;
        }
    }

    return best;
}

static int same_bind(const char *source, const char *target) {
    if (mount_layers(target) != 1) return 0;

    const mount_entry *target_mount = mount_top(target);
    const mount_entry *source_mount = mount_containing(source);
    if (!target_mount || !source_mount) return 0;

    const char *relative = source;
    if (strcmp(source_mount->point, "/") != 0) relative += strlen(source_mount->point);

    char expected[PATH_SIZE];
    if (strcmp(source_mount->root, "/") == 0) {
        if (copy_text(expected, sizeof(expected), relative[0] ? relative : "/") != 0) return 0;
    } else {
        const int length = snprintf(expected, sizeof(expected), "%s%s", source_mount->root, relative);
        if (length < 0 || (size_t) length >= sizeof(expected)) return 0;
    }

    return strcmp(source_mount->device, target_mount->device) == 0 && strcmp(expected, target_mount->root) == 0;
}

static int is_directory(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static int make_directory(const char *path) {
    if (is_directory(path)) return 0;

    char buffer[PATH_SIZE];
    if (snprintf(buffer, sizeof(buffer), "%s", path) >= (int) sizeof(buffer)) return -1;

    for (char *cursor = buffer + 1; *cursor; ++cursor) {
        if (*cursor != '/') continue;
        *cursor = '\0';
        if (mkdir(buffer, 0755) != 0 && errno != EEXIST) return -1;
        *cursor = '/';
    }

    if (mkdir(buffer, 0755) != 0 && errno != EEXIST) return -1;
    return is_directory(buffer) ? 0 : -1;
}

static void append_line(const char *file, const char *text) {
    if (!file || !file[0]) return;

    const int descriptor = open(file, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (descriptor < 0) return;

    char line[PATH_SIZE * 2 + 64];
    const int length = snprintf(line, sizeof(line), "%s\n", text);
    if (length > 0 && length < (int) sizeof(line)) {
        if (write(descriptor, line, (size_t) length) < 0) perror("write");
    }

    close(descriptor);
}

static int unmount_stack(const char *target) {
    for (int attempt = 0; attempt < UNMOUNT_LIMIT; ++attempt) {
        load_mounts();
        if (mount_layers(target) == 0) return 0;
        if (umount2(target, 0) != 0) {
            printf("Could not unmount %s\n", target);
            return -1;
        }
    }

    load_mounts();
    if (mount_layers(target) == 0) return 0;

    printf("Too many stacked mounts at %s\n", target);
    return -1;
}

static int safe_bind(const char *source, const char *target) {
    if (make_directory(target) != 0) return -1;

    load_mounts();
    if (mount_layers(target) > 0) {
        if (same_bind(source, target)) return 0;
        if (unmount_stack(target) != 0) return -1;
    }

    return mount(source, target, NULL, MS_BIND, NULL);
}

static const char *location_key(const char *location) {
    static const struct {
        const char *prefix;
        const char *key;
    } map[] = {
        {"info/catalogue", "catalogue"},   {"info/name", "name"},
        {"info/launch", "launch"},         {"info/manifest", "manifest"},
        {"info/collection", "collection"}, {"info/history", "history"},
        {"info/track", "track"},           {"package/", "package"},
    };

    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); ++i) {
        if (strncmp(location, map[i].prefix, strlen(map[i].prefix)) == 0) return map[i].key;
    }

    return location;
}

static void load_keys(const char *bindmap) {
    key_count = 0;

    FILE *file = fopen(bindmap, "r");
    if (!file) return;

    char line[PATH_SIZE * 2];
    while (key_count < MAX_KEYS && fgets(line, sizeof(line), file)) {
        char *separator = strchr(line, '|');
        if (!separator) continue;
        *separator = '\0';
        if (copy_text(keys[key_count], sizeof(keys[0]), line) == 0) key_count++;
    }

    fclose(file);
}

static int has_key(const char *key) {
    for (int i = 0; i < key_count; ++i) {
        if (strcmp(keys[i], key) == 0) return 1;
    }
    return 0;
}

static void write_bindmap(const char *bindmap, const char *key, const char *backend, const char *source) {
    if (has_key(key)) return;

    char line[PATH_SIZE * 2];
    const int length = snprintf(line, sizeof(line), "%s|%s|%s", key, backend, source);
    if (length < 0 || (size_t) length >= sizeof(line)) return;
    append_line(bindmap, line);

    if (key_count < MAX_KEYS && copy_text(keys[key_count], sizeof(keys[0]), key) == 0) key_count++;
}

static int mount_locations(int argc, char **argv) {
    if (argc < 8) return 2;

    const char *store = argv[2];
    const char *rom_root = argv[3];
    const char *sd_root = argv[4];
    const char *bindmap = argv[5];
    const char *failure = argv[6];
    const char *group = argv[7];

    load_keys(bindmap);

    for (int i = 8; i < argc; ++i) {
        const char *location = argv[i];
        char source[PATH_SIZE];
        char target[PATH_SIZE];
        const char *backend;

        if (join_path(target, sizeof(target), store, location) != 0) {
            append_line(failure, location);
            continue;
        }

        if (sd_root[0] && join_path(source, sizeof(source), sd_root, location) == 0 && is_directory(source)) {
            backend = "SDCARD";
        } else {
            if (join_path(source, sizeof(source), rom_root, location) != 0) {
                append_line(failure, location);
                continue;
            }
            backend = "ROM";

            if (!is_directory(source)) {
                printf("Creating missing SD1 path: MUOS/%s\n", location);
                if (make_directory(source) != 0) {
                    printf("FAILED to create SD1 path: MUOS/%s\n", location);
                    append_line(failure, location);
                    continue;
                }
            }
        }

        printf("%s: %s from %s\n", group, location, backend);

        if (safe_bind(source, target) == 0) {
            write_bindmap(bindmap, location_key(location), backend, source);
        } else {
            printf("FAILED to mount %s -> %s\n", source, target);
            append_line(failure, location);
        }
    }

    return 0;
}

static int mount_emulators(int argc, char **argv) {
    if (argc < 5) return 2;

    const char *store = argv[2];
    const char *emulator = argv[3];
    const char *failure = argv[4];

    for (int i = 5; i < argc; ++i) {
        char pair[PATH_SIZE];
        if (copy_text(pair, sizeof(pair), argv[i]) != 0) continue;

        char *separator = strchr(pair, '|');
        if (!separator || separator == pair) continue;
        *separator = '\0';

        const char *store_path = pair;
        const char *emulator_path = separator + 1;

        char target[PATH_SIZE];
        char point[PATH_SIZE];
        if (join_path(target, sizeof(target), store, store_path) != 0
            || join_path(point, sizeof(point), emulator, emulator_path) != 0 || make_directory(target) != 0
            || make_directory(point) != 0) {
            printf("FAILED to prepare emulator path %s -> %s\n", target, point);
            append_line(failure, store_path);
            continue;
        }

        if (safe_bind(target, point) == 0) {
            printf("Bound emulator path %s -> %s\n", target, point);
        } else {
            printf("FAILED to bind emulator path %s -> %s\n", target, point);
            append_line(failure, store_path);
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);

    int result = 2;
    if (argc > 1 && strcmp(argv[1], "locs") == 0) result = mount_locations(argc, argv);
    if (argc > 1 && strcmp(argv[1], "emulator") == 0) result = mount_emulators(argc, argv);

    if (result == 2) {
        fprintf(stderr, "Usage: %s locs STORE ROM_ROOT SD_ROOT BINDMAP FAILURE GROUP LOCATION...\n", argv[0]);
        fprintf(stderr, "       %s emulator STORE EMULATOR_DIR FAILURE STORE_PATH|EMULATOR_PATH...\n", argv[0]);
    }

    return result;
}
