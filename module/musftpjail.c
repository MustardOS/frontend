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

extern char **environ;

#define JAIL_SOURCE "/opt/muos"
#define JAIL_ROOT   "/run/muos/sftp"
#define JAIL_GROUP  "muos"
#define JAIL_SERVER JAIL_SOURCE "/bin/aarch64/sftp-server"

static int fail(const char *what) {
    fprintf(stderr, "musftpjail: %s: %s\n", what, strerror(errno));
    return EXIT_FAILURE;
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

static int is_mounted(const char *path) {
    char parent[PATH_MAX];
    const int written = snprintf(parent, sizeof(parent), "%s/..", path);
    if (written < 0 || (size_t) written >= sizeof(parent)) return 0;

    struct stat here;
    struct stat above;
    if (stat(path, &here) < 0 || !S_ISDIR(here.st_mode) || stat(parent, &above) < 0) return 0;

    return here.st_dev != above.st_dev;
}

static int bind_folder(const char *name, const char *source) {
    char target[PATH_MAX];
    if (!jail_path(target, sizeof(target), name)) return 0;
    if (mkdir(target, 0755) < 0) return 0;

    return mount(source, target, NULL, MS_BIND | MS_REC, NULL) == 0;
}

static const struct {
    const char *name;
    const char *source;
    int always;
} folders[] = {
    {"SD1 (mmc)", "/mnt/mmc", 0},        {"SD2 (sdcard)", "/mnt/sdcard", 0}, {"USB (external)", "/mnt/usb", 0},
    {"INTERNAL (muos)", JAIL_SOURCE, 1}, {"BOOT (boot)", "/mnt/boot", 0},
};

static int bind_folders(void) {
    for (size_t i = 0; i < sizeof(folders) / sizeof(folders[0]); ++i) {
        if (!folders[i].always && !is_mounted(folders[i].source)) continue;
        if (!bind_folder(folders[i].name, folders[i].source)) return 0;
    }

    return 1;
}

static int build_jail(void) {
    if (mkdir(JAIL_ROOT, 0755) < 0 && errno != EEXIST) return fail("create jail");
    if (unshare(CLONE_NEWNS) < 0) return fail("unshare");
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) return fail("private mounts");
    if (mount("tmpfs", JAIL_ROOT, "tmpfs", MS_NOSUID, "mode=0755,size=64k") < 0) return fail("mount jail");

    char path[PATH_MAX];
    if (!jail_path(path, sizeof(path), "dev") || mkdir(path, 0755) < 0) return fail("create dev");
    if (!jail_path(path, sizeof(path), "dev/null") || !touch(path)) return fail("create null");
    if (mount("/dev/null", path, NULL, MS_BIND, NULL) < 0) return fail("bind null");
    if (!jail_path(path, sizeof(path), "etc") || mkdir(path, 0755) < 0) return fail("create etc");
    if (!write_file("etc/passwd", "root:x:0:0:root:/:/bin/sh\n")) return fail("write passwd");
    if (!write_file("etc/group", "root:x:0:\n")) return fail("write group");

    if (!bind_folders()) return fail("bind storage");

    if (mount(NULL, JAIL_ROOT, NULL, MS_REMOUNT | MS_RDONLY | MS_NOSUID, NULL) < 0) return fail("seal jail");

    return EXIT_SUCCESS;
}

static int caller_allowed(void) {
    if (getuid() == 0) return 1;

    const struct group *group = getgrnam(JAIL_GROUP);
    return group && getgid() == group->gr_gid;
}

static void hide_support_files(const int ready) {
    prctl(PR_SET_PDEATHSIG, SIGKILL);

    const int watch = inotify_init1(IN_CLOEXEC);
    const int added = watch >= 0 && inotify_add_watch(watch, "/etc/passwd", IN_CLOSE_NOWRITE) >= 0;
    if (write(ready, "", 1) < 0 || !added) _exit(EXIT_FAILURE);
    close(ready);

    for (int fd = 0; fd <= STDERR_FILENO; ++fd)
        close(fd);

    struct pollfd wait = {.fd = watch, .events = POLLIN};
    char event[sizeof(struct inotify_event) + NAME_MAX + 1];
    if (poll(&wait, 1, 10000) <= 0 || read(watch, event, sizeof(event)) <= 0) _exit(EXIT_FAILURE);

    umount2("/dev/null", MNT_DETACH);
    if (mount(NULL, "/", NULL, MS_REMOUNT | MS_NOSUID, NULL) == 0) {
        unlink("/dev/null");
        rmdir("/dev");
        unlink("/etc/passwd");
        unlink("/etc/group");
        rmdir("/etc");
        mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY | MS_NOSUID, NULL);
    }

    _exit(EXIT_SUCCESS);
}

static int start_helper(void) {
    int ready[2];
    if (pipe2(ready, O_CLOEXEC) < 0) return 0;
    signal(SIGCHLD, SIG_IGN);

    const pid_t helper = fork();
    if (helper < 0) {
        close(ready[0]);
        close(ready[1]);
        return 0;
    }
    if (helper == 0) {
        close(ready[0]);
        hide_support_files(ready[1]);
    }

    close(ready[1]);
    char byte;
    const ssize_t got = read(ready[0], &byte, 1);
    close(ready[0]);
    return got == 1;
}

int main(void) {
    if (geteuid() != 0 || !caller_allowed()) {
        fprintf(stderr, "musftpjail: permission denied\n");
        return EXIT_FAILURE;
    }
    if (setgroups(0, NULL) < 0 || setgid(0) < 0 || setuid(0) < 0) return fail("become root");

    const int server = open(JAIL_SERVER, O_RDONLY | O_CLOEXEC);
    if (server < 0) return fail("open " JAIL_SERVER);

    if (build_jail() != EXIT_SUCCESS) return EXIT_FAILURE;
    if (chdir(JAIL_ROOT) < 0 || chroot(".") < 0 || chdir("/") < 0) return fail("enter jail");
    start_helper();

    umask(0022);
    clearenv();
    setenv("HOME", "/", 1);
    setenv("USER", "root", 1);
    setenv("LOGNAME", "root", 1);

    char *const args[] = {"sftp-server", NULL};
    fexecve(server, args, environ);

    return fail("start " JAIL_SERVER);
}
