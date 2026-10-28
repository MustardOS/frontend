#include "view.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include "mount.h"

enum folder_availability {
    FOLDER_REQUIRED,
    FOLDER_PRESENT,
    FOLDER_MOUNTED,
};

static const struct {
    const char *name;
    const char *source;
    enum folder_availability availability;
} folders[] = {
    {"Primary Storage", "/mnt/mmc", FOLDER_MOUNTED},
    {"Secondary Storage", "/mnt/sdcard", FOLDER_MOUNTED},
    {"External Storage", "/mnt/usb", FOLDER_MOUNTED},
    {"Internal", "/opt/muos", FOLDER_REQUIRED},
    {"Boot", "/mnt/boot", FOLDER_MOUNTED},
    {"History", "/run/muos/storage/info/history", FOLDER_PRESENT},
    {"Collections", "/run/muos/storage/info/collection", FOLDER_PRESENT},
    {"Catalogue", "/run/muos/storage/info/catalogue", FOLDER_PRESENT},
};

static int join_path(char *out, const size_t size, const char *root, const char *name) {
    const int written = snprintf(out, size, "%s/%s", root, name);
    return written >= 0 && (size_t) written < size;
}

static int source_available(const size_t index) {
    struct stat info;

    if (folders[index].availability == FOLDER_MOUNTED) return mount_path_active(folders[index].source);
    return stat(folders[index].source, &info) == 0 && S_ISDIR(info.st_mode);
}

static int bind_folder(const char *root, const char *name, const char *source) {
    char target[PATH_MAX];
    struct stat info;

    if (stat(source, &info) < 0 || !S_ISDIR(info.st_mode) || !join_path(target, sizeof(target), root, name)) return 0;
    if (mkdir(target, 0755) < 0 && errno != EEXIST) return 0;
    if (stat(target, &info) < 0 || !S_ISDIR(info.st_mode)) return 0;
    if (mount(source, target, NULL, MS_BIND, NULL) < 0) return 0;
    return mount(NULL, target, NULL, MS_BIND | MS_REMOUNT | MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL) == 0;
}

int storage_view_bind_all(const char *root) {
    if (!root || root[0] != '/') return 0;

    for (size_t i = 0; i < sizeof(folders) / sizeof(folders[0]); ++i) {
        if (!source_available(i)) {
            if (folders[i].availability == FOLDER_REQUIRED) return 0;
            continue;
        }
        if (!bind_folder(root, folders[i].name, folders[i].source)) return 0;
    }

    return 1;
}

static uint64_t blocks_to_bytes(const uint64_t blocks, const uint64_t size) {
    if (size && blocks > UINT64_MAX / size) return UINT64_MAX;
    return (uint64_t) blocks * size;
}

static void add_space(uint64_t *total, const uint64_t value) {
    *total = UINT64_MAX - *total < value ? UINT64_MAX : *total + value;
}

int storage_view_capacity(const char *root, uint64_t *total, uint64_t *available) {
    if (!root || !total || !available) return 0;

    DIR *directory = opendir(root);
    if (!directory) return 0;

    dev_t devices[sizeof(folders) / sizeof(folders[0])];
    size_t device_count = 0;
    *total = 0;
    *available = 0;

    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;

        char path[PATH_MAX];
        struct stat info;
        struct statvfs space;
        if (!join_path(path, sizeof(path), root, entry->d_name) || stat(path, &info) < 0 || !S_ISDIR(info.st_mode))
            continue;

        size_t seen = 0;
        while (seen < device_count && devices[seen] != info.st_dev)
            ++seen;
        if (seen < device_count || device_count >= sizeof(devices) / sizeof(devices[0]) || statvfs(path, &space) < 0)
            continue;

        devices[device_count++] = info.st_dev;
        const uint64_t block_size = space.f_frsize ? space.f_frsize : space.f_bsize;
        add_space(total, blocks_to_bytes(space.f_blocks, block_size));
        add_space(available, blocks_to_bytes(space.f_bavail, block_size));
    }

    const int closed = closedir(directory);
    return closed == 0 && device_count > 0;
}
