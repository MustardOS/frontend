#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../common/fileio.h"
#include "../common/init.h"
#include "../common/log.h"
#include "../common/miniz/miniz.h"
#include "vfs.h"

#define ARCHIVE_SEPARATOR '#'

struct retro_vfs_file_handle {
    char *path;
    FILE *fp;
    void *mem_data;
    size_t mem_size;
    int64_t mem_pos;
};

struct retro_vfs_dir_handle {
    DIR *dir;
    struct dirent *current;
};

static int vfs_active = 0;

static int
split_archive_path(const char *path, char *zip_path, const size_t zip_size, char *entry_name, const size_t entry_size) {
    const char *sep = strrchr(path, ARCHIVE_SEPARATOR);
    if (!sep) return -1;

    const size_t prefix_len = (size_t) (sep - path);
    if (prefix_len == 0 || prefix_len >= zip_size) return -1;

    memcpy(zip_path, path, prefix_len);
    zip_path[prefix_len] = '\0';
    snprintf(entry_name, entry_size, "%s", sep + 1);

    return 0;
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

    if (split_archive_path(path, zip_path, sizeof(zip_path), entry_name, sizeof(entry_name)) == 0) {
        if (mode & RETRO_VFS_FILE_ACCESS_WRITE) {
            LOG_ERROR(mux_module, "vfs_open: write access requested for archive entry '%s'", path);
            free(handle->path);
            free(handle);
            return NULL;
        }

        mz_zip_archive zip;
        mz_zip_zero_struct(&zip);

        if (!mz_zip_reader_init_file(&zip, zip_path, 0)) {
            LOG_ERROR(mux_module, "vfs_open: failed to open archive '%s'", zip_path);
            free(handle->path);
            free(handle);
            return NULL;
        }

        size_t size = 0;
        void *data = mz_zip_reader_extract_file_to_heap(&zip, entry_name, &size, 0);
        mz_zip_reader_end(&zip);

        if (!data) {
            LOG_ERROR(mux_module, "vfs_open: failed to extract '%s' from '%s'", entry_name, zip_path);
            free(handle->path);
            free(handle);
            return NULL;
        }

        handle->mem_data = data;
        handle->mem_size = size;
        handle->mem_pos = 0;
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
    free(stream->mem_data);
    free(stream->path);
    free(stream);

    return 0;
}

static int64_t vfs_size(struct retro_vfs_file_handle *stream) {
    if (!stream) return -1;
    if (stream->mem_data) return (int64_t) stream->mem_size;
    if (!stream->fp) return -1;

    const long cur = ftell(stream->fp);
    fseek(stream->fp, 0, SEEK_END);
    const long size = ftell(stream->fp);
    fseek(stream->fp, cur, SEEK_SET);
    return size;
}

static int64_t vfs_tell(struct retro_vfs_file_handle *stream) {
    if (!stream) return -1;
    if (stream->mem_data) return stream->mem_pos;
    return stream->fp ? ftell(stream->fp) : -1;
}

static int64_t vfs_seek(struct retro_vfs_file_handle *stream, const int64_t offset, const int seek_position) {
    if (!stream) return -1;

    if (stream->mem_data) {
        int64_t new_pos;
        switch (seek_position) {
            case RETRO_VFS_SEEK_POSITION_CURRENT:
                new_pos = stream->mem_pos + offset;
                break;
            case RETRO_VFS_SEEK_POSITION_END:
                new_pos = (int64_t) stream->mem_size + offset;
                break;
            default:
                new_pos = offset;
                break;
        }
        if (new_pos < 0 || new_pos > (int64_t) stream->mem_size) return -1;
        stream->mem_pos = new_pos;
        return new_pos;
    }

    if (!stream->fp) return -1;

    const int whence = seek_position == RETRO_VFS_SEEK_POSITION_CURRENT ? SEEK_CUR
                       : seek_position == RETRO_VFS_SEEK_POSITION_END   ? SEEK_END
                                                                        : SEEK_SET;
    if (fseek(stream->fp, offset, whence) != 0) return -1;
    return ftell(stream->fp);
}

static int64_t vfs_read(struct retro_vfs_file_handle *stream, void *s, const uint64_t len) {
    if (!stream || !s) return -1;

    if (stream->mem_data) {
        const size_t remaining = stream->mem_size - (size_t) stream->mem_pos;
        const size_t to_copy = len < remaining ? (size_t) len : remaining;
        memcpy(s, (const char *) stream->mem_data + stream->mem_pos, to_copy);
        stream->mem_pos += (int64_t) to_copy;
        return (int64_t) to_copy;
    }

    if (!stream->fp) return -1;
    return (int64_t) fread(s, 1, len, stream->fp);
}

static int64_t vfs_write(struct retro_vfs_file_handle *stream, const void *s, const uint64_t len) {
    if (!stream || !s || stream->mem_data || !stream->fp) return -1;
    return (int64_t) fwrite(s, 1, len, stream->fp);
}

static int vfs_flush(struct retro_vfs_file_handle *stream) {
    if (!stream || stream->mem_data) return -1;
    return stream->fp ? fflush(stream->fp) : -1;
}

static int64_t vfs_truncate(struct retro_vfs_file_handle *stream, const int64_t length) {
    if (!stream || stream->mem_data || !stream->fp) return -1;
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
