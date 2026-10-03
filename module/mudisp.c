#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

#define DISP_DEV          "/dev/disp"
#define DISP_SET_CONFIG   0x14
#define DISP_GET_CONFIG   0x15
#define DISP_SUPPORT_MODE 0xc4
#define DISP_SCREEN_W     0x07
#define DISP_SCREEN_H     0x08
#define DISP_LAYER_GET    0x48
#define DISP_LAYER_SET2   0x49
#define DISP_LAYER_GET2   0x4a

#define DISP_OUTPUT_HDMI 4

#define FB_DEV         "/dev/fb0"
#define FBIOGET_DMABUF 0x80084621

#define LAYER2_SIZE       224
#define LAYER2_ZORDER     4
#define LAYER2_ALPHA_MODE 5
#define LAYER2_ALPHA      6
#define LAYER2_WIN        8
#define LAYER2_FD         32
#define LAYER2_FB_SIZE    36
#define LAYER2_CROP       88
#define LAYER2_ENABLE     208
#define LAYER2_CHANNEL    212
#define LAYER2_LAYER      216

#define MIRROR_POLL_NS 8000000L

struct fb_dmabuf_export {
    uint32_t fd;
    uint32_t flags;
};

struct disp_device_config {
    uint32_t type;
    uint32_t mode;
    uint32_t format;
    uint32_t bits;
    uint32_t eotf;
    uint32_t cs;
    uint32_t dvi_hdmi;
    uint32_t range;
    uint32_t scan;
    uint32_t aspect_ratio;
    uint32_t reserve1;
};

_Static_assert(sizeof(struct disp_device_config) == 44, "disp_device_config must match the kernel copy size");

struct config_field {
    const char *name;
    size_t offset;
};

static const struct config_field fields[] = {
    {"type", offsetof(struct disp_device_config, type)},
    {"mode", offsetof(struct disp_device_config, mode)},
    {"format", offsetof(struct disp_device_config, format)},
    {"bits", offsetof(struct disp_device_config, bits)},
    {"eotf", offsetof(struct disp_device_config, eotf)},
    {"cs", offsetof(struct disp_device_config, cs)},
    {"dvi_hdmi", offsetof(struct disp_device_config, dvi_hdmi)},
    {"range", offsetof(struct disp_device_config, range)},
    {"scan", offsetof(struct disp_device_config, scan)},
    {"aspect_ratio", offsetof(struct disp_device_config, aspect_ratio)},
};

#define FIELD_COUNT (sizeof(fields) / sizeof(fields[0]))

static uint32_t *field_ptr(struct disp_device_config *cfg, const size_t index) {
    return (uint32_t *) ((char *) cfg + fields[index].offset);
}

static void print_usage(const char *argv0) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s get [screen]\n", argv0);
    fprintf(stderr, "  %s set [screen] key=value...\n", argv0);
    fprintf(stderr, "  %s support <mode> [screen]\n", argv0);
    fprintf(stderr, "  %s mirror [screen]           show the framebuffer on another screen until stopped\n", argv0);
    fprintf(stderr, "\nKeys: type mode format bits eotf cs dvi_hdmi range scan aspect_ratio\n");
    fprintf(stderr, "Exit status for support is 0 when the connected sink accepts the mode\n");
}

static int parse_uint(const char *text, uint32_t *out) {
    char *end = NULL;

    errno = 0;
    const unsigned long value = strtoul(text, &end, 0);
    if (errno || !end || end == text || *end || value > UINT32_MAX) return 0;

    *out = (uint32_t) value;
    return 1;
}

static int disp_open(void) {
    const int fd = open(DISP_DEV, O_RDWR);
    if (fd < 0) fprintf(stderr, "mudisp: cannot open %s: %s\n", DISP_DEV, strerror(errno));

    return fd;
}

static int get_config(const int fd, const uint32_t screen, struct disp_device_config *cfg) {
    unsigned long args[4] = {screen, (unsigned long) cfg, 0, 0};

    memset(cfg, 0, sizeof(*cfg));
    if (ioctl(fd, DISP_GET_CONFIG, args) < 0) {
        fprintf(stderr, "mudisp: get config failed: %s\n", strerror(errno));
        return 0;
    }

    return 1;
}

static void print_config(const struct disp_device_config *cfg) {
    for (size_t i = 0; i < FIELD_COUNT; i++)
        printf("%s=%u\n", fields[i].name, *field_ptr((struct disp_device_config *) cfg, i));
}

static int cmd_get(const uint32_t screen) {
    const int fd = disp_open();
    if (fd < 0) return 1;

    struct disp_device_config cfg;
    const int ok = get_config(fd, screen, &cfg);
    close(fd);

    if (!ok) return 1;

    print_config(&cfg);
    return 0;
}

static int apply_pair(struct disp_device_config *cfg, const char *pair) {
    const char *eq = strchr(pair, '=');
    if (!eq) return 0;

    const size_t key_len = (size_t) (eq - pair);
    for (size_t i = 0; i < FIELD_COUNT; i++) {
        if (strlen(fields[i].name) != key_len || strncmp(fields[i].name, pair, key_len) != 0) continue;
        return parse_uint(eq + 1, field_ptr(cfg, i));
    }

    return 0;
}

static int cmd_set(const uint32_t screen, const int pair_count, char **pairs) {
    if (pair_count < 1) return 2;

    const int fd = disp_open();
    if (fd < 0) return 1;

    struct disp_device_config want;
    if (!get_config(fd, screen, &want)) {
        close(fd);
        return 1;
    }

    for (int i = 0; i < pair_count; i++) {
        if (!apply_pair(&want, pairs[i])) {
            fprintf(stderr, "mudisp: invalid setting '%s'\n", pairs[i]);
            close(fd);
            return 2;
        }
    }

    if (want.type != DISP_OUTPUT_HDMI) {
        fprintf(stderr, "mudisp: screen %u is not an HDMI output (type %u)\n", screen, want.type);
        close(fd);
        return 1;
    }

    struct disp_device_config live;
    if (get_config(fd, screen, &live) && memcmp(&live, &want, sizeof(want)) == 0) {
        close(fd);
        print_config(&live);
        return 0;
    }

    unsigned long args[4] = {screen, (unsigned long) &want, 0, 0};
    if (ioctl(fd, DISP_SET_CONFIG, args) < 0) {
        fprintf(stderr, "mudisp: set config failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    struct disp_device_config got;
    const int read_back = get_config(fd, screen, &got);
    close(fd);

    if (!read_back) return 1;

    print_config(&got);

    int mismatch = 0;
    for (size_t i = 0; i < FIELD_COUNT; i++) {
        const uint32_t w = *field_ptr(&want, i);
        const uint32_t g = *field_ptr(&got, i);
        if (w == g) continue;

        fprintf(stderr, "mudisp: %s requested %u, driver applied %u\n", fields[i].name, w, g);
        mismatch = 1;
    }

    return mismatch ? 3 : 0;
}

static int cmd_support(const uint32_t mode, const uint32_t screen) {
    const int fd = disp_open();
    if (fd < 0) return 1;

    unsigned long args[4] = {screen, mode, 0, 0};
    const int result = ioctl(fd, DISP_SUPPORT_MODE, args);
    close(fd);

    if (result < 0) {
        fprintf(stderr, "mudisp: support check failed: %s\n", strerror(errno));
        return 2;
    }

    printf("mode %u %s\n", mode, result == 1 ? "supported" : "unsupported");
    return result == 1 ? 0 : 1;
}

static volatile sig_atomic_t mirror_run = 1;

static void mirror_stop(const int sig) {
    (void) sig;
    mirror_run = 0;
}

static void put32(uint8_t *buf, const size_t off, const uint32_t value) {
    memcpy(buf + off, &value, sizeof(value));
}

static int set_layer(const int fd, const uint32_t screen, uint8_t *layer) {
    unsigned long args[4] = {screen, (unsigned long) layer, 1, 0};
    return ioctl(fd, DISP_LAYER_SET2, args) == 0;
}

#define LAYER2_CROP_SIZE 32

#define LAYER_SIZE    184
#define LAYER_CROP    120
#define LAYER_CHANNEL 172
#define LAYER_LAYER   176

static int get_source_crop(const int fd, uint8_t *crop, const uint32_t channel, const uint32_t layer_id) {
    uint8_t source[LAYER_SIZE];
    memset(source, 0, sizeof(source));
    put32(source, LAYER_CHANNEL, channel);
    put32(source, LAYER_LAYER, layer_id);

    unsigned long args[4] = {0, (unsigned long) source, 1, 0};
    if (ioctl(fd, DISP_LAYER_GET, args) != 0) return 0;

    memcpy(crop, source + LAYER_CROP, LAYER2_CROP_SIZE);
    return 1;
}

static int get_source_layer(const int fd, uint8_t *source, const uint32_t channel, const uint32_t layer_id) {
    memset(source, 0, LAYER2_SIZE);
    put32(source, LAYER2_CHANNEL, channel);
    put32(source, LAYER2_LAYER, layer_id);

    unsigned long args[4] = {0, (unsigned long) source, 1, 0};
    if (ioctl(fd, DISP_LAYER_GET2, args) != 0) return 0;

    uint32_t enabled = 0;
    memcpy(&enabled, source + LAYER2_ENABLE, sizeof(enabled));
    return enabled != 0;
}

static void letterbox(uint8_t *layer, const struct fb_var_screeninfo *var, const int screen_w, const int screen_h) {
    uint32_t win_w = (uint32_t) screen_w;
    uint32_t win_h = (uint32_t) ((uint64_t) screen_w * var->yres / var->xres);

    if (win_h > (uint32_t) screen_h) {
        win_h = (uint32_t) screen_h;
        win_w = (uint32_t) ((uint64_t) screen_h * var->xres / var->yres);
    }

    put32(layer, LAYER2_WIN, ((uint32_t) screen_w - win_w) / 2);
    put32(layer, LAYER2_WIN + 4, ((uint32_t) screen_h - win_h) / 2);
    put32(layer, LAYER2_WIN + 8, win_w);
    put32(layer, LAYER2_WIN + 12, win_h);
}

static int cmd_mirror(const uint32_t screen) {
    const int disp = disp_open();
    if (disp < 0) return 1;

    const int fb = open(FB_DEV, O_RDONLY);
    if (fb < 0) {
        fprintf(stderr, "mudisp: cannot open %s: %s\n", FB_DEV, strerror(errno));
        close(disp);
        return 1;
    }

    unsigned long size_args[4] = {screen, 0, 0, 0};
    const int screen_w = ioctl(disp, DISP_SCREEN_W, size_args);
    const int screen_h = ioctl(disp, DISP_SCREEN_H, size_args);

    struct fb_var_screeninfo var;
    struct fb_dmabuf_export dmabuf = {0, O_CLOEXEC};

    if (screen_w <= 0 || screen_h <= 0 || ioctl(fb, FBIOGET_VSCREENINFO, &var) < 0 || var.xres == 0 || var.yres == 0
        || ioctl(fb, FBIOGET_DMABUF, &dmabuf) < 0) {
        fprintf(stderr, "mudisp: screen %u or the framebuffer is not ready\n", screen);
        close(fb);
        close(disp);
        return 1;
    }

    uint8_t source[LAYER2_SIZE];
    uint32_t source_channel = 0;
    uint32_t source_layer = 0;
    int found = 0;

    for (uint32_t channel = 0; channel < 4 && !found; channel++) {
        for (uint32_t layer_id = 0; layer_id < 4 && !found; layer_id++) {
            if (!get_source_layer(disp, source, channel, layer_id)) continue;
            source_channel = channel;
            source_layer = layer_id;
            found = 1;
        }
    }

    if (!found) {
        fprintf(stderr, "mudisp: no framebuffer layer on screen 0\n");
        close((int) dmabuf.fd);
        close(fb);
        close(disp);
        return 1;
    }

    uint8_t layer[LAYER2_SIZE];
    memcpy(layer, source, sizeof(layer));

    layer[LAYER2_ZORDER] = 0;
    put32(layer, LAYER2_FD, dmabuf.fd);
    for (int plane = 0; plane < 3; plane++) {
        put32(layer, LAYER2_FB_SIZE + (size_t) plane * 8, var.xres_virtual);
        put32(layer, LAYER2_FB_SIZE + (size_t) plane * 8 + 4, var.yres_virtual);
    }
    letterbox(layer, &var, screen_w, screen_h);
    put32(layer, LAYER2_ENABLE, 1);
    put32(layer, LAYER2_LAYER, 0);

    int placed = 0;
    for (uint32_t channel = 0; channel < 4 && !placed; channel++) {
        put32(layer, LAYER2_CHANNEL, channel);
        placed = set_layer(disp, screen, layer);
    }

    if (!placed) {
        fprintf(stderr, "mudisp: no free layer on screen %u: %s\n", screen, strerror(errno));
        close((int) dmabuf.fd);
        close(fb);
        close(disp);
        return 1;
    }

    signal(SIGTERM, mirror_stop);
    signal(SIGINT, mirror_stop);
    signal(SIGHUP, mirror_stop);

    uint8_t shown_crop[LAYER2_CROP_SIZE];
    memcpy(shown_crop, layer + LAYER2_CROP, sizeof(shown_crop));

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    while (mirror_run) {
        next.tv_nsec += MIRROR_POLL_NS;
        if (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);

        uint8_t live_crop[LAYER2_CROP_SIZE];
        if (!get_source_crop(disp, live_crop, source_channel, source_layer)) continue;
        if (memcmp(shown_crop, live_crop, sizeof(shown_crop)) == 0) continue;

        memcpy(shown_crop, live_crop, sizeof(shown_crop));
        memcpy(layer + LAYER2_CROP, shown_crop, sizeof(shown_crop));
        set_layer(disp, screen, layer);
    }

    put32(layer, LAYER2_ENABLE, 0);
    set_layer(disp, screen, layer);

    close((int) dmabuf.fd);
    close(fb);
    close(disp);
    return 0;
}

int main(const int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 2;
    }

    uint32_t screen = 0;

    if (strcmp(argv[1], "get") == 0) {
        if (argc > 2 && !parse_uint(argv[2], &screen)) {
            print_usage(argv[0]);
            return 2;
        }

        return cmd_get(screen);
    }

    if (strcmp(argv[1], "set") == 0) {
        int first = 2;
        if (argc > 2 && !strchr(argv[2], '=')) {
            if (!parse_uint(argv[2], &screen)) {
                print_usage(argv[0]);
                return 2;
            }
            first = 3;
        }

        const int rc = cmd_set(screen, argc - first, argv + first);
        if (rc == 2) print_usage(argv[0]);

        return rc;
    }

    if (strcmp(argv[1], "support") == 0) {
        uint32_t mode = 0;
        if (argc < 3 || !parse_uint(argv[2], &mode) || (argc > 3 && !parse_uint(argv[3], &screen))) {
            print_usage(argv[0]);
            return 2;
        }

        return cmd_support(mode, screen);
    }

    if (strcmp(argv[1], "mirror") == 0) {
        screen = 1;
        if (argc > 2 && !parse_uint(argv[2], &screen)) {
            print_usage(argv[0]);
            return 2;
        }

        return cmd_mirror(screen);
    }

    print_usage(argv[0]);
    return 2;
}
