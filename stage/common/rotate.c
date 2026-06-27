#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "rotate.h"

static int parse_int(const char *s, int *out) {
    if (!s) return 0;

    while (*s && isspace((unsigned char) *s))
        s++;
    if (!*s) return 0;

    char *end = NULL;
    const long v = strtol(s, &end, 10);
    if (end == s) return 0;

    *out = (int) v;
    return 1;
}

int rotate_normalise(const int deg) {
    int r = deg % 360;
    if (r < 0) r += 360;

    if (r < 45) return rotate_0;
    if (r < 135) return rotate_90;
    if (r < 225) return rotate_180;
    if (r < 315) return rotate_270;

    return rotate_0;
}

int rotate_read(void) {
    const char *env = getenv("STAGE_ROTATE");

    int v = 0;
    if (env && parse_int(env, &v)) return rotate_normalise(v);

    const int fd = open(ROTATE_DETECT, O_RDONLY);
    if (fd < 0) return rotate_0;

    char buf[64];
    const ssize_t n = read(fd, buf, (ssize_t) sizeof(buf) - 1);
    close(fd);

    if (n <= 0) return rotate_0;
    buf[n] = '\0';

    if (!parse_int(buf, &v)) return rotate_0;
    return rotate_normalise(v);
}

int rotate_read_cached(void) {
    static int cached = rotate_0;
    static time_t last_mtime = 0;
    static int has_stat = 0;

    const char *env = getenv("STAGE_ROTATE");
    if (env && *env) {
        int v = 0;
        if (parse_int(env, &v)) return rotate_normalise(v);
        return rotate_0;
    }

    struct stat st;
    if (stat(ROTATE_DETECT, &st) != 0) {
        cached = rotate_0;
        last_mtime = 0;
        has_stat = 0;
        return cached;
    }

    if (!has_stat || st.st_mtime != last_mtime) {
        cached = rotate_read();
        last_mtime = st.st_mtime;
        has_stat = 1;
    }

    return cached;
}

void rotate_dims(const int w, const int h, const int rot, int *out_w, int *out_h) {
    int rw = w;
    int rh = h;

    if (rot == rotate_90 || rot == rotate_270) {
        rw = h;
        rh = w;
    }

    if (out_w) *out_w = rw;
    if (out_h) *out_h = rh;
}

double rotate_angle(const int rot) {
    if (rot == rotate_90) return 90.0;
    if (rot == rotate_180) return 180.0;
    if (rot == rotate_270) return 270.0;

    return 0.0;
}
