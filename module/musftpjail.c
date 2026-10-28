#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <common/storage/view.h>

extern char **environ;

#define JAIL_SOURCE "/opt/muos"
#define JAIL_ROOT   "/opt/muos/browse"
#define JAIL_GROUP  "muos"

#if defined(__aarch64__)
static const char *const server_paths[] = {JAIL_SOURCE "/bin/aarch64/sftp-server"};
#elif defined(__x86_64__)
static const char *const server_paths[] = {
    "/usr/lib/openssh/sftp-server",
    "/usr/lib/ssh/sftp-server",
    "/usr/libexec/sftp-server",
};
#else
static const char *const server_paths[] = {JAIL_SOURCE "/bin/aarch64/sftp-server"};
#endif

static int fail(const char *what) {
    fprintf(stderr, "musftpjail: %s: %s\n", what, strerror(errno));
    return EXIT_FAILURE;
}

static int open_server(void) {
    for (size_t i = 0; i < sizeof(server_paths) / sizeof(server_paths[0]); ++i) {
        const int server = open(server_paths[i], O_RDONLY | O_CLOEXEC);
        if (server >= 0) return server;
    }
    return -1;
}

static int jail_path(char *out, const size_t size, const char *name) {
    const int written = snprintf(out, size, "%s/%s", JAIL_ROOT, name);
    return written >= 0 && (size_t) written < size;
}

static int touch(const char *path) {
    const int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (fd < 0) return 0;
    close(fd);
    return 1;
}

static int write_file(const char *name, const char *text) {
    char path[PATH_MAX];
    if (!jail_path(path, sizeof(path), name)) return 0;

    FILE *file = fopen(path, "w");
    if (!file) return 0;

    const int ok = fputs(text, file) >= 0;
    return fclose(file) == 0 && ok;
}

static int build_jail(void) {
    struct stat jail_stat;
    if (lstat(JAIL_ROOT, &jail_stat) < 0) {
        if (errno != ENOENT || mkdir(JAIL_ROOT, 0755) < 0 || lstat(JAIL_ROOT, &jail_stat) < 0)
            return fail("create jail");
    }
    if (!S_ISDIR(jail_stat.st_mode)) {
        errno = ENOTDIR;
        return fail("validate jail");
    }
    if (unshare(CLONE_NEWNS) < 0) return fail("unshare");
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) return fail("private mounts");
    if (mount("tmpfs", JAIL_ROOT, "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, "mode=0755,size=64k") < 0)
        return fail("mount jail");

    char path[PATH_MAX];
    if (!jail_path(path, sizeof(path), "dev") || mkdir(path, 0755) < 0) return fail("create dev");
    if (!jail_path(path, sizeof(path), "dev/null") || !touch(path)) return fail("create null");
    if (mount("/dev/null", path, NULL, MS_BIND, NULL) < 0) return fail("bind null");
    if (!jail_path(path, sizeof(path), "etc") || mkdir(path, 0755) < 0) return fail("create etc");
    if (!write_file("etc/passwd", "root:x:0:0:root:/:/bin/sh\n")) return fail("write passwd");
    if (!write_file("etc/group", "root:x:0:\n")) return fail("write group");

    if (!storage_view_bind_all(JAIL_ROOT)) return fail("bind storage");

    if (mount(NULL, JAIL_ROOT, NULL, MS_REMOUNT | MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL) < 0)
        return fail("seal jail");

    return EXIT_SUCCESS;
}

static int caller_allowed(void) {
    if (getuid() == 0) return 1;

    const struct group *group = getgrnam(JAIL_GROUP);
    return group && getgid() == group->gr_gid;
}

static void hide_support_files(const int ready, const int input) {
    prctl(PR_SET_PDEATHSIG, SIGKILL);

    const int watch = inotify_init1(IN_CLOEXEC);
    const int added = watch >= 0 && inotify_add_watch(watch, "/etc/passwd", IN_CLOSE_NOWRITE) >= 0;
    if (write(ready, "", 1) < 0 || !added) _exit(EXIT_FAILURE);
    close(ready);

    for (int fd = STDOUT_FILENO; fd <= STDERR_FILENO; ++fd)
        close(fd);

    struct pollfd wait = {.fd = watch, .events = POLLIN};
    poll(&wait, 1, 250);
    close(watch);

    if (umount2("/dev/null", MNT_DETACH) < 0
        || mount(NULL, "/", NULL, MS_REMOUNT | MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL) < 0 || unlink("/dev/null") < 0
        || rmdir("/dev") < 0 || unlink("/etc/passwd") < 0 || unlink("/etc/group") < 0 || rmdir("/etc") < 0
        || mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL) < 0) {
        close(input);
        _exit(EXIT_FAILURE);
    }

    for (;;) {
        const ssize_t moved = splice(STDIN_FILENO, NULL, input, NULL, 65536, SPLICE_F_MOVE);
        if (moved > 0) continue;
        if (moved < 0 && errno == EINTR) continue;
        break;
    }

    close(input);
    _exit(EXIT_SUCCESS);
}

static int start_helper(void) {
    int ready[2];
    int input[2];
    if (pipe2(ready, O_CLOEXEC) < 0) return 0;
    if (pipe2(input, O_CLOEXEC) < 0) {
        close(ready[0]);
        close(ready[1]);
        return 0;
    }
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
        close(ready[0]);
        close(ready[1]);
        close(input[0]);
        close(input[1]);
        return 0;
    }

    const pid_t helper = fork();
    if (helper < 0) {
        close(ready[0]);
        close(ready[1]);
        close(input[0]);
        close(input[1]);
        return 0;
    }
    if (helper == 0) {
        close(ready[0]);
        close(input[0]);
        hide_support_files(ready[1], input[1]);
    }

    close(ready[1]);
    close(input[1]);
    char byte;
    const ssize_t got = read(ready[0], &byte, 1);
    close(ready[0]);
    if (got != 1 || dup2(input[0], STDIN_FILENO) < 0) {
        close(input[0]);
        return 0;
    }
    close(input[0]);
    return 1;
}

int main(void) {
    if (geteuid() != 0 || !caller_allowed()) {
        fprintf(stderr, "musftpjail: permission denied\n");
        return EXIT_FAILURE;
    }
    if (setgroups(0, NULL) < 0 || setgid(0) < 0 || setuid(0) < 0) return fail("become root");

    const int server = open_server();
    if (server < 0) return fail("open sftp-server");

    if (build_jail() != EXIT_SUCCESS) return EXIT_FAILURE;
    if (chdir(JAIL_ROOT) < 0 || chroot(".") < 0 || chdir("/") < 0) return fail("enter jail");
    if (!start_helper()) {
        errno = EIO;
        return fail("start jail helper");
    }
    if (prctl(PR_SET_DUMPABLE, 0) < 0 || prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) return fail("restrict process");

    umask(0022);
    clearenv();
    setenv("HOME", "/", 1);
    setenv("USER", "root", 1);
    setenv("LOGNAME", "root", 1);

    char *const args[] = {"sftp-server", NULL};
    fexecve(server, args, environ);

    return fail("start sftp-server");
}
