#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include <common/storage/view.h>

#define MTP_VIEW "/run/muos/mtp"

int umtprd_main(int argc, char **argv);

static int fail(const char *operation) {
    fprintf(stderr, "mumtp: %s: %s\n", operation, strerror(errno));
    return EXIT_FAILURE;
}

static int command_only(const int argc, char **argv) {
    for (int i = 1; i < argc; ++i)
        if (strncmp(argv[i], "-cmd:", 5) == 0) return 1;
    return 0;
}

static int prepare_view(void) {
    if (mkdir("/run/muos", 0755) < 0 && errno != EEXIST) return 0;
    if (mkdir(MTP_VIEW, 0755) < 0 && errno != EEXIST) return 0;
    if (unshare(CLONE_NEWNS) < 0 || mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) return 0;
    if (mount("tmpfs", MTP_VIEW, "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, "mode=0755,size=64k") < 0) return 0;
    if (!storage_view_bind_all(MTP_VIEW)) return 0;
    if (mount(NULL, MTP_VIEW, NULL, MS_REMOUNT | MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL) < 0) return 0;
    return setenv("MUOS_MTP_VIEW", MTP_VIEW, 1) == 0;
}

int main(const int argc, char **argv) {
    if (geteuid() != 0) {
        errno = EACCES;
        return fail("permission denied");
    }
    if (!command_only(argc, argv) && !prepare_view()) return fail("prepare storage view");
    umask(0022);
    return umtprd_main(argc, argv);
}
