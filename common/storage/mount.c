#include "mount.h"

#include <stdio.h>
#include <string.h>

static void decode_path(char *path) {
    char *source = path;
    char *target = path;
    while (*source) {
        if (source[0] == '\\' && source[1] >= '0' && source[1] <= '7' && source[2] >= '0' && source[2] <= '7'
            && source[3] >= '0' && source[3] <= '7') {
            *target++ = (char) ((source[1] - '0') * 64 + (source[2] - '0') * 8 + (source[3] - '0'));
            source += 4;
        } else {
            *target++ = *source++;
        }
    }
    *target = '\0';
}

int mount_path_active(const char *path) {
    if (!path || path[0] != '/') return 0;

    FILE *mounts = fopen("/proc/self/mountinfo", "r");
    if (!mounts) return 0;

    char line[8192];
    int active = 0;
    while (fgets(line, sizeof(line), mounts)) {
        char *state = NULL;
        char *point = line;
        for (int field = 0; field < 5 && point; ++field)
            point = strtok_r(field ? NULL : line, " ", &state);
        if (!point) continue;
        decode_path(point);
        if (strcmp(point, path) == 0) {
            active = 1;
            break;
        }
    }

    (void) fclose(mounts);
    return active;
}
