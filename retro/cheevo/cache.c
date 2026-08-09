#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <openssl/rand.h>
#include "../../common/fileio.h"
#include "../../common/json/json.h"
#include "../../common/options.h"
#include "cache.h"

#define CHEEVO_CACHE_DIR  STORAGE_NETWORK "/cheevo/cache"
#define CHEEVO_HTTP_CAP   (2U * 1024U * 1024U)
#define CHEEVO_CACHE_CAP  (32U * 1024U * 1024U)
#define CHEEVO_CACHE_AGE  (30 * 24 * 60 * 60)

static int cache_directory_open(const int create) {
    if (create) create_directories(CHEEVO_CACHE_DIR, 0);
    const int directory = open(CHEEVO_CACHE_DIR, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (directory < 0) return -1;

    struct stat directory_stat;
    if (fstat(directory, &directory_stat) != 0 || !S_ISDIR(directory_stat.st_mode)
        || directory_stat.st_uid != geteuid()) {
        close(directory);
        return -1;
    }
    if ((directory_stat.st_mode & 0777) != 0700) fchmod(directory, 0700);
    return directory;
}

static int cache_body_valid(const char *body, const size_t body_size) {
    if (!body || !body_size || !json_validn(body, body_size)) return 0;
    const struct json root = json_parsen(body, body_size);
    return json_type(json_object_get(root, "Success")) == JSON_TRUE;
}

int cheevo_cache_load(const char *name, char **body, size_t *body_size) {
    if (!cheevo_cache_name_valid(name) || !body || !body_size) return -1;
    const int directory = cache_directory_open(0);
    if (directory < 0) return -1;
    const int fd = openat(directory, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        close(directory);
        return -1;
    }

    struct stat file_stat;
    const time_t now = time(NULL);
    if (fstat(fd, &file_stat) != 0 || !S_ISREG(file_stat.st_mode) || file_stat.st_uid != geteuid()
        || file_stat.st_nlink != 1 || file_stat.st_size <= 0 || file_stat.st_size > CHEEVO_HTTP_CAP
        || (now > 0 && file_stat.st_mtime > 0 && now - file_stat.st_mtime > CHEEVO_CACHE_AGE)) {
        close(fd);
        unlinkat(directory, name, 0);
        close(directory);
        return -1;
    }

    char *data = malloc((size_t) file_stat.st_size + 1);
    if (!data) {
        close(fd);
        close(directory);
        return -1;
    }
    size_t offset = 0;
    while (offset < (size_t) file_stat.st_size) {
        const ssize_t count = read(fd, data + offset, (size_t) file_stat.st_size - offset);
        if (count <= 0) {
            free(data);
            close(fd);
            close(directory);
            return -1;
        }
        offset += (size_t) count;
    }
    data[offset] = '\0';
    if (!cache_body_valid(data, offset)) {
        free(data);
        close(fd);
        unlinkat(directory, name, 0);
        close(directory);
        return -1;
    }
    futimens(fd, NULL);
    close(fd);
    close(directory);
    *body = data;
    *body_size = offset;
    return 0;
}

static void cache_trim(const int directory) {
    for (;;) {
        off_t total = 0;
        time_t oldest_time = 0;
        char oldest_name[96] = "";
        const int scan_fd = dup(directory);
        if (scan_fd < 0) return;
        DIR *scan = fdopendir(scan_fd);
        if (!scan) {
            close(scan_fd);
            return;
        }
        struct dirent *entry;
        while ((entry = readdir(scan))) {
            if (!cheevo_cache_name_valid(entry->d_name)) continue;
            struct stat file_stat;
            if (fstatat(directory, entry->d_name, &file_stat, AT_SYMLINK_NOFOLLOW) != 0 || !S_ISREG(file_stat.st_mode))
                continue;
            total += file_stat.st_size;
            if (!oldest_name[0] || file_stat.st_mtime < oldest_time) {
                oldest_time = file_stat.st_mtime;
                snprintf(oldest_name, sizeof(oldest_name), "%s", entry->d_name);
            }
        }
        closedir(scan);
        if (total <= CHEEVO_CACHE_CAP || !oldest_name[0]) return;
        if (unlinkat(directory, oldest_name, 0) != 0) return;
    }
}

void cheevo_cache_store(const char *name, const char *body, const size_t body_size) {
    if (!cheevo_cache_name_valid(name) || body_size > CHEEVO_HTTP_CAP || !cache_body_valid(body, body_size)) return;
    const int directory = cache_directory_open(1);
    if (directory < 0) return;

    char temporary[64];
    int fd = -1;
    for (int attempt = 0; attempt < 8 && fd < 0; attempt++) {
        uint32_t nonce;
        if (RAND_bytes((unsigned char *) &nonce, sizeof(nonce)) != 1) break;
        snprintf(temporary, sizeof(temporary), ".cache-%08x.tmp", nonce);
        fd = openat(directory, temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (fd < 0 && errno != EEXIST) break;
    }
    if (fd < 0) {
        close(directory);
        return;
    }

    size_t offset = 0;
    while (offset < body_size) {
        const ssize_t count = write(fd, body + offset, body_size - offset);
        if (count <= 0) break;
        offset += (size_t) count;
    }
    const int okay = offset == body_size && fsync(fd) == 0;
    close(fd);
    if (!okay || renameat(directory, temporary, directory, name) != 0)
        unlinkat(directory, temporary, 0);
    else
        cache_trim(directory);
    fsync(directory);
    close(directory);
}
