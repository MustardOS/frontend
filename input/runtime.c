#define _POSIX_C_SOURCE 200809L

#include "runtime.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

static int instance_lock_fd = -1;

int runtime_prepare_state_dir(void) {
    if (mkdir(MUINPUT_STATE_DIR, 0755) == 0) {
        return 0;
    }
    if (errno != EEXIST) {
        perror("mkdir " MUINPUT_STATE_DIR);
        return -1;
    }

    struct stat info;
    if (lstat(MUINPUT_STATE_DIR, &info) < 0 || !S_ISDIR(info.st_mode)) {
        fprintf(stderr, MUINPUT_STATE_DIR " exists but is not a directory\n");
        return -1;
    }
    return 0;
}

int runtime_acquire_instance_lock(void) {
    const char *lock_path = MUINPUT_STATE_DIR "/service.lock";
    instance_lock_fd = open(lock_path, O_CREAT | O_CLOEXEC | O_RDWR, 0644);
    if (instance_lock_fd < 0) {
        perror("open muinput service lock");
        return -1;
    }
    if (flock(instance_lock_fd, LOCK_EX | LOCK_NB) < 0) {
        fprintf(stderr, "Another muinput instance is already running\n");
        close(instance_lock_fd);
        instance_lock_fd = -1;
        return -1;
    }
    return 0;
}
