#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/loop.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#define PATH_SIZE       4096
#define MANIFEST_NAME   ".manifest"
#define MANIFEST_SUFFIX ".manifest"
#define PACK_SUFFIX     ".sqfs"
#define ACTIVE_MANIFEST "active.manifest"
#define MAX_VALID       1024

static char *valid_paths[MAX_VALID];
static int valid_count = 0;

static int join_path(char *out, const size_t size, const char *base, const char *leaf) {
    const int length = snprintf(out, size, "%s%s", base, leaf);
    return length > 0 && (size_t) length < size ? 0 : -1;
}

static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int in_library_dir(const char *path) {
    const char *slash = strrchr(path, '/');
    if (!slash) return 0;

    const size_t length = (size_t) (slash - path);
    return (length == 8 && strncmp(path, "/usr/lib", 8) == 0) || (length == 4 && strncmp(path, "/lib", 4) == 0);
}

static int is_valid(const char *path) {
    for (int i = 0; i < valid_count; ++i) {
        if (strcmp(valid_paths[i], path) == 0) return 1;
    }
    return 0;
}

static int is_mounted(const char *point) {
    FILE *file = fopen("/proc/self/mountinfo", "r");
    if (!file) return 0;

    char line[PATH_SIZE * 2];
    int found = 0;
    while (!found && fgets(line, sizeof(line), file)) {
        char *state = NULL;
        char *field = NULL;
        for (int i = 0; i < 5; ++i) {
            field = strtok_r(i ? NULL : line, " ", &state);
            if (!field) break;
        }
        if (field && strcmp(field, point) == 0) found = 1;
    }

    fclose(file);
    return found;
}

static int attach_loop(const char *image, char *device, const size_t size) {
    const int control = open("/dev/loop-control", O_RDWR | O_CLOEXEC);
    if (control < 0) return -1;

    const int number = ioctl(control, LOOP_CTL_GET_FREE);
    close(control);
    if (number < 0) return -1;

    snprintf(device, size, "/dev/loop%d", number);

    const int image_fd = open(image, O_RDONLY | O_CLOEXEC);
    if (image_fd < 0) return -1;

    const int loop_fd = open(device, O_RDWR | O_CLOEXEC);
    if (loop_fd < 0) {
        close(image_fd);
        return -1;
    }

    if (ioctl(loop_fd, LOOP_SET_FD, image_fd) != 0) {
        close(image_fd);
        close(loop_fd);
        return -1;
    }
    close(image_fd);

    struct loop_info64 info;
    memset(&info, 0, sizeof(info));
    info.lo_flags = LO_FLAGS_READ_ONLY | LO_FLAGS_AUTOCLEAR;
    snprintf((char *) info.lo_file_name, sizeof(info.lo_file_name), "%s", image);
    if (ioctl(loop_fd, LOOP_SET_STATUS64, &info) != 0) {
        ioctl(loop_fd, LOOP_CLR_FD, 0);
        close(loop_fd);
        return -1;
    }

    return loop_fd;
}

static int add_link(const char *point, const char *links, const char *name, const char *target) {
    char source[PATH_SIZE];
    char link_path[PATH_SIZE];
    if (join_path(source, sizeof(source), point, target) != 0) return -1;
    if (snprintf(link_path, sizeof(link_path), "%s/%s", links, name) >= (int) sizeof(link_path)) return -1;

    struct stat existing;
    if (lstat(link_path, &existing) == 0) {
        char current[PATH_SIZE];
        const ssize_t length = readlink(link_path, current, sizeof(current) - 1);
        if (length > 0) {
            current[length] = '\0';
            if (strncmp(current, point, strlen(point)) == 0 && strcmp(current, source) != 0) return 1;
        }
        unlink(link_path);
    }
    return symlink(source, link_path);
}

static int link_entries(const char *point, const char *links) {
    char manifest[PATH_SIZE];
    if (join_path(manifest, sizeof(manifest), point, "/" MANIFEST_NAME) != 0) return -1;

    FILE *file = fopen(manifest, "r");
    if (!file) {
        fprintf(stderr, "mulibpack: no manifest in %s\n", point);
        return -1;
    }

    int linked = 0;
    int skipped = 0;
    int aliases = 0;
    char line[PATH_SIZE * 2 + 64];

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] != 'F') continue;

        unsigned long long size = 0;
        long long mtime = 0;
        int offset = 0;
        if (sscanf(line, "F %llu %lld %n", &size, &mtime, &offset) != 2 || line[offset] != '/') continue;

        const char *target = line + offset;
        if (!in_library_dir(target)) continue;

        char source[PATH_SIZE];
        if (join_path(source, sizeof(source), point, target) != 0) continue;

        struct stat original;
        if (lstat(target, &original) != 0 || !S_ISREG(original.st_mode) || (unsigned long long) original.st_size != size
            || (long long) original.st_mtime != mtime || access(source, R_OK) != 0) {
            printf("skip %s\n", target);
            skipped++;
            continue;
        }

        const int result = add_link(point, links, base_name(target), target);
        if (result < 0) {
            printf("fail %s: %s\n", target, strerror(errno));
            skipped++;
            continue;
        }

        if (valid_count < MAX_VALID) valid_paths[valid_count++] = strdup(target);
        if (result == 0) linked++;
    }

    rewind(file);
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] != 'L') continue;

        char alias[PATH_SIZE];
        char target[PATH_SIZE];
        if (sscanf(line, "L %4095s %4095s", alias, target) != 2) continue;
        if (!in_library_dir(alias) || !is_valid(target)) continue;

        char resolved[PATH_MAX];
        if (!realpath(alias, resolved) || strcmp(resolved, target) != 0) continue;

        if (add_link(point, links, base_name(alias), target) == 0) aliases++;
    }

    fclose(file);
    printf("linked %d, aliases %d, skipped %d\n", linked, aliases, skipped);
    return linked > 0 ? 0 : -1;
}

static int do_mount(const char *image, const char *point, const char *links) {
    if (access(image, R_OK) != 0) {
        fprintf(stderr, "mulibpack: %s not found\n", image);
        return 1;
    }

    if ((mkdir(point, 0755) != 0 && errno != EEXIST) || (mkdir(links, 0755) != 0 && errno != EEXIST)) {
        perror("mulibpack: mkdir");
        return 1;
    }

    if (!is_mounted(point)) {
        char device[32];
        const int loop_fd = attach_loop(image, device, sizeof(device));
        if (loop_fd < 0) {
            perror("mulibpack: loop");
            return 1;
        }

        const int mounted = mount(device, point, "squashfs", MS_RDONLY | MS_NODEV | MS_NOSUID, NULL);
        if (mounted != 0) {
            perror("mulibpack: mount");
            ioctl(loop_fd, LOOP_CLR_FD, 0);
        }
        close(loop_fd);
        if (mounted != 0) return 1;
    }

    return link_entries(point, links) == 0 ? 0 : 1;
}

static int score_manifest(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) return -1;

    int score = 0;
    char line[PATH_SIZE * 2 + 64];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] != 'F') continue;

        unsigned long long size = 0;
        long long mtime = 0;
        int offset = 0;
        if (sscanf(line, "F %llu %lld %n", &size, &mtime, &offset) != 2 || line[offset] != '/') continue;

        const char *target = line + offset;
        if (!in_library_dir(target)) continue;

        struct stat original;
        if (lstat(target, &original) == 0 && S_ISREG(original.st_mode) && (unsigned long long) original.st_size == size
            && (long long) original.st_mtime == mtime)
            score++;
    }

    fclose(file);
    return score;
}

static int do_select(const char *directory, const char *current) {
    DIR *dir = opendir(directory);
    if (!dir) return 1;

    int best_score = current ? score_manifest(current) : -1;
    if (best_score < 0) best_score = 0;

    char best[PATH_SIZE] = "";
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        const size_t length = strlen(entry->d_name);
        const size_t suffix = strlen(MANIFEST_SUFFIX);
        if (length <= suffix || strcmp(entry->d_name + length - suffix, MANIFEST_SUFFIX) != 0) continue;
        if (strcmp(entry->d_name, ACTIVE_MANIFEST) == 0) continue;

        char manifest[PATH_SIZE];
        char pack[PATH_SIZE];
        if (snprintf(manifest, sizeof(manifest), "%s/%s", directory, entry->d_name) >= (int) sizeof(manifest)) continue;
        if (snprintf(pack, sizeof(pack), "%s/%.*s%s", directory, (int) (length - suffix), entry->d_name, PACK_SUFFIX)
            >= (int) sizeof(pack))
            continue;
        if (access(pack, R_OK) != 0) continue;

        const int score = score_manifest(manifest);
        if (score > best_score) {
            best_score = score;
            snprintf(best, sizeof(best), "%s", pack);
        }
    }

    closedir(dir);
    if (!best[0]) return 1;

    printf("%s\n", best);
    return 0;
}

int main(const int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc == 5 && strcmp(argv[1], "mount") == 0) return do_mount(argv[2], argv[3], argv[4]);
    if ((argc == 3 || argc == 4) && strcmp(argv[1], "select") == 0)
        return do_select(argv[2], argc == 4 ? argv[3] : NULL);

    fprintf(stderr, "Usage: %s mount PACK MOUNTPOINT LINKDIR\n", argv[0]);
    fprintf(stderr, "       %s select DIRECTORY [CURRENT_MANIFEST]\n", argv[0]);
    return 2;
}
