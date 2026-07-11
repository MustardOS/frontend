#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../../common/fileio.h"
#include "../../common/init.h"
#include "../../common/libarchive/archive.h"
#include "../../common/libarchive/archive_entry.h"
#include "../../common/log.h"
#include "../../common/miniz/miniz.h"
#include "../core/paths.h"
#include "vfs.h"

#define ARCHIVE_SEPARATOR '#'

#define VFS_CACHE_MAX_ENTRY_BYTES ((int64_t) 4LL * 1024 * 1024 * 1024)
#define VFS_CACHE_DIR_CAP_BYTES   ((uint64_t) 2LL * 1024 * 1024 * 1024)
#define VFS_CACHE_MAX_TRACKED     512

struct retro_vfs_file_handle {
    char *path;
    FILE *fp;
};

struct retro_vfs_dir_handle {
    DIR *dir;
    struct dirent *current;
};

static int vfs_active = 0;
static int cache_dir_ready = 0;

static int split_archive_path(const char *path, char *zip_path, char *entry_name) {
    const char *sep = strrchr(path, ARCHIVE_SEPARATOR);
    if (!sep) return -1;

    const size_t prefix_len = (size_t) (sep - path);
    if (prefix_len == 0 || prefix_len >= 512) return -1;

    memcpy(zip_path, path, prefix_len);
    zip_path[prefix_len] = '\0';
    snprintf(entry_name, 256, "%s", sep + 1);

    return 0;
}

static void vfs_cache_cleanup(void) {
    DIR *d = opendir(RETRO_ARC_PATH);
    if (!d) return;

    typedef struct {
        char name[300];
        time_t mtime;
        uint64_t size;
    } cache_item_t;

    static cache_item_t items[VFS_CACHE_MAX_TRACKED];
    int count = 0;
    uint64_t total = 0;

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;

        char full[MAX_BUFFER_SIZE];
        snprintf(full, sizeof(full), "%s/%s", RETRO_ARC_PATH, de->d_name);

        struct stat st;
        if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        total += (uint64_t) st.st_size;

        if (count < VFS_CACHE_MAX_TRACKED) {
            snprintf(items[count].name, sizeof(items[count].name), "%s", de->d_name);
            items[count].mtime = st.st_mtime;
            items[count].size = (uint64_t) st.st_size;
            count++;
        }
    }
    closedir(d);

    if (total <= VFS_CACHE_DIR_CAP_BYTES) return;

    for (int i = 1; i < count; i++) {
        const cache_item_t key = items[i];
        int j = i - 1;
        while (j >= 0 && items[j].mtime > key.mtime) {
            items[j + 1] = items[j];
            j--;
        }
        items[j + 1] = key;
    }

    for (int i = 0; i < count && total > VFS_CACHE_DIR_CAP_BYTES; i++) {
        char full[MAX_BUFFER_SIZE];
        snprintf(full, sizeof(full), "%s/%s", RETRO_ARC_PATH, items[i].name);
        if (remove(full) == 0) total -= items[i].size;
    }
}

static void ensure_cache_dir(void) {
    if (cache_dir_ready) return;
    cache_dir_ready = 1;

    create_directories(RETRO_ARC_PATH, 0);
    vfs_cache_cleanup();
}

static void compute_cache_path(const char *zip_path, const char *entry_name, char *out_path, const size_t out_size) {
    struct stat st;
    unsigned long long arc_size = 0;
    long long arc_mtime = 0;
    if (stat(zip_path, &st) == 0) {
        arc_size = (unsigned long long) st.st_size;
        arc_mtime = (long long) st.st_mtime;
    }

    char key[1024];
    snprintf(key, sizeof(key), "%s#%s#%llu#%lld", zip_path, entry_name, arc_size, arc_mtime);

    const mz_ulong crc = mz_crc32(MZ_CRC32_INIT, (const unsigned char *) key, strlen(key));
    snprintf(out_path, out_size, "%s/%08lX.bin", RETRO_ARC_PATH, (unsigned long) crc);
}

static int extract_entry_to_file(const char *zip_path, const char *entry_name, const char *dest_path) {
    struct archive *a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    if (archive_read_open_filename(a, zip_path, 65536) != ARCHIVE_OK) {
        LOG_ERROR(mux_module, "vfs_cache: failed to open archive '%s': %s", zip_path, archive_error_string(a));
        archive_read_free(a);
        return -1;
    }

    int result = -1;

    struct archive_entry *entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *name = archive_entry_pathname(entry);
        if (!name || strcmp(name, entry_name) != 0) {
            archive_read_data_skip(a);
            continue;
        }

        const la_int64_t entry_size = archive_entry_size(entry);
        if (entry_size <= 0 || entry_size > VFS_CACHE_MAX_ENTRY_BYTES) {
            LOG_ERROR(
                mux_module, "vfs_cache: rejecting unreasonable entry size for '%s' (%lld bytes)", entry_name,
                (long long) entry_size
            );
            break;
        }

        char tmp_path[MAX_BUFFER_SIZE];
        snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", dest_path);

        FILE *out = fopen(tmp_path, "wb");
        if (!out) {
            LOG_ERROR(mux_module, "vfs_cache: failed to open '%s' for writing", tmp_path);
            break;
        }

        char buf[65536];
        int64_t remaining = entry_size;
        int ok = 1;

        while (remaining > 0) {
            const size_t chunk = remaining < (int64_t) sizeof(buf) ? (size_t) remaining : sizeof(buf);
            const la_ssize_t got = archive_read_data(a, buf, chunk);
            if (got <= 0 || fwrite(buf, 1, (size_t) got, out) != (size_t) got) {
                ok = 0;
                break;
            }
            remaining -= got;
        }

        fclose(out);

        if (ok && remaining == 0 && rename(tmp_path, dest_path) == 0) {
            result = 0;
        } else {
            LOG_ERROR(mux_module, "vfs_cache: failed to extract '%s' from '%s'", entry_name, zip_path);
            remove(tmp_path);
        }

        break;
    }

    archive_read_free(a);
    return result;
}

static int vfs_cache_ensure(const char *zip_path, const char *entry_name, char *out_path, const size_t out_size) {
    ensure_cache_dir();
    compute_cache_path(zip_path, entry_name, out_path, out_size);

    struct stat st;
    if (stat(out_path, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) return 0;

    return extract_entry_to_file(zip_path, entry_name, out_path);
}

static const char *vfs_get_path(struct retro_vfs_file_handle *stream) {
    return stream ? stream->path : NULL;
}

static struct retro_vfs_file_handle *vfs_open(const char *path, const unsigned mode, const unsigned hints) {
    (void) hints;
    if (!path) return NULL;

    struct retro_vfs_file_handle *handle = calloc(1, sizeof(*handle));
    if (!handle) return NULL;

    handle->path = strdup(path);

    char zip_path[512];
    char entry_name[256];

    if (split_archive_path(path, zip_path, entry_name) == 0) {
        if (mode & RETRO_VFS_FILE_ACCESS_WRITE) {
            LOG_ERROR(mux_module, "vfs_open: write access requested for archive entry '%s'", path);
            free(handle->path);
            free(handle);
            return NULL;
        }

        char cache_path[MAX_BUFFER_SIZE];
        if (vfs_cache_ensure(zip_path, entry_name, cache_path, sizeof(cache_path)) != 0) {
            free(handle->path);
            free(handle);
            return NULL;
        }

        handle->fp = fopen(cache_path, "rb");
        if (!handle->fp) {
            LOG_ERROR(mux_module, "vfs_open: failed to open cached entry '%s'", cache_path);
            free(handle->path);
            free(handle);
            return NULL;
        }

        return handle;
    }

    const char *fmode;
    if (mode & RETRO_VFS_FILE_ACCESS_WRITE) {
        fmode = (mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) && file_exist(path) ? "r+b" : "w+b";
    } else {
        fmode = "rb";
    }

    handle->fp = fopen(path, fmode);
    if (!handle->fp) {
        free(handle->path);
        free(handle);
        return NULL;
    }

    return handle;
}

static int vfs_close(struct retro_vfs_file_handle *stream) {
    if (!stream) return -1;

    if (stream->fp) fclose(stream->fp);
    free(stream->path);
    free(stream);

    return 0;
}

static int64_t vfs_size(struct retro_vfs_file_handle *stream) {
    if (!stream || !stream->fp) return -1;

    const long cur = ftell(stream->fp);
    fseek(stream->fp, 0, SEEK_END);
    const long size = ftell(stream->fp);
    fseek(stream->fp, cur, SEEK_SET);
    return size;
}

static int64_t vfs_tell(struct retro_vfs_file_handle *stream) {
    return stream && stream->fp ? ftell(stream->fp) : -1;
}

static int64_t vfs_seek(struct retro_vfs_file_handle *stream, const int64_t offset, const int seek_position) {
    if (!stream || !stream->fp) return -1;

    const int whence = seek_position == RETRO_VFS_SEEK_POSITION_CURRENT ? SEEK_CUR
                       : seek_position == RETRO_VFS_SEEK_POSITION_END   ? SEEK_END
                                                                        : SEEK_SET;
    if (fseek(stream->fp, offset, whence) != 0) return -1;
    return ftell(stream->fp);
}

static int64_t vfs_read(struct retro_vfs_file_handle *stream, void *s, const uint64_t len) {
    if (!stream || !s || !stream->fp) return -1;
    return (int64_t) fread(s, 1, len, stream->fp);
}

static int64_t vfs_write(struct retro_vfs_file_handle *stream, const void *s, const uint64_t len) {
    if (!stream || !s || !stream->fp) return -1;
    return (int64_t) fwrite(s, 1, len, stream->fp);
}

static int vfs_flush(struct retro_vfs_file_handle *stream) {
    return stream && stream->fp ? fflush(stream->fp) : -1;
}

static int64_t vfs_truncate(struct retro_vfs_file_handle *stream, const int64_t length) {
    if (!stream || !stream->fp) return -1;
    if (ftruncate(fileno(stream->fp), length) != 0) return -1;
    return 0;
}

static int vfs_remove(const char *path) {
    return remove(path) == 0 ? 0 : -1;
}

static int vfs_rename(const char *old_path, const char *new_path) {
    return rename(old_path, new_path) == 0 ? 0 : -1;
}

static int vfs_stat(const char *path, int32_t *size) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;

    if (size) *size = (int32_t) st.st_size;

    int flags = RETRO_VFS_STAT_IS_VALID;
    if (S_ISDIR(st.st_mode)) flags |= RETRO_VFS_STAT_IS_DIRECTORY;
    if (S_ISCHR(st.st_mode)) flags |= RETRO_VFS_STAT_IS_CHARACTER_SPECIAL;
    return flags;
}

static int vfs_mkdir(const char *dir) {
    if (mkdir(dir, 0755) == 0) return 0;
    return errno == EEXIST ? -2 : -1;
}

static struct retro_vfs_dir_handle *vfs_opendir(const char *dir, const bool include_hidden) {
    (void) include_hidden;

    DIR *d = opendir(dir);
    if (!d) return NULL;

    struct retro_vfs_dir_handle *handle = calloc(1, sizeof(*handle));
    if (!handle) {
        closedir(d);
        return NULL;
    }

    handle->dir = d;
    return handle;
}

static bool vfs_readdir(struct retro_vfs_dir_handle *dirstream) {
    if (!dirstream || !dirstream->dir) return false;
    dirstream->current = readdir(dirstream->dir);
    return dirstream->current != NULL;
}

static const char *vfs_dirent_get_name(struct retro_vfs_dir_handle *dirstream) {
    return dirstream && dirstream->current ? dirstream->current->d_name : NULL;
}

static bool vfs_dirent_is_dir(struct retro_vfs_dir_handle *dirstream) {
    return dirstream && dirstream->current && dirstream->current->d_type == DT_DIR;
}

static int vfs_closedir(struct retro_vfs_dir_handle *dirstream) {
    if (!dirstream) return -1;
    if (dirstream->dir) closedir(dirstream->dir);
    free(dirstream);
    return 0;
}

static struct retro_vfs_interface vfs_iface = {
    .get_path = vfs_get_path,
    .open = vfs_open,
    .close = vfs_close,
    .size = vfs_size,
    .tell = vfs_tell,
    .seek = vfs_seek,
    .read = vfs_read,
    .write = vfs_write,
    .flush = vfs_flush,
    .remove = vfs_remove,
    .rename = vfs_rename,
    .truncate = vfs_truncate,
    .stat = vfs_stat,
    .mkdir = vfs_mkdir,
    .opendir = vfs_opendir,
    .readdir = vfs_readdir,
    .dirent_get_name = vfs_dirent_get_name,
    .dirent_is_dir = vfs_dirent_is_dir,
    .closedir = vfs_closedir,
};

bool vfs_bridge_get_interface(struct retro_vfs_interface_info *info) {
    if (!info || info->required_interface_version > 3) return false;

    info->required_interface_version = 3;
    info->iface = &vfs_iface;
    vfs_active = 1;

    return true;
}

int vfs_bridge_is_active(void) {
    return vfs_active;
}
