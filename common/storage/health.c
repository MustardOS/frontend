#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <common/platform/device.h>
#include <common/storage/fileio.h>
#include <common/storage/health.h>

static storage_health_status fail(storage_health_snapshot *snapshot, const storage_health_status status) {
    if (snapshot) {
        snapshot->status = status;
        snapshot->error_number = errno;
    }
    return status;
}

static int existing_parent(const char *path, char *parent, const size_t capacity) {
    if (!path || !*path) {
        errno = EINVAL;
        return -1;
    }

    const int length = snprintf(parent, capacity, "%s", path);
    if (length < 0 || (size_t) length >= capacity) {
        errno = ENAMETOOLONG;
        return -1;
    }

    struct stat st;
    while (stat(parent, &st) != 0) {
        if (errno != ENOENT && errno != ENOTDIR) return -1;

        char *slash = strrchr(parent, '/');
        if (!slash) {
            snprintf(parent, capacity, ".");
            break;
        }
        if (slash == parent) {
            parent[1] = '\0';
            break;
        }
        *slash = '\0';
    }

    if (stat(parent, &st) != 0) return -1;
    if (S_ISDIR(st.st_mode)) return 0;

    char *slash = strrchr(parent, '/');
    if (!slash) {
        snprintf(parent, capacity, ".");
    } else if (slash == parent) {
        parent[1] = '\0';
    } else {
        *slash = '\0';
    }

    return stat(parent, &st) == 0 && S_ISDIR(st.st_mode) ? 0 : -1;
}

static int path_uses_unmounted_storage(const char *path) {
    const char *const mounts[] = {
        device.storage.rom.mount, device.storage.sdcard.mount, device.storage.usb.mount
    };

    for (size_t i = 0; i < sizeof(mounts) / sizeof(mounts[0]); i++) {
        const char *mount = mounts[i];
        if (!mount || !*mount) continue;
        const size_t length = strlen(mount);
        if (strncmp(path, mount, length) != 0 || (path[length] != '/' && path[length] != '\0')) continue;
        return !is_partition_mounted(mount);
    }

    return 0;
}

storage_health_status storage_preflight_write(
    const char *path, const uint64_t required_bytes, const uint64_t reserve_bytes, storage_health_snapshot *snapshot
) {
    if (snapshot) memset(snapshot, 0, sizeof(*snapshot));

    if (path && path_uses_unmounted_storage(path)) {
        errno = ENODEV;
        return fail(snapshot, storage_health_missing);
    }

    char parent[PATH_MAX];
    if (existing_parent(path, parent, sizeof(parent)) != 0) {
        const int saved = errno;
        errno = saved;
        return fail(
            snapshot, saved == ENOENT || saved == ENODEV || saved == ENXIO ? storage_health_missing
                                                                           : storage_health_error
        );
    }

    struct statvfs fs;
    if (statvfs(parent, &fs) != 0) {
        const int saved = errno;
        errno = saved;
        return fail(
            snapshot, saved == ENOENT || saved == ENODEV || saved == ENXIO ? storage_health_missing
                                                                           : storage_health_error
        );
    }

    const uint64_t block_size = fs.f_frsize ? (uint64_t) fs.f_frsize : (uint64_t) fs.f_bsize;
    if (!block_size) {
        errno = EIO;
        return fail(snapshot, storage_health_error);
    }
    const uint64_t free_bytes = (uint64_t) fs.f_bavail > UINT64_MAX / block_size
                                    ? UINT64_MAX
                                    : (uint64_t) fs.f_bavail * block_size;
    if (snapshot) snapshot->free_bytes = free_bytes;

    if ((fs.f_flag & ST_RDONLY) != 0 || access(parent, W_OK) != 0) {
        errno = EROFS;
        return fail(snapshot, storage_health_read_only);
    }

    if (required_bytes > UINT64_MAX - reserve_bytes || free_bytes < required_bytes + reserve_bytes) {
        errno = ENOSPC;
        return fail(snapshot, storage_health_full);
    }

    if (snapshot) snapshot->status = storage_health_ok;
    return storage_health_ok;
}
