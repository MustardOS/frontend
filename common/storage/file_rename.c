#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <common/storage/fileio.h>

#define EXFAT_MAGIC 0x2011BAB0

int file_rename(const char *source, const char *target) {
    struct stat info;
    struct statfs volume;
    char aside[PATH_MAX];

    if (lstat(target, &info) != 0 || !S_ISREG(info.st_mode) || statfs(target, &volume) != 0
        || (unsigned long) volume.f_type != EXFAT_MAGIC
        || snprintf(aside, sizeof(aside), "%s.replace-%ld", target, (long) getpid()) >= (int) sizeof(aside)) {
        return rename(source, target);
    }

    if (rename(target, aside) != 0) return rename(source, target);

    if (rename(source, target) != 0) {
        const int error = errno;
        rename(aside, target);
        errno = error;
        return -1;
    }

    remove(aside);
    return 0;
}
