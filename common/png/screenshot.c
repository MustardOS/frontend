#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image_write.h"
#include "screenshot.h"

#define DRM_IOCTL_BASE 'd'
#define DRM_IOWR(nr, type) _IOWR(DRM_IOCTL_BASE, nr, type)

#define DRM_IOCTL_MODE_GETRESOURCES DRM_IOWR(0xA0, struct drm_mode_card_res)
#define DRM_IOCTL_MODE_GETCRTC      DRM_IOWR(0xA1, struct drm_mode_crtc)
#define DRM_IOCTL_MODE_GETFB        DRM_IOWR(0xAD, struct drm_mode_fb_cmd)
#define DRM_IOCTL_MODE_MAP_DUMB     DRM_IOWR(0xB3, struct drm_mode_map_dumb)

#define DRM_MAX_CRTCS 8
#define DRM_MAX_CARDS 4
#define FBDEV_MAX 4

struct drm_mode_card_res {
    uint64_t fb_id_ptr;
    uint64_t crtc_id_ptr;
    uint64_t connector_id_ptr;
    uint64_t encoder_id_ptr;
    uint32_t count_fbs;
    uint32_t count_crtcs;
    uint32_t count_connectors;
    uint32_t count_encoders;
    uint32_t min_width;
    uint32_t max_width;
    uint32_t min_height;
    uint32_t max_height;
};

struct drm_mode_modeinfo {
    uint32_t clock;
    uint16_t hdisplay;
    uint16_t hsync_start;
    uint16_t hsync_end;
    uint16_t htotal;
    uint16_t hskew;
    uint16_t vdisplay;
    uint16_t vsync_start;
    uint16_t vsync_end;
    uint16_t vtotal;
    uint16_t vscan;
    uint32_t vrefresh;
    uint32_t flags;
    uint32_t type;
    char name[32];
};

struct drm_mode_crtc {
    uint64_t set_connectors_ptr;
    uint32_t count_connectors;
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t x;
    uint32_t y;
    uint32_t gamma_size;
    uint32_t mode_valid;
    struct drm_mode_modeinfo mode;
};

struct drm_mode_fb_cmd {
    uint32_t fb_id;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t depth;
    uint32_t handle;
};

struct drm_mode_map_dumb {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
};

static inline uint8_t scale_bits(uint32_t value, uint32_t bits) {
    if (bits == 0) return 0;
    if (bits >= 8) return (uint8_t) (value >> (bits - 8));

    return (uint8_t) ((value * 255U) / ((1U << bits) - 1U));
}

static int png_write(const char *path, const uint8_t *rgb, uint32_t width, uint32_t height) {
    if (!width || !height) return -1;

    stbi_write_png_compression_level = 1;
    stbi_write_force_png_filter = 0;

    return stbi_write_png(path, (int) width, (int) height, 3, rgb, (int) (width * 3U)) ? 0 : -1;
}

static inline void convert_16b(uint8_t *dst, const uint8_t *src, uint32_t width) {
    const uint8_t *s = src;

    uint8_t *d = dst;
    uint32_t x = width;

    while (x >= 4) {
        uint16_t p0 = (uint16_t) s[0] | ((uint16_t) s[1] << 8);
        uint16_t p1 = (uint16_t) s[2] | ((uint16_t) s[3] << 8);
        uint16_t p2 = (uint16_t) s[4] | ((uint16_t) s[5] << 8);
        uint16_t p3 = (uint16_t) s[6] | ((uint16_t) s[7] << 8);

        d[0] = scale_bits((p0 >> 11) & 0x1F, 5);
        d[1] = scale_bits((p0 >> 5) & 0x3F, 6);
        d[2] = scale_bits(p0 & 0x1F, 5);

        d[3] = scale_bits((p1 >> 11) & 0x1F, 5);
        d[4] = scale_bits((p1 >> 5) & 0x3F, 6);
        d[5] = scale_bits(p1 & 0x1F, 5);

        d[6] = scale_bits((p2 >> 11) & 0x1F, 5);
        d[7] = scale_bits((p2 >> 5) & 0x3F, 6);
        d[8] = scale_bits(p2 & 0x1F, 5);

        d[9] = scale_bits((p3 >> 11) & 0x1F, 5);
        d[10] = scale_bits((p3 >> 5) & 0x3F, 6);
        d[11] = scale_bits(p3 & 0x1F, 5);

        s += 8;
        d += 12;
        x -= 4;
    }

    while (x--) {
        uint16_t pix = (uint16_t) s[0] | ((uint16_t) s[1] << 8);

        d[0] = scale_bits((pix >> 11) & 0x1F, 5);
        d[1] = scale_bits((pix >> 5) & 0x3F, 6);
        d[2] = scale_bits(pix & 0x1F, 5);

        s += 2;
        d += 3;
    }
}

static inline void convert_24b(uint8_t *dst, const uint8_t *src, uint32_t width) {
    const uint8_t *s = src;

    uint8_t *d = dst;
    uint32_t x = width;

    while (x >= 4) {
        d[0] = s[2];
        d[1] = s[1];
        d[2] = s[0];

        d[3] = s[5];
        d[4] = s[4];
        d[5] = s[3];

        d[6] = s[8];
        d[7] = s[7];
        d[8] = s[6];

        d[9] = s[11];
        d[10] = s[10];
        d[11] = s[9];

        s += 12;
        d += 12;
        x -= 4;
    }

    while (x--) {
        d[0] = s[2];
        d[1] = s[1];
        d[2] = s[0];

        s += 3;
        d += 3;
    }
}

static inline void convert_32b(uint8_t *dst, const uint8_t *src, uint32_t width) {
    const uint8_t *s = src;

    uint8_t *d = dst;
    uint32_t x = width;

    while (x >= 4) {
        d[0] = s[2];
        d[1] = s[1];
        d[2] = s[0];

        d[3] = s[6];
        d[4] = s[5];
        d[5] = s[4];

        d[6] = s[10];
        d[7] = s[9];
        d[8] = s[8];

        d[9] = s[14];
        d[10] = s[13];
        d[11] = s[12];

        s += 16;
        d += 12;
        x -= 4;
    }

    while (x--) {
        d[0] = s[2];
        d[1] = s[1];
        d[2] = s[0];

        s += 4;
        d += 3;
    }
}

static void convert_fbdev(uint8_t *dst, const uint8_t *src, uint32_t width, uint32_t height, uint32_t pitch, const struct fb_var_screeninfo *var) {
    const uint32_t bytes_pp = (var->bits_per_pixel + 7U) / 8U;

    for (uint32_t y = 0; y < height; y++) {
        const uint8_t *src_row = src + ((size_t) y * pitch);
        uint8_t *dst_row = dst + ((size_t) y * width * 3U);

        for (uint32_t x = 0; x < width; x++) {
            const uint8_t *p = src_row + ((size_t) x * bytes_pp);
            uint32_t pix = 0;

            if (bytes_pp == 2) {
                pix = (uint32_t) p[0] | ((uint32_t) p[1] << 8);
            } else if (bytes_pp == 3) {
                pix = (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16);
            } else {
                pix = (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
            }

            dst_row[x * 3 + 0] = scale_bits((pix >> var->red.offset) & ((1U << var->red.length) - 1U), var->red.length);
            dst_row[x * 3 + 1] = scale_bits((pix >> var->green.offset) & ((1U << var->green.length) - 1U), var->green.length);
            dst_row[x * 3 + 2] = scale_bits((pix >> var->blue.offset) & ((1U << var->blue.length) - 1U), var->blue.length);
        }
    }
}

static void convert_drm(uint8_t *dst, const uint8_t *src, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp) {
    void (*conv)(uint8_t *, const uint8_t *, uint32_t);

    if (bpp == 32) {
        conv = convert_32b;
    } else if (bpp == 24) {
        conv = convert_24b;
    } else {
        conv = convert_16b;
    }

    for (uint32_t y = 0; y < height; y++) {
        const uint8_t *src_row = src + ((size_t) y * pitch);
        uint8_t *dst_row = dst + ((size_t) y * width * 3U);

        conv(dst_row, src_row, width);
    }
}

static int capture_fbdev_path(const char *fb_path, const char *path) {
    int fd = open(fb_path, O_RDONLY);
    if (fd < 0) return -1;

    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;

    if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0 ||
        ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0) {
        close(fd);
        return -1;
    }

    if (var.xres == 0 || var.yres == 0 || fix.line_length == 0) {
        close(fd);
        return -1;
    }

    if (var.bits_per_pixel != 16 &&
        var.bits_per_pixel != 24 &&
        var.bits_per_pixel != 32) {
        close(fd);
        return -1;
    }

    size_t map_size = (size_t) fix.line_length * var.yres;
    uint8_t *fb = mmap(NULL, map_size, PROT_READ, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) {
        close(fd);
        return -1;
    }

    size_t rgb_size = (size_t) var.xres * var.yres * 3U;
    uint8_t *rgb = malloc(rgb_size);
    if (!rgb) {
        munmap(fb, map_size);
        close(fd);
        return -1;
    }

    convert_fbdev(rgb, fb, var.xres, var.yres, fix.line_length, &var);
    int ret = png_write(path, rgb, var.xres, var.yres);

    free(rgb);
    munmap(fb, map_size);
    close(fd);

    return ret;
}

static int capture_fbdev(const char *path) {
    char fb_path[32];

    for (int i = 0; i < FBDEV_MAX; i++) {
        snprintf(fb_path, sizeof(fb_path), "/dev/fb%d", i);

        if (access(fb_path, R_OK) != 0) continue;
        if (capture_fbdev_path(fb_path, path) == 0) return 0;
    }

    return -1;
}

static int capture_drm_crtc(int fd, uint32_t crtc_id, const char *path) {
    struct drm_mode_crtc crtc;
    struct drm_mode_fb_cmd fb;
    struct drm_mode_map_dumb map;

    memset(&crtc, 0, sizeof(crtc));
    crtc.crtc_id = crtc_id;

    if (ioctl(fd, DRM_IOCTL_MODE_GETCRTC, &crtc) < 0) return -1;
    if (!crtc.mode_valid || crtc.fb_id == 0) return -1;

    memset(&fb, 0, sizeof(fb));
    fb.fb_id = crtc.fb_id;

    if (ioctl(fd, DRM_IOCTL_MODE_GETFB, &fb) < 0) return -1;
    if (fb.width == 0 || fb.height == 0 || fb.pitch == 0) return -1;
    if (fb.bpp != 16 && fb.bpp != 24 && fb.bpp != 32) return -1;

    memset(&map, 0, sizeof(map));
    map.handle = fb.handle;

    if (ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) return -1;
    if (map.offset > (uint64_t) INT64_MAX) return -1;

    size_t map_size = (size_t) fb.pitch * fb.height;
    uint8_t *mem = mmap(NULL, map_size, PROT_READ, MAP_SHARED, fd, (off_t) map.offset);
    if (mem == MAP_FAILED) return -1;

    size_t rgb_size = (size_t) fb.width * fb.height * 3U;
    uint8_t *rgb = malloc(rgb_size);
    if (!rgb) {
        munmap(mem, map_size);
        return -1;
    }

    convert_drm(rgb, mem, fb.width, fb.height, fb.pitch, fb.bpp);
    int ret = png_write(path, rgb, fb.width, fb.height);

    free(rgb);
    munmap(mem, map_size);

    if (ret != 0) unlink(path);
    return ret;
}

static int capture_drm_card(const char *card_path, const char *path) {
    int fd = open(card_path, O_RDWR);
    if (fd < 0) return -1;

    struct drm_mode_card_res res;
    uint32_t crtc_ids[DRM_MAX_CRTCS];

    memset(&res, 0, sizeof(res));
    memset(crtc_ids, 0, sizeof(crtc_ids));

    res.crtc_id_ptr = (uint64_t) (uintptr_t) crtc_ids;
    res.count_crtcs = DRM_MAX_CRTCS;

    if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) {
        close(fd);
        return -1;
    }

    if (res.count_crtcs == 0) {
        close(fd);
        return -1;
    }

    uint32_t limit = res.count_crtcs;
    if (limit > DRM_MAX_CRTCS) limit = DRM_MAX_CRTCS;

    for (uint32_t i = 0; i < limit; i++) {
        if (crtc_ids[i] == 0) continue;

        if (capture_drm_crtc(fd, crtc_ids[i], path) == 0) {
            close(fd);
            return 0;
        }
    }

    close(fd);
    return -1;
}

static int capture_drm(const char *path) {
    static const char *cards[DRM_MAX_CARDS] = {
            "/dev/dri/card0",
            "/dev/dri/card1",
            "/dev/dri/card2",
            "/dev/dri/card3"
    };

    for (size_t i = 0; i < DRM_MAX_CARDS; i++) {
        if (capture_drm_card(cards[i], path) == 0) return 0;
    }

    return -1;
}

int screenshot_save(const char *path, screenshot_mode mode) {
    if (!path || !*path) return -1;

    switch (mode) {
        case SCREENSHOT_FBDEV:
            return capture_fbdev(path);
        case SCREENSHOT_DRM:
            return capture_drm(path);
        case SCREENSHOT_AUTO:
        default:
            if (capture_fbdev(path) == 0) return 0;
            return capture_drm(path);
    }
}
